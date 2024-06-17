// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_cluster_manager.h"

#include "base/task/sequenced_task_runner.h"

namespace commerce {

MockClusterManager::MockClusterManager(
    ProductSpecificationsService* product_specifications_service)
    : ClusterManager(
          product_specifications_service,
          base::RepeatingCallback<void(const GURL&, ProductInfoCallback)>(),
          base::RepeatingCallback<const std::vector<UrlInfo>()>()) {}

MockClusterManager::~MockClusterManager() = default;

void MockClusterManager::SetResponseForGetEntryPointInfoForSelection(
    std::optional<EntryPointInfo> entry_point_info) {
  ON_CALL(*this, GetEntryPointInfoForSelection)
      .WillByDefault([entry_point_info = std::move(entry_point_info)](
                         const GURL& old_url, const GURL& new_url,
                         ClusterManager::GetEntryPointInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), std::move(entry_point_info)));
      });
}

void MockClusterManager::SetResponseForGetEntryPointInfoForNavigation(
    std::optional<EntryPointInfo> entry_point_info) {
  ON_CALL(*this, GetEntryPointInfoForNavigation)
      .WillByDefault([entry_point_info = std::move(entry_point_info)](
                         const GURL& url,
                         ClusterManager::GetEntryPointInfoCallback callback) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback), std::move(entry_point_info)));
      });
}

void MockClusterManager::SetResponseForGetProductGroupForCandidateProduct(
    std::optional<ProductGroup> product_group) {
  ON_CALL(*this, GetProductGroupForCandidateProduct)
      .WillByDefault(testing::Return(product_group));
}
}  // namespace commerce
