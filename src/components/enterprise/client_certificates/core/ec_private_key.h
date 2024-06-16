// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_EC_PRIVATE_KEY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_EC_PRIVATE_KEY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "components/enterprise/client_certificates/core/private_key.h"

namespace crypto {
class ECPrivateKey;
}  // namespace crypto

namespace client_certificates {

class ECPrivateKey : public PrivateKey {
 public:
  explicit ECPrivateKey(std::unique_ptr<crypto::ECPrivateKey> key);

  // PrivateKey:
  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) const override;
  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override;
  crypto::SignatureVerifier::SignatureAlgorithm GetAlgorithm() const override;
  client_certificates_pb::PrivateKey ToProto() const override;

 private:
  friend class base::RefCountedThreadSafe<ECPrivateKey>;

  ~ECPrivateKey() override;

  std::unique_ptr<crypto::ECPrivateKey> key_;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_EC_PRIVATE_KEY_H_