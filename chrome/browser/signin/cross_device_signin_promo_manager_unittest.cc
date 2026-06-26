// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_signin_promo_manager.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrossDeviceSigninPromoManagerTest : public testing::Test {
 public:
  CrossDeviceSigninPromoManagerTest() = default;
  ~CrossDeviceSigninPromoManagerTest() override = default;

  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());

    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::FakeDeviceInfoSyncService>();
        }));

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  syncer::FakeDeviceInfoTracker* device_info_tracker() {
    auto* service = static_cast<syncer::FakeDeviceInfoSyncService*>(
        DeviceInfoSyncServiceFactory::GetForProfile(profile_.get()));
    return static_cast<syncer::FakeDeviceInfoTracker*>(
        service->GetDeviceInfoTracker());
  }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  void SetHistoryAndTabsSyncingPreference(bool enable_sync) {
    auto* user_settings = sync_service()->GetUserSettings();
    user_settings->SetSelectedType(syncer::UserSelectableType::kHistory,
                                   /*is_type_on=*/enable_sync);
    user_settings->SetSelectedType(syncer::UserSelectableType::kTabs,
                                   /*is_type_on=*/enable_sync);
    user_settings->SetSelectedType(syncer::UserSelectableType::kSavedTabGroups,
                                   /*is_type_on=*/enable_sync);
  }

  void AddDevice(const std::string& guid, bool is_local) {
    syncer::TestDeviceInfoBuilder builder(syncer::DeviceInfo::OsType::kLinux);
    builder.WithGuid(guid).WithLastUpdatedTimestamp(base::Time::Now());
    auto device = builder.Build();
    device_info_tracker()->Add(std::move(device));
    if (is_local) {
      device_info_tracker()->SetLocalCacheGuid(guid);
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kCrossDeviceSigninFromDesktop};
};

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_FeatureFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(switches::kCrossDeviceSigninFromDesktop);

  // Setup: Signed in, only local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_SignedOut) {
  // Setup: User is signed out.
  ASSERT_FALSE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_AuthError) {
  // Setup: User is signed in but has a persistent error.
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_HasOtherDevices) {
  // Setup: Signed in, local device added, remote device added.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  AddDevice("remote_device_guid", /*is_local=*/false);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_HistorySyncDisabled) {
  // Setup: Signed in, only local device, history sync disabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(false);

  EXPECT_FALSE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest, ShouldShowPromo_HistorySyncEnabled) {
  // Setup: Signed in, only local device, history sync enabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(true);

  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kHistoryPage, profile()));
}

TEST_F(CrossDeviceSigninPromoManagerTest,
       ShouldShowPromo_ProfileMenuEntryIgnoreHistorySync) {
  // Setup: Signed in, only local device, history sync disabled.
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  AddDevice("local_device_guid", /*is_local=*/true);
  SetHistoryAndTabsSyncingPreference(false);

  // Profile menu promo should still return true even if history sync is
  // disabled.
  EXPECT_TRUE(ShouldShowCrossDeviceSigninPromo(
      CrossDeviceSigninPromoEntryPoint::kProfileMenu, profile()));
}
