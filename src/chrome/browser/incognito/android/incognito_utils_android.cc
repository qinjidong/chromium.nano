// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/incognito/jni_headers/IncognitoUtils_jni.h"

static jboolean JNI_IncognitoUtils_GetIncognitoModeEnabled(JNIEnv* env,
                                                           Profile* profile) {
  return true;
}

static jboolean JNI_IncognitoUtils_GetIncognitoModeManaged(JNIEnv* env,
                                                           Profile* profile) {
  return false;
}
