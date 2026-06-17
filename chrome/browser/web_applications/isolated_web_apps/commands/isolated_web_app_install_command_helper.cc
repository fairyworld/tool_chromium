// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/isolated_web_apps/bundle_operations/bundle_operations.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

namespace {

using web_package::SignedWebBundleId;
using web_package::SignedWebBundleIntegrityBlock;

bool IntegrityBlockDataHasRotatedKey(
    base::optional_ref<const IsolatedWebAppIntegrityBlockData>
        integrity_block_data,
    base::span<const uint8_t> rotated_key) {
  return integrity_block_data &&
         integrity_block_data->HasPublicKey(rotated_key);
}

}  // namespace

std::optional<KeyRotationData> GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data) {
  const auto* kr_info =
      ChromeIwaRuntimeDataProvider::GetInstance().GetKeyRotationInfo(
          web_bundle_id.id());
  if (!kr_info) {
    return std::nullopt;
  }
  const auto& rotated_key = kr_info->public_key;

  // Checks whether `rotated_key` is contained in
  // `isolation_data.integrity_block_data`.
  const bool current_installation_has_rk = IntegrityBlockDataHasRotatedKey(
      isolation_data.integrity_block_data(), rotated_key);
  const auto& pending_update = isolation_data.pending_update_info();

  // Checks whether `rotated_key` is contained in
  // `isolation_data.pending_update_info.integrity_block_data`.
  const bool pending_update_has_rk =
      pending_update && IntegrityBlockDataHasRotatedKey(
                            pending_update->integrity_block_data, rotated_key);

  return {{.rotated_key = rotated_key,
           .current_installation_has_rk = current_installation_has_rk,
           .pending_update_has_rk = pending_update_has_rk}};
}

VersionChangeValidationResult ValidateVersionChangeFeasibility(
    const IwaVersion& expected_version,
    const IwaVersion& installed_version,
    bool allow_downgrades,
    bool same_version_update_allowed_by_key_rotation) {
  if (expected_version < installed_version && !allow_downgrades) {
    return VersionChangeValidationResult::kDowngradeDisallowed;
  }
  if (expected_version == installed_version &&
      !same_version_update_allowed_by_key_rotation) {
    return VersionChangeValidationResult::kSameVersionUpdateDisallowed;
  }
  return VersionChangeValidationResult::kAllowed;
}

IsolatedWebAppInstallCommandHelper::IsolatedWebAppInstallCommandHelper(
    IsolatedWebAppUrlInfo url_info)
    : url_info_(std::move(url_info)) {}

IsolatedWebAppInstallCommandHelper::~IsolatedWebAppInstallCommandHelper() =
    default;

void IsolatedWebAppInstallCommandHelper::CreateStoragePartitionIfNotPresent(
    Profile& profile) {
  profile.GetStoragePartition(url_info_.storage_partition_config(&profile),
                              /*can_create=*/true);
}


}  // namespace web_app
