// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/reporting_util.h"

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/account_id/account_id.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace reporting {

base::Value::Dict GetContext(Profile* profile) {
  base::Value::Dict context;
  context.SetByDottedPath("browser.userAgent",
                          embedder_support::GetUserAgent());

  if (!profile)
    return context;

  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (entry) {
    context.SetByDottedPath("profile.profileName", entry->GetName());
    context.SetByDottedPath("profile.gaiaEmail", entry->GetUserName());
  }

  context.SetByDottedPath("profile.profilePath",
                          profile->GetPath().AsUTF8Unsafe());

  std::optional<std::string> client_id = GetUserClientId(profile);
  if (client_id)
    context.SetByDottedPath("profile.clientId", *client_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string device_dm_token = GetDeviceDmToken(profile);
  if (!device_dm_token.empty())
    context.SetByDottedPath("device.dmToken", device_dm_token);
#endif

  std::optional<std::string> user_dm_token = GetUserDmToken(profile);
  if (user_dm_token)
    context.SetByDottedPath("profile.dmToken", *user_dm_token);

  return context;
}

// Returns User DMToken for a given |profile| if:
// * |profile| is NOT incognito profile.
// * |profile| is NOT sign-in screen profile
// * user corresponding to a |profile| is managed.
// Otherwise returns empty string. More about DMToken:
// go/dmserver-domain-model#dmtoken.
std::optional<std::string> GetUserDmToken(Profile* profile) {
  return std::nullopt;
}

std::optional<std::string> GetUserClientId(Profile* profile) {
  return std::nullopt;
}

}  // namespace reporting
