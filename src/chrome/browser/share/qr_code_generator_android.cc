// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/QRCodeGenerator_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> JNI_QRCodeGenerator_GenerateBitmap(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_data_string) {
  ScopedJavaLocalRef<jobject> java_bitmap;
  return java_bitmap;
}
