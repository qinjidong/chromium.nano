// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace features {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2BypassMemoryLimitCheck);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2NewLimitAndScheduler);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2AllowActivationInBackground);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2EmbedderBlockedHosts);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kPrerender2EmbedderBlockedHostsParam;

}  // namespace features

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_