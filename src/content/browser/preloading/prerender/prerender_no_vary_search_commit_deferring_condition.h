// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_COMMIT_DEFERRING_CONDITION_H_

#include <memory>
#include <optional>

#include "content/browser/renderer_host/navigation_type.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {

class NavigationRequest;

class PrerenderNoVarySearchCommitDeferringCondition
    : public CommitDeferringCondition {
 public:
  ~PrerenderNoVarySearchCommitDeferringCondition() override;
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request,
      NavigationType navigation_type,
      std::optional<int> candidate_prerender_frame_tree_node_id);
  Result WillCommitNavigation(base::OnceClosure resume) override;

 private:
  PrerenderNoVarySearchCommitDeferringCondition(
      NavigationRequest& navigation_request,
      int candidate_prerender_frame_tree_node_id);
  const int candidate_prerender_frame_tree_node_id_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_COMMIT_DEFERRING_CONDITION_H_