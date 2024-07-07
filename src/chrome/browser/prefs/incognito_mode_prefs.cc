// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/parental_controls.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/partner_browser_customizations.h"
#endif  // BUILDFLAG(IS_ANDROID)

// static
void IncognitoModePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kIncognitoReauthenticationForAndroid,
                                false);
#endif
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  return ShouldLaunchIncognitoInternal(command_line, prefs, false);
}

// static
bool IncognitoModePrefs::ShouldOpenSubsequentBrowsersInIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  return ShouldLaunchIncognitoInternal(command_line, prefs, true);
}

// static
bool IncognitoModePrefs::CanOpenBrowser(Profile* profile) {
  return true;
}

// static
bool IncognitoModePrefs::IsIncognitoAllowed(Profile* profile) {
  return !profile->IsGuestSession() ;
}

// static
bool IncognitoModePrefs::ArePlatformParentalControlsEnabled() {
#if BUILDFLAG(IS_WIN)
  return GetWinParentalControls().logging_required;
#elif BUILDFLAG(IS_ANDROID)
  return chrome::android::PartnerBrowserCustomizations::IsIncognitoDisabled();
#else
  return false;
#endif
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognitoInternal(
    const base::CommandLine& command_line,
    const PrefService* prefs,
    const bool for_subsequent_browsers) {
  return false;
}
