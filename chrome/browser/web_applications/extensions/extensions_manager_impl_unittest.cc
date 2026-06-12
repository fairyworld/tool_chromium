// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/extensions_manager_impl.h"

#include <memory>
#include <string>

#include "chrome/browser/extensions/chrome_app_deprecation.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class ExtensionsManagerImplTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    extensions_manager_ = std::make_unique<ExtensionsManagerImpl>(profile());
  }

  void TearDown() override {
    extensions_manager_.reset();
    WebAppTest::TearDown();
  }

 protected:
  std::unique_ptr<ExtensionsManagerImpl> extensions_manager_;
};

TEST_F(ExtensionsManagerImplTest, IsExtensionBlockedByPolicy) {
  using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();

  constexpr char kTestAppId[] = "abcdefghijklmnopabcdefghijklmnop";

  // Default: not blocked.
  EXPECT_FALSE(extensions_manager_->IsExtensionBlockedByPolicy(kTestAppId));

  // Block by default.
  PolicyUpdater(prefs).SetBlocklistedByDefault(true);
  EXPECT_TRUE(extensions_manager_->IsExtensionBlockedByPolicy(kTestAppId));

  // Allow list.
  PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(kTestAppId,
                                                                 true);
  EXPECT_FALSE(extensions_manager_->IsExtensionBlockedByPolicy(kTestAppId));
}

TEST_F(ExtensionsManagerImplTest, IsExtensionInstalled) {
  constexpr char kTestAppId[] = "abcdefghijklmnopabcdefghijklmnop";
  auto* registry = extensions::ExtensionRegistry::Get(profile());

  EXPECT_FALSE(extensions_manager_->IsExtensionInstalled(kTestAppId));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test App").SetID(kTestAppId).Build();
  registry->AddEnabled(extension);

  EXPECT_TRUE(extensions_manager_->IsExtensionInstalled(kTestAppId));
}

TEST_F(ExtensionsManagerImplTest, IsExtensionForceInstalled) {
  using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();

  constexpr char kTestAppId[] = "abcdefghijklmnopabcdefghijklmnop";
  auto* registry = extensions::ExtensionRegistry::Get(profile());

  // Not force installed by default.
  EXPECT_FALSE(
      extensions_manager_->IsExtensionForceInstalled(kTestAppId, nullptr));

  // Set policy to force install, but extension is not installed yet.
  PolicyUpdater(prefs).SetIndividualExtensionAutoInstalled(
      kTestAppId, "https://clients2.google.com/service/update2/crx",
      /*forced=*/true);
  // It should still return false because the extension is not in Registry.
  EXPECT_FALSE(
      extensions_manager_->IsExtensionForceInstalled(kTestAppId, nullptr));

  // Install it.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test App").SetID(kTestAppId).Build();
  registry->AddEnabled(extension);

  std::u16string reason;
  EXPECT_TRUE(
      extensions_manager_->IsExtensionForceInstalled(kTestAppId, &reason));
  // Reason should not be empty.
  EXPECT_FALSE(reason.empty());
}

TEST_F(ExtensionsManagerImplTest, IsExtensionDefaultInstalled) {
  constexpr char kTestAppId[] = "abcdefghijklmnopabcdefghijklmnop";
  auto* registry = extensions::ExtensionRegistry::Get(profile());

  EXPECT_FALSE(extensions_manager_->IsExtensionDefaultInstalled(kTestAppId));

  // Install with default flag.
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test App")
          .SetID(kTestAppId)
          .AddFlags(extensions::Extension::WAS_INSTALLED_BY_DEFAULT)
          .Build();
  registry->AddEnabled(extension);

  EXPECT_TRUE(extensions_manager_->IsExtensionDefaultInstalled(kTestAppId));
}

TEST_F(ExtensionsManagerImplTest, IsExternalExtensionUninstalled) {
  constexpr char kTestAppId[] = "abcdefghijklmnopabcdefghijklmnop";

  EXPECT_FALSE(extensions_manager_->IsExternalExtensionUninstalled(kTestAppId));

  // Mark as uninstalled by writing to pref directly.
  {
    ScopedListPrefUpdate update(profile()->GetPrefs(),
                                "extensions.external_uninstalls");
    update->Append(kTestAppId);
  }

  EXPECT_TRUE(extensions_manager_->IsExternalExtensionUninstalled(kTestAppId));
}

TEST_F(ExtensionsManagerImplTest, IsPreinstalledExtensionAppId) {
  constexpr char kTestAppId[] = "abcdefghijklmnopabcdefghijklmnop";

  EXPECT_FALSE(extensions_manager_->IsPreinstalledExtensionAppId(kTestAppId));

  // Set override.
  extensions::chrome_app_deprecation::SetPreinstalledAppIdForTesting(
      kTestAppId);
  EXPECT_TRUE(extensions_manager_->IsPreinstalledExtensionAppId(kTestAppId));

  // Reset override.
  extensions::chrome_app_deprecation::SetPreinstalledAppIdForTesting(nullptr);
  EXPECT_FALSE(extensions_manager_->IsPreinstalledExtensionAppId(kTestAppId));

  // Check a real one (Gmail).
  EXPECT_TRUE(extensions_manager_->IsPreinstalledExtensionAppId(
      extension_misc::kGmailAppId));
}

}  // namespace web_app
