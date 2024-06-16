// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_PRECONDITION_CHECKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_PRECONDITION_CHECKER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace autofill {

// Helper class to determine whether a user is eligible for the CONTACT_INFO
// sync model type. This is needed to disable the model type for unsupported
// users in the model type controller. It is also needed to hide the opt-out
// option in the settings for unsupported users.
class ContactInfoPreconditionChecker
    : public syncer::SyncServiceObserver,
      public signin::IdentityManager::Observer {
 public:
  // `on_precondition_changed` is called whenever the result of
  // `GetPreconditionState()` has possibly changed.
  ContactInfoPreconditionChecker(
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      base::RepeatingClosure on_precondition_changed);
  ~ContactInfoPreconditionChecker() override;

  syncer::ModelTypeController::PreconditionState GetPreconditionState() const;

  // IdentityManager::Observer overrides.
  void OnRefreshTokensLoaded() override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  // Called by the `managed_status_finder_` when it determines the account type.
  void AccountTypeDetermined();

  const raw_ref<syncer::SyncService> sync_service_;
  const raw_ref<signin::IdentityManager> identity_manager_;
  const base::RepeatingClosure on_precondition_changed_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  std::unique_ptr<signin::AccountManagedStatusFinder> managed_status_finder_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_ADDRESSES_CONTACT_INFO_PRECONDITION_CHECKER_H_