// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"

#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/identity/iwa_identity_validator.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Each;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

IsolatedWebAppUrlInfo CreateRandomIsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForProxyMode();
  return IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      signed_web_bundle_id);
}

class IsolatedWebAppInstallCommandHelperTest : public ::testing::Test {
 public:
  void SetUp() override {
    IwaIdentityValidator::CreateSingleton();
    scoped_feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppDevMode}, {});
  }

  TestingProfile* profile() const { return profile_.get(); }

  content::WebContents& web_contents() {
    if (web_contents_ == nullptr) {
      web_contents_ = content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
    }
    return *web_contents_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_ = []() {
    TestingProfile::Builder builder;
    return builder.Build();
  }();
  std::unique_ptr<content::WebContents> web_contents_;
};

using IsolatedWebAppInstallCommandHelperStoragePartitionTest =
    IsolatedWebAppInstallCommandHelperTest;

TEST_F(IsolatedWebAppInstallCommandHelperStoragePartitionTest,
       CreateIfNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper =
      std::make_unique<IsolatedWebAppInstallCommandHelper>(url_info);

  command_helper->CreateStoragePartitionIfNotPresent(*profile());
  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              NotNull());
}

TEST_F(IsolatedWebAppInstallCommandHelperStoragePartitionTest,
       CreateIfPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command_helper =
      std::make_unique<IsolatedWebAppInstallCommandHelper>(url_info);

  auto* partition = profile()->GetStoragePartition(
      url_info.storage_partition_config(profile()),
      /*can_create=*/true);

  command_helper->CreateStoragePartitionIfNotPresent(*profile());
  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              Eq(partition));
}

// Deleted redundant tests covered in install command unittests.

}  // namespace
}  // namespace web_app
