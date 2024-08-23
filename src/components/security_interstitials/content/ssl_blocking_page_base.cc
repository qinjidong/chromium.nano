// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_blocking_page_base.h"

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/controller_client.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

SSLBlockingPageBase::SSLBlockingPageBase(
    content::WebContents* web_contents,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    const base::Time& time_triggered,
    bool can_show_enhanced_protection_message,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      can_show_enhanced_protection_message_(
          can_show_enhanced_protection_message) {}

SSLBlockingPageBase::~SSLBlockingPageBase() = default;

void SSLBlockingPageBase::OnInterstitialClosing() {}

bool SSLBlockingPageBase::ShouldShowEnhancedProtectionMessage() {
  // Only show the enhanced protection message if all the following are true:
  // |can_show_enhanced_protection_message_| is set to true AND
  // the window is not incognito AND
  // Safe Browsing is not managed by policy AND
  // the user is not already in enhanced protection mode.
  if (!can_show_enhanced_protection_message_) {
    return false;
  }

  const bool in_incognito =
      web_contents()->GetBrowserContext()->IsOffTheRecord();

  if (in_incognito) {
    return false;
  }
  return true;
}

void SSLBlockingPageBase::PopulateEnhancedProtectionMessage(
    base::Value::Dict& load_time_data) {
  const bool show = ShouldShowEnhancedProtectionMessage();

  load_time_data.Set(security_interstitials::kDisplayEnhancedProtectionMessage,
                     show);
  // This needs to be set even if it's not shown.
  load_time_data.Set(
      security_interstitials::kEnhancedProtectionMessage,
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  // Disable Extended Reporting checkbox.
  load_time_data.Set(security_interstitials::kDisplayCheckBox, false);

  if (!show) {
    return;
  }

  if (controller()->metrics_helper()) {
    controller()->metrics_helper()->RecordUserInteraction(
        security_interstitials::MetricsHelper::SHOW_ENHANCED_PROTECTION);
  }
}
