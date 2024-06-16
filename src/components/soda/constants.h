// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SODA_CONSTANTS_H_
#define COMPONENTS_SODA_CONSTANTS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "components/soda/pref_names.h"
#include "components/strings/grit/components_strings.h"

namespace speech {

extern const char kUsEnglishLocale[];
extern const char kEnglishLocaleNoCountry[];

// Metrics names for keeping track of SODA installation.
extern const char kSodaBinaryInstallationResult[];
extern const char kSodaBinaryInstallationSuccessTimeTaken[];
extern const char kSodaBinaryInstallationFailureTimeTaken[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LanguageCode {
  kNone = 0,
  kEnUs = 1,
  kJaJp = 2,
  kDeDe = 3,
  kEsEs = 4,
  kFrFr = 5,
  kItIt = 6,
  kEnCa = 7,
  kEnAu = 8,
  kEnGb = 9,
  kEnIe = 10,
  kEnSg = 11,
  kFrBe = 12,
  kFrCh = 13,
  kEnIn = 14,
  kItCh = 15,
  kDeAt = 16,
  kDeBe = 17,
  kDeCh = 18,
  kEsUs = 19,
  kHiIn = 20,
  kPtBr = 21,
  kIdId = 22,
  kKoKr = 23,
  kPlPl = 24,
  kThTh = 25,
  kTrTr = 26,
  kZhCn = 27,
  kZhTw = 28,
  // TODO(evliu): Add Chrome LC Support for languages below.
  kDaDk = 29,
  kFrCa = 30,
  kNbNo = 31,
  kNlNl = 32,
  kSvSe = 33,
  kRuRu = 34,
  kViVn = 35,
  kMaxValue = kViVn,
};

// Describes all metadata needed to dynamically install SODA language pack
// components.
struct SodaLanguagePackComponentConfig {
  // The language code of the language pack component.
  LanguageCode language_code = LanguageCode::kNone;

  // The language name for the language component (e.g. "en-US").
  const char* language_name = nullptr;

  // The name of the config file path pref for the language pack.
  const char* config_path_pref = nullptr;

  // The SHA256 of the SubjectPublicKeyInfo used to sign the language pack
  // component.
  const uint8_t public_key_sha[32] = {};
};

constexpr SodaLanguagePackComponentConfig kLanguageComponentConfigs[] = {
    {LanguageCode::kEnUs,
     "en-US",
     prefs::kSodaEnUsConfigPath,
     {0xe4, 0x64, 0x1c, 0xc2, 0x8c, 0x2a, 0x97, 0xa7, 0x16, 0x61, 0xbd,
      0xa9, 0xbe, 0xe6, 0x93, 0x56, 0xf5, 0x05, 0x33, 0x9b, 0x8b, 0x0b,
      0x02, 0xe2, 0x6b, 0x7e, 0x6c, 0x40, 0xa1, 0xd2, 0x7e, 0x18}},
    {LanguageCode::kDeDe,
     "de-DE",
     prefs::kSodaDeDeConfigPath,
     {0x92, 0xb6, 0xd8, 0xa3, 0x0b, 0x09, 0xce, 0x21, 0xdb, 0x68, 0x48,
      0x15, 0xcb, 0x49, 0xd7, 0xc6, 0x21, 0x3f, 0xe5, 0x96, 0x10, 0x97,
      0x6e, 0x0f, 0x08, 0x31, 0xec, 0xe4, 0x7f, 0xed, 0xef, 0x3d}},
    {LanguageCode::kEsEs,
     "es-ES",
     prefs::kSodaEsEsConfigPath,
     {0x9a, 0x22, 0xac, 0x04, 0x97, 0xc1, 0x70, 0x61, 0x24, 0x1f, 0x49,
      0x18, 0x72, 0xd8, 0x67, 0x31, 0x72, 0x7a, 0xf9, 0x77, 0x04, 0xf0,
      0x17, 0xb5, 0xfe, 0x88, 0xac, 0x60, 0xdd, 0x8a, 0x67, 0xdd}},
    {LanguageCode::kFrFr,
     "fr-FR",
     prefs::kSodaFrFrConfigPath,
     {0x6e, 0x0e, 0x2b, 0xd3, 0xc6, 0xe5, 0x1b, 0x5e, 0xfa, 0xef, 0x42,
      0x3f, 0x57, 0xb9, 0x2b, 0x13, 0x56, 0x47, 0x58, 0xdb, 0x76, 0x89,
      0x71, 0xeb, 0x1f, 0xed, 0x48, 0x6c, 0xac, 0xd5, 0x31, 0xa0}},
    {LanguageCode::kItIt,
     "it-IT",
     prefs::kSodaItItConfigPath,
     {0x97, 0x45, 0xd7, 0xbc, 0xf0, 0x61, 0x24, 0xb3, 0x0e, 0x13, 0xf2,
      0x97, 0xaa, 0xd5, 0x9e, 0x78, 0xa5, 0x81, 0x35, 0x75, 0xb5, 0x9d,
      0x3b, 0xbb, 0xde, 0xba, 0x0e, 0xf7, 0xf0, 0x48, 0x56, 0x01}},
    {LanguageCode::kJaJp,
     "ja-JP",
     prefs::kSodaJaJpConfigPath,
     {0xed, 0x7f, 0x96, 0xa5, 0x60, 0x9c, 0xaa, 0x4d, 0x80, 0xe5, 0xb8,
      0x26, 0xea, 0xf0, 0x41, 0x50, 0x09, 0x52, 0xa4, 0xb3, 0x1e, 0x6a,
      0x8e, 0x24, 0x99, 0xde, 0x51, 0x14, 0xc4, 0x3c, 0xfa, 0x48}},
    {LanguageCode::kHiIn,
     "hi-IN",
     prefs::kSodaHiInConfigPath,
     {0x0e, 0xb6, 0x04, 0xa8, 0x86, 0xb5, 0x7d, 0x96, 0x06, 0x3b, 0x94,
      0x8b, 0x64, 0x6f, 0x54, 0x2d, 0x75, 0x34, 0x87, 0xcf, 0xaf, 0x19,
      0x4c, 0x76, 0xb7, 0xf5, 0xe0, 0x7d, 0x21, 0x0a, 0xca, 0x88}},
    {LanguageCode::kPtBr,
     "pt-BR",
     prefs::kSodaPtBrConfigPath,
     {0x0d, 0x52, 0xe0, 0x31, 0xbc, 0x9f, 0xbc, 0xa6, 0xc0, 0x7b, 0xa7,
      0x6d, 0xd6, 0xa7, 0xe1, 0x4d, 0x97, 0xaa, 0x2e, 0x2b, 0x51, 0x9a,
      0xfa, 0x0f, 0x17, 0x04, 0x52, 0x04, 0xfc, 0xa9, 0x07, 0xd4}},
    {LanguageCode::kIdId,
     "id-ID",
     prefs::kSodaIdIdConfigPath,
     {0xa2, 0x60, 0xdb, 0xd4, 0xdf, 0x1b, 0xaa, 0x71, 0xdf, 0xf5, 0x86,
      0x8e, 0x27, 0xe4, 0x36, 0xd7, 0xc1, 0x25, 0xfe, 0x0d, 0x48, 0xba,
      0x3b, 0x28, 0x24, 0x60, 0x9d, 0x2a, 0xc1, 0x6f, 0xa0, 0x5c}},
    {LanguageCode::kKoKr,
     "ko-KR",
     prefs::kSodaKoKrConfigPath,
     {0x2e, 0x69, 0x38, 0x62, 0xd2, 0x77, 0xab, 0x59, 0xa7, 0x0b, 0x67,
      0x75, 0x41, 0x9f, 0x39, 0x71, 0x22, 0x95, 0x7c, 0x25, 0xbd, 0xea,
      0x8a, 0x19, 0x92, 0xa0, 0xa6, 0x81, 0x93, 0xa0, 0x11, 0x45}},
    {LanguageCode::kPlPl,
     "pl-PL",
     prefs::kSodaPlPlConfigPath,
     {0x3f, 0xa6, 0xc9, 0x44, 0x13, 0x6a, 0xbe, 0xb8, 0xc2, 0xd2, 0x30,
      0xa0, 0x71, 0x0e, 0xab, 0xfc, 0xfc, 0x5d, 0x74, 0x62, 0xd8, 0x50,
      0x67, 0x90, 0x61, 0x81, 0x21, 0xd9, 0xf8, 0x74, 0x6d, 0x1c}},
    {LanguageCode::kRuRu,
     "ru-RU",
     prefs::kSodaRuRuConfigPath,
     {0xd9, 0xcd, 0x1f, 0x88, 0xbe, 0xa7, 0xe1, 0xe5, 0x62, 0x8b, 0xe1,
      0x3c, 0xf6, 0x3a, 0x54, 0x5e, 0x38, 0xb4, 0x79, 0x2b, 0x56, 0xc5,
      0x61, 0x3b, 0xc1, 0x97, 0xb6, 0x91, 0x16, 0x98, 0xa3, 0xb6}},
    {LanguageCode::kThTh,
     "th-TH",
     prefs::kSodaThThConfigPath,
     {0xeb, 0x2e, 0xa1, 0x98, 0xba, 0x0a, 0x10, 0xad, 0x09, 0x15, 0xea,
      0x1e, 0x84, 0x8f, 0x1b, 0x4b, 0x7d, 0xfb, 0xfa, 0x33, 0x92, 0x4c,
      0x34, 0xe2, 0x5b, 0x33, 0x0c, 0x4a, 0x35, 0x79, 0xfe, 0xa4}},
    {LanguageCode::kTrTr,
     "tr-TR",
     prefs::kSodaTrTrConfigPath,
     {0x85, 0x02, 0x59, 0xd8, 0x1f, 0xba, 0x8d, 0x23, 0x42, 0xfa, 0xa4,
      0x3c, 0x4a, 0x65, 0x4e, 0x09, 0xfa, 0xb0, 0x22, 0x2d, 0x31, 0x85,
      0xdb, 0xb2, 0x04, 0x42, 0x22, 0x6f, 0x4b, 0x86, 0xb9, 0xff}},
    {LanguageCode::kViVn,
     "vi-VN",
     prefs::kSodaViVnConfigPath,
     {0x1f, 0xd2, 0xae, 0x98, 0x67, 0x68, 0x1b, 0x4b, 0x31, 0xe5, 0x39,
      0xc5, 0x38, 0xf5, 0x48, 0x0a, 0xa8, 0xb6, 0xf6, 0xcc, 0x2f, 0x7d,
      0xe8, 0x49, 0x35, 0x5e, 0xa8, 0xb9, 0x13, 0x9f, 0x32, 0x3c}},
    {LanguageCode::kZhCn,
     "cmn-Hans-CN",
     prefs::kSodaZhCnConfigPath,
     {0xfb, 0x55, 0xfa, 0xa7, 0x38, 0x1d, 0xf5, 0x5d, 0x29, 0xf5, 0xbd,
      0xeb, 0x5b, 0xaf, 0x69, 0xde, 0x6c, 0xcc, 0x98, 0xa5, 0x7d, 0x66,
      0x29, 0x34, 0xbf, 0xce, 0x8e, 0xde, 0x24, 0x5b, 0x30, 0x79}},
    {LanguageCode::kZhTw,
     "cmn-Hant-TW",
     prefs::kSodaZhTwConfigPath,
     {0x91, 0xb3, 0x38, 0x63, 0x94, 0x27, 0xd8, 0xd9, 0xb1, 0xa9, 0x94,
      0x80, 0x35, 0x6c, 0xbf, 0xe2, 0x7d, 0xf3, 0x1a, 0xc8, 0x8a, 0x13,
      0xcf, 0x95, 0x0a, 0x15, 0x3a, 0xb1, 0x1c, 0x19, 0xbc, 0xbe}},
};

// Location of the libsoda binary within the SODA installation directory.
extern const base::FilePath::CharType kSodaBinaryRelativePath[];

// Name of the of the libsoda binary used in browser tests.
extern const base::FilePath::CharType kSodaTestBinaryRelativePath[];

// Location of the SODA component relative to the components directory.
extern const base::FilePath::CharType kSodaInstallationRelativePath[];

// Location of the SODA language packs relative to the components
// directory.
extern const base::FilePath::CharType kSodaLanguagePacksRelativePath[];

// Location of the SODA files used in browser tests.
extern const base::FilePath::CharType kSodaTestResourcesRelativePath[];

// Location of the SODA models directory relative to the language pack
// installation directory.
extern const base::FilePath::CharType kSodaLanguagePackDirectoryRelativePath[];

// Get the absolute path of the SODA component directory.
const base::FilePath GetSodaDirectory();

// Get the absolute path of the SODA directory containing the language packs.
const base::FilePath GetSodaLanguagePacksDirectory();

// Get the absolute path of the SODA directory containing the language packs
// used in browser tests.
const base::FilePath GetSodaTestResourcesDirectory();

// Get the absolute path of the latest SODA language pack for a given language
// (e.g. en-US).
const base::FilePath GetLatestSodaLanguagePackDirectory(
    const std::string& language);

// Get the directory containing the latest version of SODA. In most cases
// there will only be one version of SODA, but it is possible for there to be
// multiple versions if a newer version of SODA was recently downloaded before
// the old version gets cleaned up. Returns an empty path if SODA is not
// installed.
const base::FilePath GetLatestSodaDirectory();

// Get the path to the SODA binary. Returns an empty path if SODA is not
// installed.
const base::FilePath GetSodaBinaryPath();

// Get the path to the SODA binary used in browser tests. Returns an empty path
// if SODA is not installed.
const base::FilePath GetSodaTestBinaryPath();

std::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    LanguageCode language_code);

std::optional<SodaLanguagePackComponentConfig> GetLanguageComponentConfig(
    const std::string& language_name);

// Get the language component config matching a given language subtag. For
// example, the "fr-CA" language name will return the language component config
// for "fr-FR".
std::optional<SodaLanguagePackComponentConfig>
GetLanguageComponentConfigMatchingLanguageSubtag(
    const std::string& language_name);

LanguageCode GetLanguageCodeByComponentId(const std::string& component_id);

std::string GetLanguageName(LanguageCode language_code);

LanguageCode GetLanguageCode(const std::string& language_name);

const std::u16string GetLanguageDisplayName(const std::string& language_name,
                                            const std::string& display_locale);

// Returns the `SodaInstaller.Language.{language}.InstallationSuccessTime` uma
// metric string for the language code.
const std::string GetInstallationSuccessTimeMetricForLanguagePack(
    const LanguageCode& language_code);
const std::string GetInstallationSuccessTimeMetricForLanguage(
    const std::string& language);

// Returns the `SodaInstaller.Language.{language}.InstallationFailureTime` uma
// metric string for the language code.
const std::string GetInstallationFailureTimeMetricForLanguagePack(
    const LanguageCode& language_code);
const std::string GetInstallationFailureTimeMetricForLanguage(
    const std::string& language);

// Returns the `SodaInstaller.Language.{language}.InstallationResult` uma
// metric string for the language code..
const std::string GetInstallationResultMetricForLanguagePack(
    const LanguageCode& language_code);
const std::string GetInstallationResultMetricForLanguage(
    const std::string& language);

// Gets a list of locales enabled by the Finch flag.
std::vector<std::string> GetLiveCaptionEnabledLanguages();
}  // namespace speech

#endif  // COMPONENTS_SODA_CONSTANTS_H_