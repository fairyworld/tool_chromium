// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/sb_store.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"

namespace safe_browsing {

SBStore::SBStore(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path,
                 int64_t old_file_size)
    : file_size_(old_file_size),
      has_valid_data_(false),
      store_path_(store_path),
      task_runner_(task_runner) {}

SBStore::~SBStore() = default;

bool SBStore::HasValidData() {
  // Record every 256th time (`record_has_valid_data_counter_` is 8-bit).
  if (++record_has_valid_data_counter_ == 1) {
    RecordBooleanWithAndWithoutSuffix(GetMetricPrefix() + ".IsStoreValid",
                                      has_valid_data_, store_path_);
  }
  return has_valid_data_;
}

// static
void SBStore::RecordBooleanWithAndWithoutSuffix(
    const std::string& metric,
    bool value,
    const base::FilePath& file_path) {
  base::UmaHistogramBoolean(metric, value);
  std::string suffix = GetUmaSuffixForStore(file_path);
  base::UmaHistogramBoolean(metric + suffix, value);
}

BaseFileInputStream::BaseFileInputStream(const base::FilePath& input_file)
    : stream_(input_file), impl_(&stream_) {}

BaseFileInputStream::~BaseFileInputStream() = default;

base::File::Error BaseFileInputStream::GetError() const {
  return stream_.GetError();
}

bool BaseFileInputStream::Next(const void** data, int* size) {
  return impl_.Next(data, size);
}

void BaseFileInputStream::BackUp(int count) {
  return impl_.BackUp(count);
}

bool BaseFileInputStream::Skip(int count) {
  return impl_.Skip(count);
}

int64_t BaseFileInputStream::ByteCount() const {
  return impl_.ByteCount();
}

BaseFileInputStream::CopyingBaseFileInputStream::CopyingBaseFileInputStream(
    const base::FilePath& input_file)
    : file_(input_file,
            base::File::FLAG_OPEN | base::File::FLAG_READ |
                base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                base::File::FLAG_WIN_SHARE_DELETE) {}

BaseFileInputStream::CopyingBaseFileInputStream::~CopyingBaseFileInputStream() =
    default;

base::File::Error BaseFileInputStream::CopyingBaseFileInputStream::GetError()
    const {
  return file_.error_details();
}

int BaseFileInputStream::CopyingBaseFileInputStream::Read(void* buffer,
                                                          int size) {
  if (!file_.IsValid()) {
    return -1;
  }
  const std::optional<size_t> bytes_read = file_.ReadAtCurrentPos(
      UNSAFE_TODO(base::span(reinterpret_cast<uint8_t*>(buffer),
                             base::checked_cast<size_t>(size))));
  if (bytes_read) {
    return base::checked_cast<int>(*bytes_read);
  }
  file_ = base::File(base::File::GetLastFileError());
  return -1;
}

int BaseFileInputStream::CopyingBaseFileInputStream::Skip(int count) {
  if (file_.Seek(base::File::FROM_CURRENT, count) != -1) {
    return count;
  }
  return CopyingInputStream::Skip(count);
}

}  // namespace safe_browsing
