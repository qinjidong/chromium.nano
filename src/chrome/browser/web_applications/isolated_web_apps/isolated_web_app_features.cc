// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"

namespace web_app {

bool IsIwaDevModeEnabled(Profile* profile) {
  return false;
}

bool IsIwaUnmanagedInstallEnabled(Profile* profile) {
  return false;
}

}  // namespace web_app
