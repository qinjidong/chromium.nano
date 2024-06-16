// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/android/jni_headers/PhoneNumberUtil_jni.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/geo/autofill_country.h"

namespace autofill {

namespace {

using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::ScopedJavaLocalRef;

}  // namespace

// Formats the given number |jphone_number| for the given country
// |jcountry_code| to
// i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL format
// by using i18n::phonenumbers::PhoneNumberUtil::Format.
ScopedJavaLocalRef<jstring> JNI_PhoneNumberUtil_FormatForDisplay(
    JNIEnv* env,
    const JavaParamRef<jstring>& jphone_number,
    const JavaParamRef<jstring>& jcountry_code) {
  return nullptr;
}

// Formats the given number |jphone_number| to
// i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::E164 format by using
// i18n::phonenumbers::PhoneNumberUtil::Format , as defined in the Payment
// Request spec
// (https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm)
ScopedJavaLocalRef<jstring> JNI_PhoneNumberUtil_FormatForResponse(
    JNIEnv* env,
    const JavaParamRef<jstring>& jphone_number) {
  return nullptr;
}

// Checks whether the given number |jphone_number| is a possible number for a
// given country |jcountry_code| by using
// i18n::phonenumbers::PhoneNumberUtil::IsPossibleNumberForString.
jboolean JNI_PhoneNumberUtil_IsPossibleNumber(
    JNIEnv* env,
    const JavaParamRef<jstring>& jphone_number,
    const JavaParamRef<jstring>& jcountry_code) {
  return false;
}

}  // namespace autofill
