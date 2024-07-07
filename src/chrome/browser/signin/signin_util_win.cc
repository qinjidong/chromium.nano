// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util_win.h"

#include <memory>
#include <string>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "base/win/wincrypt_shim.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace signin_util {

namespace {

std::unique_ptr<TurnSyncOnHelper::Delegate>*
GetTurnSyncOnHelperDelegateForTestingStorage() {
  static base::NoDestructor<std::unique_ptr<TurnSyncOnHelper::Delegate>>
      delegate;
  return delegate.get();
}

// Extracts the |cred_provider_gaia_id| and |cred_provider_email| for the user
// signed in throuhg credential provider.
void ExtractCredentialProviderUser(std::wstring* cred_provider_gaia_id,
                                   std::wstring* cred_provider_email) {
  DCHECK(cred_provider_gaia_id);
  DCHECK(cred_provider_email);

  cred_provider_gaia_id->clear();
  cred_provider_email->clear();
}

// Attempt to sign in with a credentials from a system installed credential
// provider if available.  If |auth_gaia_id| is not empty then the system
// credential must be for the same account.  Starts the process to turn on DICE
// only if |turn_on_sync| is true.
bool TrySigninWithCredentialProvider(Profile* profile,
                                     const std::wstring& auth_gaia_id,
                                     bool turn_on_sync) {
  return false;
}

}  // namespace

void SetTurnSyncOnHelperDelegateForTesting(
    std::unique_ptr<TurnSyncOnHelper::Delegate> delegate) {
  GetTurnSyncOnHelperDelegateForTestingStorage()->swap(delegate);  // IN-TEST
}

// Credential provider needs to stick to profile it previously used to import
// credentials. Thus, if there is another profile that was previously signed in
// with credential provider regardless of whether user signed in or out,
// credential provider shouldn't attempt to import credentials into current
// profile.
bool IsGCPWUsedInOtherProfile(Profile* profile) {
  DCHECK(profile);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    std::vector<ProfileAttributesEntry*> entries =
        profile_manager->GetProfileAttributesStorage()
            .GetAllProfilesAttributes();

    for (const ProfileAttributesEntry* entry : entries) {
      if (entry->GetPath() == profile->GetPath())
        continue;

      if (entry->IsSignedInWithCredentialProvider())
        return true;
    }
  }

  return false;
}

void SigninWithCredentialProviderIfPossible(Profile* profile) {
  // This flow is used for first time signin through credential provider. Any
  // subsequent signin for the credential provider user needs to go through
  // reauth flow.
  if (profile->GetPrefs()->GetBoolean(prefs::kSignedInWithCredentialProvider))
    return;

  std::wstring cred_provider_gaia_id;
  std::wstring cred_provider_email;

  ExtractCredentialProviderUser(&cred_provider_gaia_id, &cred_provider_email);
  if (cred_provider_gaia_id.empty() || cred_provider_email.empty())
    return;

  // Chrome doesn't allow signing into current profile if the same user is
  // signed in another profile.
  if (!CanOfferSignin(profile, base::WideToUTF8(cred_provider_gaia_id),
                      base::WideToUTF8(cred_provider_email))
           .IsOk() ||
      IsGCPWUsedInOtherProfile(profile)) {
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  std::wstring gaia_id;
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    gaia_id = base::UTF8ToWide(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
            .gaia);
  }

  TrySigninWithCredentialProvider(profile, gaia_id, gaia_id.empty());
}

bool ReauthWithCredentialProviderIfPossible(Profile* profile) {
  // Check to see if auto signin information is available.  Only applies if:
  //
  //  - The profile is marked as having been signed in with a system credential.
  //  - The profile is already signed in.
  //  - The profile is in an auth error state.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!(profile->GetPrefs()->GetBoolean(
            prefs::kSignedInWithCredentialProvider) &&
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync) &&
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSync)))) {
    return false;
  }

  std::wstring gaia_id = base::UTF8ToWide(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .gaia.c_str());
  return TrySigninWithCredentialProvider(profile, gaia_id, false);
}

}  // namespace signin_util
