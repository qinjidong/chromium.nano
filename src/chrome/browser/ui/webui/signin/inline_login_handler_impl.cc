// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/chrome_colors/selected_colors_info.h"
#include "chrome/browser/password_manager/password_reuse_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_split.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

// Subset of signin_metrics::Reason that is supported by the
// InlineLoginHandlerImpl.
enum class HandlerSigninReason {
  kForcedSigninPrimaryAccount,
  kReauthentication,
  kFetchLstOnly
};

// Decodes the signin reason from the URL parameter.
HandlerSigninReason GetHandlerSigninReason(const GURL& url) {
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(url);
  switch (reason) {
    case signin_metrics::Reason::kForcedSigninPrimaryAccount:
      return HandlerSigninReason::kForcedSigninPrimaryAccount;
    case signin_metrics::Reason::kReauthentication:
      return HandlerSigninReason::kReauthentication;
    case signin_metrics::Reason::kFetchLstOnly:
      return HandlerSigninReason::kFetchLstOnly;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected signin reason: " << static_cast<int>(reason);
      return HandlerSigninReason::kForcedSigninPrimaryAccount;
  }
}

void SetProfileLocked(const base::FilePath profile_path, bool locked) {
  if (profile_path.empty())
    return;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return;

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry)
    return;

  if (signin_util::IsForceSigninEnabled())
    entry->LockForceSigninProfile(locked);
}

void UnlockProfileAndHideLoginUI(const base::FilePath profile_path,
                                 InlineLoginHandlerImpl* handler) {
  SetProfileLocked(profile_path, false);
  handler->CloseDialogFromJavascript();
  ProfilePicker::Hide();
}

void LockProfileAndShowUserManager(const base::FilePath& profile_path) {
  SetProfileLocked(profile_path, true);
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
}

}  // namespace

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool confirm_untrusted_signin,
    bool is_force_sign_in_with_usermanager)
    : gaia_auth_fetcher_(this, gaia::GaiaSource::kChrome, url_loader_factory),
      handler_(handler),
      profile_(profile),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      auth_code_(auth_code),
      confirm_untrusted_signin_(confirm_untrusted_signin),
      is_force_sign_in_with_usermanager_(is_force_sign_in_with_usermanager) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  DCHECK(!auth_code_.empty());
  DCHECK(handler);

  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code_, signin_scoped_device_id);
}

InlineSigninHelper::~InlineSigninHelper() {}

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  if (is_force_sign_in_with_usermanager_) {
    // If user sign in in UserManager with force sign in enabled, the browser
    // window won't be opened until now.
    UnlockProfileAndHideLoginUI(profile_->GetPath(), handler_.get());
    // TODO(crbug.com/40764426): In case of reauth, wait until cookies
    // are set before opening a browser window.
    profiles::OpenBrowserWindowForProfile(
        base::IgnoreArgs<Browser*>(base::BindOnce(
            &InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened,
            base::Unretained(this), result)),
        true, false, true, profile_);
  } else {
    OnClientOAuthSuccessAndBrowserOpened(result);
  }
}

void InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened(
    const ClientOAuthResult& result) {
  HandlerSigninReason reason = GetHandlerSigninReason(current_url_);
  if (reason == HandlerSigninReason::kFetchLstOnly) {
    // Constants are only available on Windows for the Google Credential
    // Provider for Windows.
#if BUILDFLAG(IS_WIN)
    std::string json_retval;
    base::Value::Dict args;
    handler_->SendLSTFetchResultsMessage(base::Value(std::move(args)));
#else
    NOTREACHED_IN_MIGRATION()
        << "Google Credential Provider is only available on Windows";
#endif  // BUILDFLAG(IS_WIN)
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  this);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Successful");

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  std::string sync_email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;

  if (!password_.empty()) {
    password_manager::PasswordReuseManager* reuse_manager =
        PasswordReuseManagerFactory::GetForProfile(profile_);
    if (reuse_manager) {
      reuse_manager->SaveGaiaPasswordHash(
          sync_email, base::UTF8ToUTF16(password_),
          /*is_sync_password_for_metrics=*/!sync_email.empty(),
          password_manager::metrics_util::GaiaPasswordHashChange::
              SAVED_ON_CHROME_SIGNIN);
    }
  }

  if (reason == HandlerSigninReason::kReauthentication) {
    DCHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
    // TODO(b/278545484): support LST binding for refresh tokens created by
    // InlineSigninHelper.
    identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
        gaia_id_, email_, result.refresh_token,
        result.is_under_advanced_protection,
        signin_metrics::AccessPoint::ACCESS_POINT_FORCED_SIGNIN,
        signin_metrics::SourceForRefreshTokenOperation::
            kInlineLoginHandler_Signin);
  } else {
    if (confirm_untrusted_signin_) {
      // Display a confirmation dialog to the user.
      base::RecordAction(
          base::UserMetricsAction("Signin_Show_UntrustedSigninPrompt"));
      Browser* browser = chrome::FindLastActiveWithProfile(profile_);
      browser->window()->ShowOneClickSigninConfirmation(
          base::UTF8ToUTF16(email_),
          base::BindOnce(&InlineSigninHelper::UntrustedSigninConfirmed,
                         base::Unretained(this), result.refresh_token));
      return;
    }
    CreateSyncStarter(result.refresh_token);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void InlineSigninHelper::UntrustedSigninConfirmed(
    const std::string& refresh_token,
    bool confirmed) {
  DCHECK(signin_util::IsForceSigninEnabled());
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
  if (confirmed) {
    CreateSyncStarter(refresh_token);
    return;
  }

  base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
  BrowserList::CloseAllBrowsersWithProfile(
      profile_, base::BindRepeating(&LockProfileAndShowUserManager),
      // Cannot be called because  skip_beforeunload is true.
      BrowserList::CloseCallback(),
      /*skip_beforeunload=*/true);
}

void InlineSigninHelper::CreateSyncStarter(const std::string& refresh_token) {
  DCHECK(signin_util::IsForceSigninEnabled());
}

void InlineSigninHelper::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  if (handler_) {
    handler_->HandleLoginError(
        SigninUIError::FromGoogleServiceAuthError(email_, error));
  }

  HandlerSigninReason reason = GetHandlerSigninReason(current_url_);
  if (reason != HandlerSigninReason::kFetchLstOnly) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile_);
    about_signin_internals->OnRefreshTokenReceived("Failure");
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

InlineLoginHandlerImpl::InlineLoginHandlerImpl()
    : confirm_untrusted_signin_(false) {}

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() {}

// static
void InlineLoginHandlerImpl::SetExtraInitParams(base::Value::Dict& params) {
  params.Set("service", "chromiumsync");

  // If this was called from the user manager to reauthenticate the profile,
  // make sure the webui is aware.
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  if (HasFromProfilePickerURLParameter(current_url))
    params.Set("dontResizeNonEmbeddedPages", true);

  HandlerSigninReason reason = GetHandlerSigninReason(current_url);

  const GURL& url = GaiaUrls::GetInstance()->embedded_signin_url();
  params.Set("clientId", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.Set("gaiaPath", url.path().substr(1));

  std::string flow;
  switch (reason) {
    case HandlerSigninReason::kReauthentication:
      flow = "reauth";
      break;
    case HandlerSigninReason::kForcedSigninPrimaryAccount:
      flow = "enterprisefsi";
      break;
    case HandlerSigninReason::kFetchLstOnly:
      flow = "signin";
      break;
  }
  params.Set("flow", flow);
}

void InlineLoginHandlerImpl::CompleteLogin(const CompleteLoginParams& params) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();

  if (params.skip_for_now) {
    SyncSetupFailed();
    return;
  }

  // This value exists only for webview sign in.
  if (params.trusted_found)
    confirm_untrusted_signin_ = !params.trusted_value;

  DCHECK(!params.email.empty());
  DCHECK(!params.gaia_id.empty());
  DCHECK(!params.auth_code.empty());

  content::StoragePartition* partition =
      signin::GetSigninPartition(contents->GetBrowserContext());

  HandlerSigninReason reason = GetHandlerSigninReason(current_url);
  Profile* profile = Profile::FromWebUI(web_ui());

  bool force_sign_in_with_usermanager = false;
  base::FilePath path;
  if (reason != HandlerSigninReason::kFetchLstOnly &&
      HasFromProfilePickerURLParameter(current_url)) {
    DCHECK(reason == HandlerSigninReason::kForcedSigninPrimaryAccount ||
           reason == HandlerSigninReason::kReauthentication);
    DCHECK(signin_util::IsForceSigninEnabled());

    force_sign_in_with_usermanager = true;
    path = profile->GetPath();
  }

  FinishCompleteLogin(
      FinishCompleteLoginParams(
          this, partition, current_url, path, confirm_untrusted_signin_,
          params.email, params.gaia_id, params.password, params.auth_code,
          force_sign_in_with_usermanager),
      profile);
}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    InlineLoginHandlerImpl* handler,
    content::StoragePartition* partition,
    const GURL& url,
    const base::FilePath& profile_path,
    bool confirm_untrusted_signin,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    bool is_force_sign_in_with_usermanager)
    : handler(handler),
      partition(partition),
      url(url),
      profile_path(profile_path),
      confirm_untrusted_signin(confirm_untrusted_signin),
      email(email),
      gaia_id(gaia_id),
      password(password),
      auth_code(auth_code),
      is_force_sign_in_with_usermanager(is_force_sign_in_with_usermanager) {}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    const FinishCompleteLoginParams& other) = default;

InlineLoginHandlerImpl::FinishCompleteLoginParams::
    ~FinishCompleteLoginParams() {}

// static
void InlineLoginHandlerImpl::FinishCompleteLogin(
    const FinishCompleteLoginParams& params,
    Profile* profile) {
  DCHECK(params.handler);
  HandlerSigninReason reason = GetHandlerSigninReason(params.url);

  std::string default_email;
  net::GetValueForKeyInQuery(params.url, "email", &default_email);
  std::string validate_email;
  net::GetValueForKeyInQuery(params.url, "validateEmail", &validate_email);

  // When doing a SAML sign in, this email check may result in a false positive.
  // This happens when the user types one email address in the gaia sign in
  // page, but signs in to a different account in the SAML sign in page.
  if (validate_email == "1" && !default_email.empty()) {
    if (!gaia::AreEmailsSame(params.email, default_email)) {
      params.handler->HandleLoginError(
          SigninUIError::WrongReauthAccount(params.email, default_email));
      return;
    }
  }

  SigninUIError can_offer_error = SigninUIError::Ok();
  switch (reason) {
    case HandlerSigninReason::kReauthentication:
    case HandlerSigninReason::kForcedSigninPrimaryAccount:
      can_offer_error = CanOfferSignin(profile, params.gaia_id, params.email);
      break;
    case HandlerSigninReason::kFetchLstOnly:
      break;
  }

  if (!can_offer_error.IsOk()) {
    params.handler->HandleLoginError(can_offer_error);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  if (about_signin_internals)
    about_signin_internals->OnAuthenticationResultReceived("Successful");

  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(profile);

  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(
      params.handler->GetWeakPtr(),
      params.partition->GetURLLoaderFactoryForBrowserProcess(), profile,
      params.url, params.email, params.gaia_id, params.password,
      params.auth_code, signin_scoped_device_id,
      params.confirm_untrusted_signin,
      params.is_force_sign_in_with_usermanager);

  // If opened from user manager to unlock a profile, make sure the user manager
  // is closed and that the profile is marked as unlocked.
  if (reason != HandlerSigninReason::kFetchLstOnly &&
      !params.is_force_sign_in_with_usermanager) {
    UnlockProfileAndHideLoginUI(params.profile_path, params.handler);
  }
}

void InlineLoginHandlerImpl::HandleLoginError(const SigninUIError& error) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetLastCommittedURL();
  HandlerSigninReason reason = GetHandlerSigninReason(current_url);

  if (reason == HandlerSigninReason::kFetchLstOnly) {
    base::Value::Dict error_value;
    SendLSTFetchResultsMessage(base::Value(std::move(error_value)));
    return;
  }
  SyncSetupFailed();

  if (!error.IsOk()) {
    Browser* browser = GetDesktopBrowser();
    Profile* profile = Profile::FromWebUI(web_ui());
    LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
        browser, error, HasFromProfilePickerURLParameter(current_url));
  }
}

void InlineLoginHandlerImpl::SendLSTFetchResultsMessage(
    const base::Value& arg) {
  if (IsJavascriptAllowed())
    FireWebUIListener("send-lst-fetch-results", arg);
}

Browser* InlineLoginHandlerImpl::GetDesktopBrowser() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui()));
  return browser;
}

void InlineLoginHandlerImpl::SyncSetupFailed() {
  content::WebContents* contents = web_ui()->GetWebContents();

  if (contents->GetController().GetPendingEntry()) {
    // Do nothing if a navigation is pending, since this call can be triggered
    // from DidStartLoading. This avoids deleting the pending entry while we are
    // still navigating to it. See crbug/346632.
    return;
  }

  // Redirect to NTP.
  GURL url(chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params, /*navigation_handle_callback=*/{});
}
