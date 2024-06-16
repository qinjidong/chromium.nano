// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_ACTION_H_
#define COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_ACTION_H_

namespace page_info {

// Histogram name for when an action happens in Page Info used in all platforms.
extern const char kWebsiteSettingsActionHistogram[];

// UMA statistics for PageInfo. Do not reorder or remove existing
// fields. A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.page_info
// All values here should have corresponding entries in
// WebsiteSettingsAction area of enums.xml.
enum PageInfoAction {
  PAGE_INFO_OPENED = 0,
  // No longer used; indicated actions for the old version of Page Info that
  // had a "Permissions" tab and a "Connection" tab.
  // PAGE_INFO_PERMISSIONS_TAB_SELECTED = 1,
  // PAGE_INFO_CONNECTION_TAB_SELECTED = 2,
  // PAGE_INFO_CONNECTION_TAB_SHOWN_IMMEDIATELY = 3,
  PAGE_INFO_COOKIES_DIALOG_OPENED = 4,
  PAGE_INFO_CHANGED_PERMISSION = 5,
  PAGE_INFO_CERTIFICATE_DIALOG_OPENED = 6,
  // No longer used; indicated a UI viewer for SCTs.
  // PAGE_INFO_TRANSPARENCY_VIEWER_OPENED = 7,
  PAGE_INFO_CONNECTION_HELP_OPENED = 8,
  PAGE_INFO_SITE_SETTINGS_OPENED = 9,
  PAGE_INFO_SECURITY_DETAILS_OPENED = 10,
  PAGE_INFO_COOKIES_ALLOWED_FOR_SITE = 11,
  PAGE_INFO_COOKIES_BLOCKED_FOR_SITE = 12,
  PAGE_INFO_COOKIES_CLEARED = 13,
  PAGE_INFO_PERMISSION_DIALOG_OPENED = 14,
  PAGE_INFO_PERMISSIONS_CLEARED = 15,
  // No longer used; indicated permission change but was a duplicate metric.
  // PAGE_INFO_PERMISSIONS_CHANGED = 16,
  PAGE_INFO_FORGET_SITE_OPENED = 17,
  PAGE_INFO_FORGET_SITE_CLEARED = 18,
  PAGE_INFO_HISTORY_OPENED = 19,
  PAGE_INFO_HISTORY_ENTRY_REMOVED = 20,
  PAGE_INFO_HISTORY_ENTRY_CLICKED = 21,
  PAGE_INFO_PASSWORD_REUSE_ALLOWED = 22,
  PAGE_INFO_CHANGE_PASSWORD_PRESSED = 23,
  PAGE_INFO_SAFETY_TIP_HELP_OPENED = 24,
  PAGE_INFO_CHOOSER_OBJECT_DELETED = 25,
  PAGE_INFO_RESET_DECISIONS_CLICKED = 26,
  PAGE_INFO_STORE_INFO_CLICKED = 27,
  PAGE_INFO_ABOUT_THIS_SITE_PAGE_OPENED = 28,
  PAGE_INFO_ABOUT_THIS_SITE_SOURCE_LINK_CLICKED = 29,
  PAGE_INFO_AD_PERSONALIZATION_PAGE_OPENED = 30,
  PAGE_INFO_AD_PERSONALIZATION_SETTINGS_OPENED = 31,
  PAGE_INFO_ABOUT_THIS_SITE_MORE_ABOUT_CLICKED = 32,
  PAGE_INFO_COOKIES_PAGE_OPENED = 33,
  PAGE_INFO_COOKIES_SETTINGS_OPENED = 34,
  PAGE_INFO_ALL_SITES_WITH_FPS_FILTER_OPENED = 35,
  kMaxValue = PAGE_INFO_ALL_SITES_WITH_FPS_FILTER_OPENED
};

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_ACTION_H_