// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SERVICE_WORKER_ROUTER_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SERVICE_WORKER_ROUTER_INFO_H_

#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-blink-forward.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PLATFORM_EXPORT ServiceWorkerRouterInfo
    : public RefCounted<ServiceWorkerRouterInfo> {
 public:
  static scoped_refptr<ServiceWorkerRouterInfo> Create();

  static String GetRouterSourceTypeString(
      const network::mojom::ServiceWorkerRouterSourceType source);

  network::mojom::blink::ServiceWorkerRouterInfoPtr ToMojo() const;

  void SetRuleIdMatched(std::optional<std::uint32_t> rule_id_matched) {
    rule_id_matched_ = rule_id_matched;
  }

  const std::optional<std::uint32_t>& RuleIdMatched() const {
    return rule_id_matched_;
  }

  void SetMatchedSourceType(
      const std::optional<network::mojom::ServiceWorkerRouterSourceType>&
          type) {
    matched_source_type_ = type;
  }

  const std::optional<network::mojom::ServiceWorkerRouterSourceType>&
  MatchedSourceType() const {
    return matched_source_type_;
  }

  void SetActualSourceType(
      const std::optional<network::mojom::ServiceWorkerRouterSourceType>&
          type) {
    actual_source_type_ = type;
  }

  const std::optional<network::mojom::ServiceWorkerRouterSourceType>&
  ActualSourceType() const {
    return actual_source_type_;
  }

  std::uint64_t RouteRuleNum() const { return route_rule_num_; }

  void SetRouteRuleNum(std::uint64_t route_rule_num) {
    route_rule_num_ = route_rule_num;
  }

  const std::optional<network::mojom::ServiceWorkerStatus>&
  EvaluationWorkerStatus() const {
    return evaluation_worker_status_;
  }

  void SetEvaluationWorkerStatus(
      const std::optional<network::mojom::ServiceWorkerStatus>&
          evaluation_worker_status) {
    evaluation_worker_status_ = evaluation_worker_status;
  }

 private:
  ServiceWorkerRouterInfo();

  std::optional<uint32_t> rule_id_matched_;
  std::optional<network::mojom::ServiceWorkerRouterSourceType>
      matched_source_type_;
  std::optional<network::mojom::ServiceWorkerRouterSourceType>
      actual_source_type_;
  uint64_t route_rule_num_;
  std::optional<network::mojom::ServiceWorkerStatus> evaluation_worker_status_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SERVICE_WORKER_ROUTER_INFO_H_