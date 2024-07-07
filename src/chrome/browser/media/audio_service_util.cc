// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_service_util.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "content/public/common/content_features.h"

bool IsAudioServiceSandboxEnabled() {
  return true;
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40242320): Remove the kAudioProcessHighPriorityEnabled policy
// and the code enabled by this function.
bool IsAudioProcessHighPriorityEnabled() {
  return true;
}
#endif
