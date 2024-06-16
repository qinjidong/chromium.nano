// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

// static
const UpdateChannelId& UpdateChannelId::default_id() {
  static const base::NoDestructor<UpdateChannelId> kDefaultChannelId(
      [] { return *UpdateChannelId::Create("default"); }());
  return *kDefaultChannelId;
}

// static
base::expected<UpdateChannelId, absl::monostate> UpdateChannelId::Create(
    std::string input) {
  if (input.empty() || !base::IsStringUTF8(input)) {
    return base::unexpected(absl::monostate());
  }
  return UpdateChannelId(std::move(input));
}

void PrintTo(const UpdateChannelId& id, std::ostream* os) {
  *os << id.ToString();
}

UpdateChannelId::UpdateChannelId(std::string id) : id_(std::move(id)) {}

UpdateChannelId::~UpdateChannelId() = default;

bool UpdateChannelId::operator==(const UpdateChannelId& other) const = default;
auto UpdateChannelId::operator<=>(const UpdateChannelId& other) const = default;
bool UpdateChannelId::operator<(const UpdateChannelId& other) const = default;

// static
base::expected<UpdateManifest, UpdateManifest::JsonFormatError>
UpdateManifest::CreateFromJson(const base::Value& json,
                               const GURL& update_manifest_url) {
  if (!json.is_dict()) {
    return base::unexpected(JsonFormatError::kRootNotADictionary);
  }

  const base::Value::List* versions =
      json.GetDict().FindList(kUpdateManifestAllVersionsKey);
  if (!versions) {
    return base::unexpected(JsonFormatError::kVersionsNotAnArray);
  }

  base::flat_map<base::Version, VersionEntry> version_entry_map;
  for (const auto& version_entry_value : *versions) {
    const base::Value::Dict* version_entry_dict =
        version_entry_value.GetIfDict();
    if (!version_entry_dict) {
      return base::unexpected(JsonFormatError::kVersionEntryNotADictionary);
    }

    base::expected<VersionEntry, absl::monostate> version_entry =
        VersionEntry::ParseFromJson(*version_entry_dict, update_manifest_url);
    if (!version_entry.has_value()) {
      // Each version entry must at least contain the version number and URL. If
      // a version entry cannot be parsed, it is ignored for forward
      // compatibility reasons.
      continue;
    }

    // Deliberately overwrite a potential previous entry of the same version.
    // This is for forward-compatibility, see
    // https://github.com/WICG/isolated-web-apps/blob/main/Updates.md#web-application-update-manifest
    // for more information.
    version_entry_map.insert_or_assign(version_entry->version(),
                                       *version_entry);
  }

  std::vector<VersionEntry> version_entries;
  for (auto& [version, version_entry] : version_entry_map) {
    version_entries.emplace_back(std::move(version_entry));
  }

  return UpdateManifest(std::move(version_entries));
}

UpdateManifest::UpdateManifest(std::vector<VersionEntry> version_entries)
    : version_entries_(std::move(version_entries)) {}

UpdateManifest::UpdateManifest(const UpdateManifest& other) = default;

UpdateManifest& UpdateManifest::operator=(const UpdateManifest& other) =
    default;

UpdateManifest::~UpdateManifest() = default;

std::optional<UpdateManifest::VersionEntry> UpdateManifest::GetLatestVersion(
    const UpdateChannelId& channel_id) const {
  std::optional<VersionEntry> latest_version_entry;
  for (const VersionEntry& version_entry : version_entries_) {
    if (!version_entry.channel_ids().contains(channel_id)) {
      // Ignore version entries that are not part of the provided
      // `channel_id`.
      continue;
    }
    if (!latest_version_entry.has_value() ||
        latest_version_entry->version() <= version_entry.version()) {
      latest_version_entry = version_entry;
    }
  }
  return latest_version_entry;
}

// static
base::expected<UpdateManifest::VersionEntry, absl::monostate>
UpdateManifest::VersionEntry::ParseFromJson(
    const base::Value::Dict& version_entry_dict,
    const GURL& update_manifest_url) {
  ASSIGN_OR_RETURN(auto version,
                   ParseAndValidateVersion(
                       version_entry_dict.Find(kUpdateManifestVersionKey)));
  ASSIGN_OR_RETURN(auto src, ParseAndValidateSrc(
                                 version_entry_dict.Find(kUpdateManifestSrcKey),
                                 update_manifest_url));
  ASSIGN_OR_RETURN(auto channel_ids,
                   ParseAndValidateChannels(
                       version_entry_dict.Find(kUpdateManifestChannelsKey)));
  return VersionEntry(std::move(src), std::move(version),
                      std::move(channel_ids));
}

UpdateManifest::VersionEntry::VersionEntry(
    GURL src,
    base::Version version,
    base::flat_set<UpdateChannelId> channel_ids)
    : src_(std::move(src)),
      version_(std::move(version)),
      channel_ids_(std::move(channel_ids)) {}

UpdateManifest::VersionEntry::VersionEntry(const VersionEntry& other) = default;
UpdateManifest::VersionEntry& UpdateManifest::VersionEntry::operator=(
    const VersionEntry& other) = default;

UpdateManifest::VersionEntry::~VersionEntry() = default;

GURL UpdateManifest::VersionEntry::src() const {
  CHECK(src_.is_valid());
  return src_;
}

base::Version UpdateManifest::VersionEntry::version() const {
  CHECK(version_.IsValid());
  return version_;
}

const base::flat_set<UpdateChannelId>&
UpdateManifest::VersionEntry::channel_ids() const {
  return channel_ids_;
}

// static
base::expected<base::Version, absl::monostate>
UpdateManifest::VersionEntry::ParseAndValidateVersion(
    base::optional_ref<const base::Value> version_value) {
  if (!version_value.has_value() || !version_value->is_string()) {
    return base::unexpected(absl::monostate());
  }

  base::expected<std::vector<uint32_t>, IwaVersionParseError>
      version_components =
          ParseIwaVersionIntoComponents(version_value->GetString());
  if (!version_components.has_value()) {
    return base::unexpected(absl::monostate());
  }
  base::Version version(std::move(*version_components));
  CHECK(version.IsValid());
  return version;
}

// static
base::expected<GURL, absl::monostate>
UpdateManifest::VersionEntry::ParseAndValidateSrc(
    base::optional_ref<const base::Value> src_value,
    const GURL& update_manifest_url) {
  if (!src_value.has_value() || !src_value->is_string()) {
    return base::unexpected(absl::monostate());
  }

  GURL src = update_manifest_url.Resolve(src_value->GetString());
  if (!src.is_valid() || src == update_manifest_url) {
    return base::unexpected(absl::monostate());
  }
  if (!src.SchemeIsHTTPOrHTTPS() ||
      !network::IsUrlPotentiallyTrustworthy(src)) {
    // Only https: and http: URLs are supported as the src URL. Also, they need
    // to be "potentially trustworthy", which includes https:, localhost, and
    // origins configured as trustworthy via enterprise policy. The separate
    // check for the scheme is crucial, as file:// and some other URLs are
    // "potentially trustworthy".
    return base::unexpected(absl::monostate());
  }

  return src;
}

// static
base::expected<base::flat_set<UpdateChannelId>, absl::monostate>
UpdateManifest::VersionEntry::ParseAndValidateChannels(
    base::optional_ref<const base::Value> channels_value) {
  if (!channels_value.has_value()) {
    // If the "channels" field is not present in the version entry of the Update
    // Manifest, we treat it as if it was present and contained a single
    // "default" channel.
    return base::flat_set<UpdateChannelId>{UpdateChannelId::default_id()};
  }

  if (!channels_value->is_list()) {
    return base::unexpected(absl::monostate());
  }

  std::vector<UpdateChannelId> channel_ids;
  for (const auto& channel_value : channels_value->GetList()) {
    const std::string* channel_id_string = channel_value.GetIfString();
    if (!channel_id_string) {
      return base::unexpected(absl::monostate());
    }
    auto channel_id = UpdateChannelId::Create(*channel_id_string);
    if (!channel_id.has_value()) {
      return base::unexpected(absl::monostate());
    }
    channel_ids.emplace_back(*channel_id);
  }

  return channel_ids;
}

bool operator==(const UpdateManifest::VersionEntry& lhs,
                const UpdateManifest::VersionEntry& rhs) = default;

std::ostream& operator<<(std::ostream& os,
                         const UpdateManifest::VersionEntry& version_entry) {
  base::Value::List channel_ids;
  for (const auto& channel_id : version_entry.channel_ids()) {
    channel_ids.Append(channel_id.ToString());
  }
  return os << base::Value::Dict()
                   .Set(kUpdateManifestSrcKey, version_entry.src().spec())
                   .Set(kUpdateManifestVersionKey,
                        version_entry.version().GetString())
                   .Set(kUpdateManifestChannelsKey, std::move(channel_ids));
}

}  // namespace web_app