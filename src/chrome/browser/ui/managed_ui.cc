// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/managed_ui.h"

#include <optional>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/management/management_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace chrome {

namespace {

enum ManagementStringType : size_t {
  BROWSER_MANAGED = 0,
  BROWSER_MANAGED_BY = 1,
  BROWSER_PROFILE_SAME_MANAGED_BY = 2,
  BROWSER_PROFILE_DIFFERENT_MANAGED_BY = 3,
  BROWSER_MANAGED_PROFILE_MANAGED_BY = 4,
  PROFILE_MANAGED_BY = 5,
  SUPERVISED = 6,
  NOT_MANAGED = 7
};

const char* g_device_manager_for_testing = nullptr;

bool ShouldDisplayManagedByParentUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  // Don't display the managed by parent UI on ChromeOS, because similar UI is
  // displayed at the OS level.
  return false;
#else
  return profile &&
         supervised_user::IsSubjectToParentalControls(*profile->GetPrefs());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

ManagementStringType GetManagementStringType(Profile* profile) {
  return SUPERVISED;
}

}  // namespace

ScopedDeviceManagerForTesting::ScopedDeviceManagerForTesting(
    const char* manager) {
  previous_manager_ = g_device_manager_for_testing;
  g_device_manager_for_testing = manager;
}

ScopedDeviceManagerForTesting::~ScopedDeviceManagerForTesting() {
  g_device_manager_for_testing = previous_manager_;
}

bool ShouldDisplayManagedUi(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Don't show the UI in demo mode.
  if (ash::DemoSession::IsDeviceInDemoMode())
    return false;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Don't show the UI for Family Link accounts.
  if (profile->IsChild())
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  return ShouldDisplayManagedByParentUi(profile);
}

#if !BUILDFLAG(IS_ANDROID)

GURL GetManagedUiUrl(Profile* profile) {
  if (ShouldDisplayManagedByParentUi(profile)) {
    return GURL(supervised_user::kManagedByParentUiMoreInfoUrl);
  }

  return GURL();
}

const gfx::VectorIcon& GetManagedUiIcon(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));
  CHECK(ShouldDisplayManagedByParentUi(profile));
  return vector_icons::kFamilyLinkIcon;
}

std::u16string GetManagedUiMenuItemLabel(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGED);
    case BROWSER_MANAGED_BY:
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGED_BY,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringUTF16(IDS_BROWSER_PROFILE_MANAGED);
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_PROFILE_MANAGED_BY,
                                        base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::u16string GetManagedUiMenuItemTooltip(Profile* profile) {
  CHECK(ShouldDisplayManagedUi(profile));
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  switch (GetManagementStringType(profile)) {
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY_TOOLTIP,
          base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_TOOLTIP,
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED:
    case BROWSER_MANAGED_BY:
    case BROWSER_PROFILE_SAME_MANAGED_BY:
    case PROFILE_MANAGED_BY:
    case SUPERVISED:
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::string GetManagedUiWebUIIcon(Profile* profile) {
  if (ShouldDisplayManagedByParentUi(profile)) {
    // The Family Link "kite" icon.
    return "cr20:kite";
  }

  // This method can be called even if we shouldn't display the managed UI.
  return std::string();
}

std::u16string GetManagedUiWebUILabel(Profile* profile) {
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGED_WITH_HYPERLINK,
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
    case BROWSER_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGED_BY_WITH_HYPERLINK,
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL),
          base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_SAME_MANAGED_BY_WITH_HYPERLINK,
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL),
          base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY_WITH_HYPERLINK,
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL),
          base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY_WITH_HYPERLINK,
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL),
          base::UTF8ToUTF16(*account_manager));
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_PROFILE_MANAGED_BY_WITH_HYPERLINK,
          base::UTF8ToUTF16(chrome::kChromeUIManagementURL),
          base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGED_BY_PARENT_WITH_HYPERLINK,
          base::UTF8ToUTF16(supervised_user::kManagedByParentUiMoreInfoUrl));
    case NOT_MANAGED:
      return std::u16string();
  }
  return std::u16string();
}

std::u16string GetDeviceManagedUiHelpLabel(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ManagementUI::GetManagementPageSubtitle(profile);
#else
  if (ShouldDisplayManagedByParentUi(profile)) {
    return l10n_util::GetStringUTF16(IDS_HELP_MANAGED_BY_YOUR_PARENT);
  }

  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string GetDeviceManagedUiWebUILabel() {
  int string_id = IDS_DEVICE_MANAGED_WITH_HYPERLINK;
  std::vector<std::u16string> replacements;
  replacements.push_back(base::UTF8ToUTF16(chrome::kChromeUIManagementURL));
  replacements.push_back(ui::GetChromeOSDeviceName());

  const std::optional<std::string> device_manager = GetDeviceManagerIdentity();
  if (device_manager && !device_manager->empty()) {
    string_id = IDS_DEVICE_MANAGED_BY_WITH_HYPERLINK;
    replacements.push_back(base::UTF8ToUTF16(*device_manager));
  }

  return l10n_util::GetStringFUTF16(string_id, replacements, nullptr);
}
#else
std::u16string GetManagementPageSubtitle(Profile* profile) {
  std::optional<std::string> account_manager =
      GetAccountManagerIdentity(profile);
  std::optional<std::string> device_manager = GetDeviceManagerIdentity();

  switch (GetManagementStringType(profile)) {
    case BROWSER_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_SUBTITLE);
    case BROWSER_MANAGED_BY:
      return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                        base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_SAME_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_SAME_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager));
    case BROWSER_PROFILE_DIFFERENT_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_AND_PROFILE_DIFFERENT_MANAGED_BY,
          base::UTF8ToUTF16(*device_manager),
          base::UTF8ToUTF16(*account_manager));
    case BROWSER_MANAGED_PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_BROWSER_MANAGED_AND_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*account_manager));
    case PROFILE_MANAGED_BY:
      return l10n_util::GetStringFUTF16(
          IDS_MANAGEMENT_SUBTITLE_PROFILE_MANAGED_BY,
          base::UTF8ToUTF16(*account_manager));
    case SUPERVISED:
      return l10n_util::GetStringUTF16(IDS_MANAGED_BY_PARENT);
    case NOT_MANAGED:
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
  }
  return std::u16string();
}
#endif

std::optional<std::string> GetDeviceManagerIdentity() {
  if (g_device_manager_for_testing) {
    return g_device_manager_for_testing;
  }

  return std::nullopt;
}

std::optional<std::string> GetAccountManagerIdentity(Profile* profile) {
  return std::nullopt;
}

}  // namespace chrome
