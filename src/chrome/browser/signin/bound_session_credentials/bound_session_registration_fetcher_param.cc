// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "net/base/schemeful_site.h"
#include "net/http/structured_headers.h"

namespace {
constexpr char kAlgoItemKey[] = "supported-alg";
constexpr char kRegistrationHeaderName[] = "Sec-Session-Google-Registration";
constexpr char kRegistrationListHeaderName[] =
    "Sec-Session-Google-Registration-List";
constexpr char kRegistrationItemKey[] = "registration";
constexpr char kChallengeItemKey[] = "challenge";

std::optional<crypto::SignatureVerifier::SignatureAlgorithm> AlgoFromString(
    const std::string algo) {
  if (base::EqualsCaseInsensitiveASCII(algo, "ES256")) {
    return crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  if (base::EqualsCaseInsensitiveASCII(algo, "RS256")) {
    return crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  return std::nullopt;
}
}  // namespace

// A temporary feature to gate the new list header support until its format is
// finalized.
BASE_FEATURE(kBoundSessionRegistrationListHeaderSupport,
             "BoundSessionRegistrationListHeaderSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BoundSessionRegistrationFetcherParam::BoundSessionRegistrationFetcherParam(
    BoundSessionRegistrationFetcherParam&& other) noexcept = default;

BoundSessionRegistrationFetcherParam&
BoundSessionRegistrationFetcherParam::operator=(
    BoundSessionRegistrationFetcherParam&& other) noexcept = default;

BoundSessionRegistrationFetcherParam::~BoundSessionRegistrationFetcherParam() =
    default;

BoundSessionRegistrationFetcherParam::BoundSessionRegistrationFetcherParam(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::string challenge)
    : registration_endpoint_(std::move(registration_endpoint)),
      supported_algos_(std::move(supported_algos)),
      challenge_(std::move(challenge)) {}

// static
std::vector<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::CreateFromHeaders(
    const GURL& request_url,
    const net::HttpResponseHeaders* headers) {
  if (!request_url.is_valid() || !headers) {
    return {};
  }

  // First, try the new header format (gated behind a feature flag).
  if (base::FeatureList::IsEnabled(
          kBoundSessionRegistrationListHeaderSupport)) {
    std::string list_header_value;
    if (headers->GetNormalizedHeader(kRegistrationListHeaderName,
                                     &list_header_value)) {
      return MaybeCreateFromListHeader(request_url, list_header_value);
    }
  }

  // Only if the new format header is missing, try the legacy format.
  std::string legacy_header_value;
  if (headers->GetNormalizedHeader(kRegistrationHeaderName,
                                   &legacy_header_value)) {
    return MaybeCreateFromLegacyHeader(request_url, legacy_header_value);
  }

  // Return an empty result if none of the headers are present.
  return {};
}

// static
BoundSessionRegistrationFetcherParam
BoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
    GURL registration_endpoint,
    std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos,
    std::string challenge) {
  return BoundSessionRegistrationFetcherParam(std::move(registration_endpoint),
                                              std::move(supported_algos),
                                              std::move(challenge));
}

// static
std::optional<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::ParseListItem(
    const GURL& request_url,
    const net::structured_headers::Item& item,
    const net::structured_headers::Parameters& params) {
  if (!item.is_string()) {
    return std::nullopt;
  }

  GURL registration_endpoint = bound_session_credentials::ResolveEndpointPath(
      request_url, item.GetString());
  if (!registration_endpoint.is_valid()) {
    return std::nullopt;
  }

  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  std::string challenge;
  for (const auto& [name, value] : params) {
    // The only boolean parameters are the supported algorithms.
    if (value.is_boolean() && value.GetBoolean()) {
      std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algo =
          AlgoFromString(name);
      if (algo) {
        supported_algos.push_back(*algo);
      }
    }

    // The only string parameter is the challenge.
    if (value.is_string() && name == kChallengeItemKey) {
      challenge = value.GetString();
    }
  }

  if (challenge.empty() || supported_algos.empty()) {
    return {};
  }

  return BoundSessionRegistrationFetcherParam(std::move(registration_endpoint),
                                              std::move(supported_algos),
                                              std::move(challenge));
}

// static
std::vector<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::MaybeCreateFromListHeader(
    const GURL& request_url,
    std::string_view header_value) {
  std::optional<net::structured_headers::List> list =
      net::structured_headers::ParseList(header_value);
  if (!list || list->empty()) {
    return {};
  }

  std::vector<BoundSessionRegistrationFetcherParam> params;
  for (const auto& item : *list) {
    // Header spec does not support inner lists.
    if (item.member_is_inner_list) {
      continue;
    }

    // This is not obvious, the way the structured header parser works these
    // params will be considered to belong to the list item and not to the item
    // itself. This is to enable the more intuitive syntax:
    // "path1"; challenge=:Y2hhbGxlbmdl:; es256;
    // instead of:
    // ("path1"; challenge=:Y2hhbGxlbmdl:; es256;)
    std::optional<BoundSessionRegistrationFetcherParam> param =
        ParseListItem(request_url, item.member[0].item, item.params);
    if (param) {
      params.push_back(std::move(param).value());
    }
  }

  return params;
}

// static
std::vector<BoundSessionRegistrationFetcherParam>
BoundSessionRegistrationFetcherParam::MaybeCreateFromLegacyHeader(
    const GURL& request_url,
    std::string_view header_value) {
  GURL registration_endpoint;
  std::vector<crypto::SignatureVerifier::SignatureAlgorithm> supported_algos;
  std::string challenge;
  base::StringPairs items;
  base::SplitStringIntoKeyValuePairs(header_value, '=', ';', &items);
  for (const auto& [key, value] : items) {
    if (base::EqualsCaseInsensitiveASCII(key, kRegistrationItemKey)) {
      GURL potential_registration_endpoint =
          bound_session_credentials::ResolveEndpointPath(request_url, value);
      if (potential_registration_endpoint.is_valid()) {
        registration_endpoint = potential_registration_endpoint;
      }
    }

    if (base::EqualsCaseInsensitiveASCII(key, kAlgoItemKey)) {
      auto list = base::SplitString(value, ",",
                                    base::WhitespaceHandling::TRIM_WHITESPACE,
                                    base::SplitResult::SPLIT_WANT_NONEMPTY);
      for (const auto& alg_string : list) {
        std::optional<crypto::SignatureVerifier::SignatureAlgorithm> alg =
            AlgoFromString(alg_string);
        if (alg.has_value()) {
          supported_algos.push_back(alg.value());
        }
      }
    }

    if (base::EqualsCaseInsensitiveASCII(key, kChallengeItemKey)) {
      // Challenge will be eventually written into a `base::Value`, when
      // generating the registration token, which is restricted to UTF8.
      if (!base::IsStringUTF8AllowingNoncharacters(value)) {
        return {};
      }
      challenge = value;
    }
  }

  if (registration_endpoint.is_valid() && !supported_algos.empty() &&
      !challenge.empty()) {
    std::vector<BoundSessionRegistrationFetcherParam> result;
    result.push_back(BoundSessionRegistrationFetcherParam(
        std::move(registration_endpoint), std::move(supported_algos),
        std::move(challenge)));
    return result;
  } else {
    return {};
  }
}