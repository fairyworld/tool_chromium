// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_list.h"
#include "components/safe_browsing/core/browser/db/sb_store.h"
#include "components/safe_browsing/core/browser/db/sb_store_file_format.h"

namespace safe_browsing {

// Enumerate different failure events while parsing the file read from disk for
// histogramming purposes. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
// LINT.IfChange(V5StoreReadResult)
enum class V5StoreReadResult {
  // No errors.
  kReadSuccess = 0,

  // Reserved for errors in parsing this enum.
  kUnexpectedReadFailure = 1,

  // The store file could not be opened (e.g. missing, access denied).
  kFileOpenFailure = 2,

  // The file was found to be empty.
  kFileEmptyFailure = 3,

  // The contents of the file could not be interpreted as a valid
  // V5StoreFileFormat proto.
  kProtoParsingFailure = 4,

  // The magic number didn't match. We're most likely trying to read a file
  // that doesn't contain hash prefixes.
  kUnexpectedMagicNumberFailure = 5,

  // The version of the file is different from expected and Chromium doesn't
  // know how to interpret this version of the file.
  kFileVersionIncompatibleFailure = 6,

  // The rest of the file could not be parsed.
  kHashPrefixInfoMissingFailure = 7,

  // Unable to generate the hash prefix list from the updates on disk.
  kHashPrefixListGenerationFailure = 8,

  // A read error occurred while parsing the file.
  kFileReadFailure = 9,

  kMaxValue = kFileReadFailure
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingV5StoreReadResult)

class V5Store : public SBStore {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // If the store is being created to apply an update to the old store, then
  // |old_file_size| is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  V5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          PrefixSize prefix_size,
          int64_t old_file_size = 0);
  ~V5Store() override;

  const std::string& version() const { return version_; }

  // Reads the store file from disk and populates the in-memory representation
  // of the hash prefixes.
  void Initialize();

 protected:
  std::string GetMetricPrefix() const override;

 private:
  friend class V5StoreTest;

  // Reads the state of the store from the file on disk and returns the reason
  // for the failure or reports success.
  V5StoreReadResult ReadFromDisk();

  std::unique_ptr<HashPrefixList> hash_prefix_list_;

  // The version of the store as returned by the PVer5 server in the last
  // applied update response.
  std::string version_;

  // The checksum value as read from the disk, until it is verified. Once
  // verified, it is cleared.
  std::string expected_checksum_;

  // Records the status of the update being applied to the database.
  V5ApplyUpdateResult last_apply_update_result_ = V5ApplyUpdateResult::kUnknown;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V5_STORE_H_
