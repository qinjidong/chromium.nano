// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_LOADER_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

// Callback to return the network response to the caller.
using NetworkLoaderCallback =
    base::OnceCallback<void(std::unique_ptr<std::string>)>;

namespace data_sharing {

// Class for fetching data sharing related data from network.
class DataSharingNetworkLoader {
 public:
  DataSharingNetworkLoader() = default;
  virtual ~DataSharingNetworkLoader() = default;

  // Disallow copy/assign.
  DataSharingNetworkLoader(const DataSharingNetworkLoader&) = delete;
  DataSharingNetworkLoader& operator=(const DataSharingNetworkLoader&) = delete;

  // Called to fetch data from the network. Callback will be invoked once the
  // fetch completes. If an error occurs, a nullptr will be passed to the
  // callback.
  virtual void LoadUrl(const GURL& gurl,
                       const std::vector<std::string>& scopes,
                       const std::string& post_data,
                       const net::NetworkTrafficAnnotationTag& annotation_tag,
                       NetworkLoaderCallback callback) = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_DATA_SHARING_NETWORK_LOADER_H_