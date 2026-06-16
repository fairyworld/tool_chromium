// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_

#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
namespace safe_browsing {

// A ZeroCopyInputStream that reads from a file using base::File. Any errors
// during deserialization close the file.
class BaseFileInputStream : public google::protobuf::io::ZeroCopyInputStream {
 public:
  // Creates and opens `input_file`.
  explicit BaseFileInputStream(const base::FilePath& input_file);
  BaseFileInputStream(const BaseFileInputStream&) = delete;
  BaseFileInputStream& operator=(const BaseFileInputStream&) = delete;

  // Closes the file, if it was still open.
  ~BaseFileInputStream() override;

  // Returns `base::File::FILE_OK` if no error and the file is still open; else
  // the error that led to closure of the file.
  base::File::Error GetError() const;

  // google::protobuf::io::ZeroCopyInputStream:
  bool Next(const void** data, int* size) override;
  void BackUp(int count) override;
  bool Skip(int count) override;
  int64_t ByteCount() const override;

 private:
  class CopyingBaseFileInputStream
      : public google::protobuf::io::CopyingInputStream {
   public:
    explicit CopyingBaseFileInputStream(const base::FilePath& input_file);
    CopyingBaseFileInputStream(const CopyingBaseFileInputStream&) = delete;
    CopyingBaseFileInputStream& operator=(const CopyingBaseFileInputStream&) =
        delete;
    ~CopyingBaseFileInputStream() override;

    base::File::Error GetError() const;

    // google::protobuf::io::CopyingInputStream:
    int Read(void* buffer, int size) override;
    int Skip(int count) override;

   private:
    base::File file_;
  };

  CopyingBaseFileInputStream stream_;
  google::protobuf::io::CopyingInputStreamAdaptor impl_;
};

// The base class for the Safe Browsing V4 and V5 stores.
class SBStore {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file. The constructor doesn't read the store file from disk.
  // If the store is being created to apply an update to the old store, then
  // |old_file_size| is the size of the existing file on disk for this store;
  // 0 otherwise. This is needed so that we can correctly report the size of
  // store file on disk, even if writing the new file fails after successfully
  // applying an update.
  SBStore(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path,
          int64_t old_file_size = 0);
  virtual ~SBStore();

  // True if this store has valid contents, either from a successful read
  // from disk or a full update.  This does not mean the checksum was verified.
  virtual bool HasValidData();

  const base::FilePath& store_path() const { return store_path_; }

  int64_t file_size() const { return file_size_; }

 protected:
  static constexpr uint32_t kFileMagic = 0x600D71FE;

  virtual std::string GetMetricPrefix() const = 0;

  // The size of the file on disk for this store.
  int64_t file_size_;

  // True if the file was successfully read+parsed or was populated from
  // a full update.
  bool has_valid_data_;

  const base::FilePath store_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

 private:
  static void RecordBooleanWithAndWithoutSuffix(const std::string& metric,
                                                bool value,
                                                const std::string& suffix);

  void LogHasValidDataHistograms();

  // A counter used to manage how frequently the value of `has_valid_data_`
  // below is recorded.
  uint8_t record_has_valid_data_counter_ = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_SB_STORE_H_
