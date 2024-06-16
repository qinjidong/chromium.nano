// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/language_detection/language_detection_model.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/utf_string_conversions.h"
#include "components/language/core/common/language_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_util.h"

namespace translate {

LanguageDetectionModel::LanguageDetectionModel() = default;

LanguageDetectionModel::~LanguageDetectionModel() = default;

void LanguageDetectionModel::UpdateWithFile(base::File model_file) {}

bool LanguageDetectionModel::IsAvailable() const {
  return false;
}

std::string LanguageDetectionModel::DeterminePageLanguage(
    const std::string& code,
    const std::string& html_lang,
    const std::u16string& contents,
    std::string* predicted_language,
    bool* is_prediction_reliable,
    float& prediction_reliability_score) const {
  return translate::kUnknownLanguageCode;
}

LanguageDetectionModel::Prediction LanguageDetectionModel::DetectLanguage(
    const std::u16string& contents) const {
  return Prediction{translate::kUnknownLanguageCode, 0.0f};
}

std::string LanguageDetectionModel::GetModelVersion() const {
  // TODO(crbug.com/40748826): Return the model version provided
  // by the model itself.
  return "TFLite_v1";
}

}  // namespace translate
