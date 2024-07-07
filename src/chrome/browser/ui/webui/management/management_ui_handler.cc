// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "management_ui_handler.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/user_permission_service.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"

enum class ReportingType {
  kDevice,
  kExtensions,
  kSecurity,
  kUser,
  kUserActivity,
  kLegacyTech,
};

namespace {

base::Value::List GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  base::Value::List permission_messages;
  return permission_messages;
}

base::Value::List GetPowerfulExtensions(
    const extensions::ExtensionSet& extensions) {
  base::Value::List powerful_extensions;

  for (const auto& extension : extensions) {
    base::Value::List permission_messages =
        GetPermissionsForExtension(extension);

    // Only show extension on page if there is at least one permission
    // message to show.
    if (!permission_messages.empty()) {
      base::Value::Dict extension_to_add =
          extensions::util::GetExtensionInfo(extension.get());
      extension_to_add.Set("permissions", std::move(permission_messages));
      powerful_extensions.Append(std::move(extension_to_add));
    }
  }

  return powerful_extensions;
}

}  // namespace

ManagementUIHandler::ManagementUIHandler(Profile* profile) {
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId};
  UpdateAccountManagedState(profile);
#if !BUILDFLAG(IS_CHROMEOS)
  UpdateBrowserManagedState();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

ManagementUIHandler::~ManagementUIHandler() {
  DisallowJavascript();
}

#if !BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ManagementUIHandler> ManagementUIHandler::Create(
    Profile* profile) {
  return std::make_unique<ManagementUIHandler>(profile);
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getContextualManagedData",
      base::BindRepeating(&ManagementUIHandler::HandleGetContextualManagedData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getThreatProtectionInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetThreatProtectionInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getManagedWebsites",
      base::BindRepeating(&ManagementUIHandler::HandleGetManagedWebsites,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getApplications",
      base::BindRepeating(&ManagementUIHandler::HandleGetApplications,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initBrowserReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitBrowserReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initProfileReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitProfileReportingInfo,
                          base::Unretained(this)));
}

void ManagementUIHandler::OnJavascriptAllowed() {
  AddObservers();
}

void ManagementUIHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void ManagementUIHandler::AddReportingInfo(base::Value::List* report_sources,
                                           bool is_browser) {}

base::Value::Dict ManagementUIHandler::GetContextualManagedData(
    Profile* profile) {
  base::Value::Dict response;
#if !BUILDFLAG(IS_CHROMEOS)
  int message_id = IDS_MANAGEMENT_NOT_MANAGED_NOTICE;
  if (browser_managed_) {
    message_id = IDS_MANAGEMENT_BROWSER_NOTICE;
  } else if (account_managed_) {
    message_id = IDS_MANAGEMENT_PROFILE_NOTICE;
  }

  response.Set("browserManagementNotice",
               l10n_util::GetStringFUTF16(
                   message_id, chrome::kManagedUiLearnMoreUrl,
                   base::EscapeForHTML(l10n_util::GetStringUTF16(
                       IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  response.Set("pageSubtitle", chrome::GetManagementPageSubtitle(profile));

  response.Set("extensionReportingSubtitle",
               l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  response.Set(
      "applicationReportingSubtitle",
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_APPLICATIONS_INSTALLED));
  response.Set(
      "managedWebsitesSubtitle",
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  response.Set("managed", managed());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  return response;
}

base::Value::Dict ManagementUIHandler::GetThreatProtectionInfo(
    Profile* profile) {
  base::Value::List info;

  base::Value::Dict result;
  result.Set("description",
             l10n_util::GetStringUTF16(
                   IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION));
  result.Set("info", std::move(info));
  return result;
}

base::Value::List ManagementUIHandler::GetManagedWebsitesInfo(
    Profile* profile) const {
  base::Value::List managed_websites;
  auto* managed_configuration =
      ManagedConfigurationAPIFactory::GetForProfile(profile);

  if (!managed_configuration) {
    return managed_websites;
  }

  for (const auto& entry : managed_configuration->GetManagedOrigins()) {
    managed_websites.Append(entry.Serialize());
  }

  return managed_websites;
}

base::Value::List ManagementUIHandler::GetApplicationsInfo(
    Profile* profile) const {
  base::Value::List applications;
  return applications;
}

bool ManagementUIHandler::managed() const {
  return account_managed() || browser_managed_;
}

void ManagementUIHandler::RegisterPrefChange(
    PrefChangeRegistrar& pref_registrar) {
  pref_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&ManagementUIHandler::UpdateManagedState,
                          base::Unretained(this)));
}

void ManagementUIHandler::UpdateManagedState() {
#if !BUILDFLAG(IS_CHROMEOS)
  bool is_account_updated =
      UpdateAccountManagedState(Profile::FromWebUI(web_ui()));
  bool is_browser_updated = UpdateBrowserManagedState();
  if (is_account_updated || is_browser_updated) {
    FireWebUIListener("managed_data_changed");
  }
#endif
}

bool ManagementUIHandler::UpdateAccountManagedState(Profile* profile) {
  if (!profile) {
    CHECK_IS_TEST();
    return false;
  }
  bool new_managed = IsProfileManaged(profile);
  bool is_updated = (new_managed != account_managed_);
  account_managed_ = new_managed;
  return is_updated;
}

#if !BUILDFLAG(IS_CHROMEOS)
bool ManagementUIHandler::UpdateBrowserManagedState() {
  bool new_managed = false;
  bool is_updated = (new_managed != browser_managed_);
  browser_managed_ = new_managed;
  return is_updated;
}
#endif

std::string ManagementUIHandler::GetAccountManager(Profile* profile) const {
  std::optional<std::string> manager =
      chrome::GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = chrome::GetDeviceManagerIdentity();
  }

  return manager.value_or(std::string());
}

bool ManagementUIHandler::IsProfileManaged(Profile* profile) const {
  return false;
}

void ManagementUIHandler::HandleGetExtensions(const base::Value::List& args) {
  AllowJavascript();
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  ResolveJavascriptCallback(args[0] /* callback_id */,
                            GetPowerfulExtensions(extensions));
}

void ManagementUIHandler::HandleGetContextualManagedData(
    const base::Value::List& args) {
  AllowJavascript();
  auto result = GetContextualManagedData(Profile::FromWebUI(web_ui()));
  ResolveJavascriptCallback(args[0] /* callback_id */, result);
}

void ManagementUIHandler::HandleGetThreatProtectionInfo(
    const base::Value::List& args) {
  AllowJavascript();
  ResolveJavascriptCallback(
      args[0] /* callback_id */,
      GetThreatProtectionInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleGetManagedWebsites(
    const base::Value::List& args) {
  AllowJavascript();

  ResolveJavascriptCallback(
      args[0] /* callback_id */,
      GetManagedWebsitesInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleGetApplications(const base::Value::List& args) {
  AllowJavascript();

  ResolveJavascriptCallback(args[0] /* callback_id */,
                            GetApplicationsInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleInitBrowserReportingInfo(
    const base::Value::List& args) {
  base::Value::List report_sources;
  AllowJavascript();
  AddReportingInfo(&report_sources, /*is_browser=*/true);
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
}

void ManagementUIHandler::HandleInitProfileReportingInfo(
    const base::Value::List& args) {
  base::Value::List report_sources;
  AllowJavascript();
  AddReportingInfo(&report_sources, /*is_browser=*/false);
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
}

void ManagementUIHandler::NotifyBrowserReportingInfoUpdated() {
  base::Value::List report_sources;
  AddReportingInfo(&report_sources, /*is_browser=*/true);
  FireWebUIListener("browser-reporting-info-updated", report_sources);
}

void ManagementUIHandler::NotifyProfileReportingInfoUpdated() {
  base::Value::List report_sources;
  AddReportingInfo(&report_sources, /*is_browser=*/false);
  FireWebUIListener("profile-reporting-info-updated", report_sources);
}

void ManagementUIHandler::NotifyThreatProtectionInfoUpdated() {
  FireWebUIListener("threat-protection-info-updated",
                    GetThreatProtectionInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::OnExtensionLoaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::OnExtensionUnloaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason /*reason*/) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::AddObservers() {
  if (has_observers_) {
    return;
  }

  has_observers_ = true;

  auto* profile = Profile::FromWebUI(web_ui());

  extensions::ExtensionRegistry::Get(profile)->AddObserver(this);

  pref_registrar_.Init(profile->GetPrefs());

  RegisterPrefChange(pref_registrar_);
}

void ManagementUIHandler::RemoveObservers() {
  if (!has_observers_) {
    return;
  }

  has_observers_ = false;

  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);

  pref_registrar_.RemoveAll();
}
