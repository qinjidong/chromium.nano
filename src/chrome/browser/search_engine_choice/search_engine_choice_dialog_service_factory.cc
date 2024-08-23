// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

namespace {
// Stores whether this is a Google Chrome-branded build.
bool g_is_chrome_build =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    true;
#else
    false;
#endif

}  // namespace

SearchEngineChoiceDialogServiceFactory::SearchEngineChoiceDialogServiceFactory()
    : ProfileKeyedServiceFactory(
          "SearchEngineChoiceDialogServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithAshInternals(ProfileSelection::kNone)
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(search_engines::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchEngineChoiceDialogServiceFactory::
    ~SearchEngineChoiceDialogServiceFactory() = default;

// static
SearchEngineChoiceDialogServiceFactory*
SearchEngineChoiceDialogServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceDialogServiceFactory> factory;
  return factory.get();
}

// static
SearchEngineChoiceDialogService*
SearchEngineChoiceDialogServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SearchEngineChoiceDialogService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
base::AutoReset<bool>
SearchEngineChoiceDialogServiceFactory::ScopedChromeBuildOverrideForTesting(
    bool force_chrome_build) {
  CHECK_IS_TEST();
  return base::AutoReset<bool>(&g_is_chrome_build, force_chrome_build);
}

std::unique_ptr<KeyedService>
SearchEngineChoiceDialogServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return nullptr;
}
