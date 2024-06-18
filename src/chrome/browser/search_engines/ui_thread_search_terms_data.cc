// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"

#include "base/check.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/google/core/common/google_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

using content::BrowserThread;

UIThreadSearchTermsData::UIThreadSearchTermsData() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
}

std::string UIThreadSearchTermsData::GoogleBaseURLValue() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  GURL base_url(google_util::CommandLineGoogleBaseURL());
  if (base_url.is_valid())
    return base_url.spec();

  return SearchTermsData::GoogleBaseURLValue();
}

std::string UIThreadSearchTermsData::GetApplicationLocale() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_browser_process->GetApplicationLocale();
}

// Android implementations are in ui_thread_search_terms_data_android.cc.
#if !BUILDFLAG(IS_ANDROID)
std::u16string UIThreadSearchTermsData::GetRlzParameterValue(
    bool from_app_list) const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::u16string rlz_string;
  return rlz_string;
}

// We can enable this on non-Android if other platforms ever want a non-empty
// search client string.  There is already a unit test in place for Android
// called TemplateURLTest::SearchClient.
std::string UIThreadSearchTermsData::GetSearchClient() const {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
      BrowserThread::CurrentlyOn(BrowserThread::UI));
  return std::string();
}
#endif

// It's acutally OK to call this method on any thread, but it's currently placed
// in UIThreadSearchTermsData since SearchTermsData cannot depend on src/chrome
// as it is shared with iOS.
std::string UIThreadSearchTermsData::GoogleImageSearchSource() const {
  // Do not distinguish extended from regular stable in image search queries.
  const std::string channel_name =
      chrome::GetChannelName(chrome::WithExtendedStable(false));
  return base::StrCat({version_info::GetProductName(), " ",
                       version_info::GetVersionNumber(),
                       version_info::IsOfficialBuild() ? " (Official) " : " ",
                       version_info::GetOSType(),
                       channel_name.empty() ? "" : " ", channel_name});
}

size_t UIThreadSearchTermsData::EstimateMemoryUsage() const {
  return 0;
}
