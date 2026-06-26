// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_signin_promo_manager.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace {

bool IsUserSignedInWithNoError(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  return state == signin_util::SignedInState::kSignedIn ||
         state == signin_util::SignedInState::kSyncing;
}

bool HasOtherSignedInDevices(Profile* profile) {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  CHECK(device_info_sync_service);
  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  CHECK(device_info_tracker);

  for (const syncer::DeviceInfo* device_info :
       device_info_tracker->GetAllDeviceInfo()) {
    if (!device_info_tracker->IsRecentLocalCacheGuid(device_info->guid())) {
      return true;
    }
  }
  return false;
}

bool IsHistorySyncEnabled(Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory);
}

}  // namespace

bool ShouldShowCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop)) {
    return false;
  }

  // 1. General eligibility: Signed in with no errors, and has NO other devices.
  if (!IsUserSignedInWithNoError(profile) || HasOtherSignedInDevices(profile)) {
    return false;
  }

  // 2. Data-type eligibility check.
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      if (!IsHistorySyncEnabled(profile)) {
        return false;
      }
      break;
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      // Permanent entry point, no data-type constraints.
      break;
  }

  return true;
}

void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile) {
  // TODO(crbug.com/527889253): Implement the actual logic.
}

void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  // TODO(crbug.com/527889253): Implement the actual logic.
}

void OpenSigninToPhoneQrCodeBubble(Browser* browser,
                                   base::OnceClosure closing_callback) {
  CHECK(base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop));
  // TODO(crbug.com/527889253): Implement the actual logic.
  std::move(closing_callback).Run();
}
