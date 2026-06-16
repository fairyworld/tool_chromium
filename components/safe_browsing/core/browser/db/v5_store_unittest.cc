// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v5_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/common/proto/v5_store.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class V5StoreTest : public PlatformTest {
 public:
  V5StoreTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_path_ = temp_dir_.GetPath().AppendASCII("V5StoreTest.store");
    DVLOG(1) << "store_path_: " << store_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(store_path_);
    PlatformTest::TearDown();
  }

  void WriteFileFormatProtoToFile(uint32_t magic,
                                  uint32_t file_version = 0,
                                  ListDetails* details = nullptr) {
    V5StoreFileFormat file_format;
    WriteFileFormatProtoToFile(&file_format, magic, file_version, details);
  }

  void WriteFileFormatProtoToFile(V5StoreFileFormat* file_format,
                                  uint32_t magic,
                                  uint32_t file_version,
                                  ListDetails* details) {
    file_format->set_magic_number(magic);
    file_format->set_file_version(file_version);
    if (details != nullptr) {
      ListDetails* list_details = file_format->mutable_list_details();
      *list_details = *details;
    }

    std::string file_format_string;
    file_format->SerializeToString(&file_format_string);
    base::WriteFile(store_path_, file_format_string);
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner() {
    return base::SequencedTaskRunner::GetCurrentDefault();
  }

  V5StoreReadResult ReadFromDisk(V5Store& store) {
    return store.ReadFromDisk();
  }

  const HashPrefixList& GetHashPrefixList(const V5Store& store) {
    return *store.hash_prefix_list_;
  }

  int64_t GetFileSize(const V5Store& store) { return store.file_size_; }

  std::string GetExpectedChecksum(const V5Store& store) {
    return store.expected_checksum_;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath store_path_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(V5StoreTest, TestReadFromEmptyFile) {
  base::CloseFile(base::OpenFile(store_path_, "wb+"));

  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kFileEmptyFailure, ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromAbsentFile) {
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kFileOpenFailure, ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromInvalidContentsFile) {
  const char kInvalidContents[] = "Chromium";
  base::WriteFile(store_path_, kInvalidContents);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kProtoParsingFailure, ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromFileWithUnknownProto) {
  Checksum checksum;
  checksum.set_sha256("checksum");
  std::string checksum_string;
  checksum.SerializeToString(&checksum_string);
  base::WriteFile(store_path_, checksum_string);

  // Even though we wrote a completely different proto to file, the proto
  // parsing method does not fail. This shows the importance of a magic number.
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kUnexpectedMagicNumberFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromUnexpectedMagicFile) {
  WriteFileFormatProtoToFile(111);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kUnexpectedMagicNumberFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromLowVersionFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 2);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kFileVersionIncompatibleFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromNoHashPrefixInfoFile) {
  WriteFileFormatProtoToFile(0x600D71FE, 10);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kHashPrefixInfoMissingFailure,
            ReadFromDisk(store));
}

TEST_F(V5StoreTest, TestReadFromNoHashPrefixesFile) {
  base::HistogramTester histogram_tester;
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_TRUE(GetHashPrefixList(store).view().empty());
  EXPECT_EQ(24, GetFileSize(store));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kSuccess, 1);
}

TEST_F(V5StoreTest, TestReadFromInvalidHashPrefixList) {
  base::HistogramTester histogram_tester;
  // Manually create an invalid store on disk
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_client_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  // Set file size to 6, which is not a multiple of 4.
  hash_file->set_file_size(6);
  // Write the file format and hash file to disk.
  base::WriteFile(store_path_, file_format.SerializeAsString());
  base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcdef");
  // Set the prefix size to 4. This will cause a failure in the read.
  V5Store read_store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kHashPrefixListGenerationFailure,
            ReadFromDisk(read_store));
  EXPECT_TRUE(read_store.version().empty());
  EXPECT_TRUE(GetHashPrefixList(read_store).view().empty());
  EXPECT_EQ(0, GetFileSize(read_store));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kFileSizeNotMultipleOfPrefixSize, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kFileSizeNotMultipleOfPrefixSize, 1);
}

TEST_F(V5StoreTest, TestReadWithMissingHashFile) {
  base::HistogramTester histogram_tester;
  V5StoreFileFormat file_format;
  ListDetails list_details;
  list_details.set_version("test_client_version");
  V5HashFile* hash_file = list_details.mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);
  // Write only the file format to disk. The hash file is missing.
  WriteFileFormatProtoToFile(&file_format, 0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);

  EXPECT_EQ(V5StoreReadResult::kHashPrefixListGenerationFailure,
            ReadFromDisk(store));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kMmapFailure, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kMmapFailure, 1);
}

TEST_F(V5StoreTest, TestInitializeSucceeds) {
  base::HistogramTester histogram_tester;
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());

  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5StoreRead.Result",
                                      V5StoreReadResult::kReadSuccess, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest", true, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStoreRead.Success", true,
                                      1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", true, 1);
}

TEST_F(V5StoreTest, TestInitializeSucceedsWithV5Suffix) {
  base::HistogramTester histogram_tester;
  base::FilePath v5_store_path =
      temp_dir_.GetPath().AppendASCII("V5StoreTest_v5.store");

  ListDetails list_details;
  list_details.set_version("test_version");

  // Temporarily swap store_path_ to use the helper
  base::FilePath original_store_path = store_path_;
  store_path_ = v5_store_path;
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  store_path_ = original_store_path;  // restore

  V5Store store(task_runner(), v5_store_path, 4);
  store.Initialize();
  EXPECT_TRUE(store.HasValidData());

  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5StoreRead.Result",
                                      V5StoreReadResult::kReadSuccess, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest_v5", true, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStoreRead.Success", true,
                                      1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", true, 1);

  base::DeleteFile(v5_store_path);
}

TEST_F(V5StoreTest, TestInitializeFails) {
  base::HistogramTester histogram_tester;
  // No file on disk, so Initialize will fail.
  V5Store store(task_runner(), store_path_, 4);
  store.Initialize();
  EXPECT_FALSE(store.HasValidData());

  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5StoreRead.Result",
                                      V5StoreReadResult::kFileOpenFailure, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest", false, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStoreRead.Success", false,
                                      1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", false, 1);
}

TEST_F(V5StoreTest, TestReadFromDiskDoesNotSetValidData) {
  base::HistogramTester histogram_tester;
  ListDetails list_details;
  list_details.set_version("test_version");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  // Only `Initialize()` sets the `has_valid_data_` property.
  EXPECT_FALSE(store.HasValidData());

  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5Store.IsStoreValid.V5StoreTest", false, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.SBStore.IsStoreValid.V5StoreTest", false, 1);
  histogram_tester.ExpectTotalCount("SafeBrowsing.V5StoreRead.Result", 0);
  histogram_tester.ExpectTotalCount("SafeBrowsing.SBStoreRead.Success", 0);
}

TEST_F(V5StoreTest, TestHasValidDataLoggingHeuristic) {
  base::HistogramTester histogram_tester;
  V5Store store(task_runner(), store_path_, 4);

  // First call should log.
  store.HasValidData();
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                      false, 1);

  // Calls 2 to 256 should NOT log.
  for (int i = 2; i <= 256; ++i) {
    store.HasValidData();
  }
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                      false, 1);

  // Next call should log again (total 2 samples).
  store.HasValidData();
  histogram_tester.ExpectUniqueSample("SafeBrowsing.V5Store.IsStoreValid",
                                      false, 2);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.SBStore.IsStoreValid",
                                      false, 2);
}

TEST_F(V5StoreTest, TestReadFromNoVersionFile) {
  ListDetails list_details;
  // `version` is omitted.
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_TRUE(store.version().empty());
}

TEST_F(V5StoreTest, TestReadWithValidChecksum) {
  ListDetails list_details;
  list_details.set_version("test_version");
  list_details.mutable_checksum()->set_sha256(
      "test_checksum_value_32_bytes____");
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_EQ("test_checksum_value_32_bytes____", GetExpectedChecksum(store));
}

TEST_F(V5StoreTest, TestReadWithMissingSha256) {
  ListDetails list_details;
  list_details.set_version("test_version");
  list_details.mutable_checksum();  // checksum is present but empty
  WriteFileFormatProtoToFile(0x600D71FE, 10, &list_details);
  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_TRUE(GetExpectedChecksum(store).empty());
}

TEST_F(V5StoreTest, TestReadWithValidHashPrefixList) {
  base::HistogramTester histogram_tester;
  V5StoreFileFormat file_format;
  file_format.set_magic_number(0x600D71FE);
  file_format.set_file_version(10);
  ListDetails* list_details = file_format.mutable_list_details();
  list_details->set_version("test_version");
  V5HashFile* hash_file = list_details->mutable_hash_file();
  hash_file->set_extension("foo");
  hash_file->set_file_size(4);

  // Write main proto.
  base::WriteFile(store_path_, file_format.SerializeAsString());
  // Write valid hash file (4 bytes).
  base::WriteFile(store_path_.AddExtensionASCII("foo"), "abcd");

  V5Store store(task_runner(), store_path_, 4);
  EXPECT_EQ(V5StoreReadResult::kReadSuccess, ReadFromDisk(store));
  EXPECT_EQ("test_version", store.version());
  EXPECT_EQ(GetHashPrefixList(store).view().at(4), "abcd");
  EXPECT_EQ(base::checked_cast<int64_t>(file_format.ByteSizeLong() + 4),
            GetFileSize(store));

  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result",
      V5ApplyUpdateResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.V5ReadFromDisk.ApplyUpdate.Result.V5StoreTest",
      V5ApplyUpdateResult::kSuccess, 1);
}

}  // namespace safe_browsing
