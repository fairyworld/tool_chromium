// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_store.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace safe_browsing {

namespace {

const uint32_t kFileVersion = 10;
const char kApplyUpdate[] = ".ApplyUpdate";
const char kResult[] = ".Result";

void RecordStoreReadResult(V5StoreReadResult result) {
  base::UmaHistogramEnumeration("SafeBrowsing.V5StoreRead.Result", result);
  base::UmaHistogramBoolean("SafeBrowsing.SBStoreRead.Success",
                            result == V5StoreReadResult::kReadSuccess);
}

template <typename T>
void RecordEnumWithAndWithoutSuffix(const std::string& metric,
                                    T value,
                                    const base::FilePath& file_path) {
  base::UmaHistogramEnumeration(metric + kResult, value);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramEnumeration(metric + kResult + suffix, value);
}

void RecordApplyUpdateResult(const std::string& base_metric,
                             V5ApplyUpdateResult result,
                             const base::FilePath& file_path) {
  RecordEnumWithAndWithoutSuffix(base_metric + kApplyUpdate, result, file_path);
}

}  // namespace

V5Store::V5Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 PrefixSize prefix_size,
                 const int64_t old_file_size)
    : SBStore(task_runner, store_path, old_file_size),
      hash_prefix_list_(std::make_unique<HashPrefixList>(store_path,
                                                         prefix_size,
                                                         task_runner)) {}

V5Store::~V5Store() = default;

void V5Store::Initialize() {
  CHECK(version_.empty());

  V5StoreReadResult store_read_result = ReadFromDisk();
  has_valid_data_ = (store_read_result == V5StoreReadResult::kReadSuccess);
  RecordStoreReadResult(store_read_result);
}

std::string V5Store::GetMetricPrefix() const {
  return "SafeBrowsing.V5Store";
}

V5StoreReadResult V5Store::ReadFromDisk() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  V5StoreFileFormat file_format;
  int64_t file_size;
  {
    BaseFileInputStream input_stream(store_path_);
    if (!file_format.ParseFromZeroCopyStream(&input_stream)) {
      return input_stream.GetError() != base::File::FILE_OK
                 ? V5StoreReadResult::kFileReadFailure
                 : V5StoreReadResult::kProtoParsingFailure;
    }
    // `ParseFromZeroCopyStream` will return true if the file didn't exist, so
    // explicitly check for an error when reading from the file.
    if (input_stream.GetError() != base::File::FILE_OK) {
      return V5StoreReadResult::kFileOpenFailure;
    }
    file_size = input_stream.ByteCount();
    if (!file_size) {
      return V5StoreReadResult::kFileEmptyFailure;
    }
  }

  if (file_format.magic_number() != kFileMagic) {
    return V5StoreReadResult::kUnexpectedMagicNumberFailure;
  }

  if (file_format.file_version() != kFileVersion) {
    return V5StoreReadResult::kFileVersionIncompatibleFailure;
  }

  if (!file_format.has_list_details()) {
    return V5StoreReadResult::kHashPrefixInfoMissingFailure;
  }

  V5ApplyUpdateResult apply_update_result =
      hash_prefix_list_->ReadFromDisk(SBStoreFileFormat(&file_format));
  RecordApplyUpdateResult("SafeBrowsing.V5ReadFromDisk", apply_update_result,
                          store_path_);
  last_apply_update_result_ = apply_update_result;

  if (apply_update_result != V5ApplyUpdateResult::kSuccess) {
    hash_prefix_list_->Clear();
    return V5StoreReadResult::kHashPrefixListGenerationFailure;
  }

  if (file_format.list_details().has_version()) {
    version_ = file_format.list_details().version();
  }
  if (file_format.list_details().has_checksum() &&
      file_format.list_details().checksum().has_sha256()) {
    expected_checksum_ = file_format.list_details().checksum().sha256();
  }

  // Update |file_size_| now because we parsed the file correctly.
  file_size_ = file_size;
  if (file_format.list_details().has_hash_file()) {
    file_size_ += file_format.list_details().hash_file().file_size();
  }

  return V5StoreReadResult::kReadSuccess;
}

}  // namespace safe_browsing
