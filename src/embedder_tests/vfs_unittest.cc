// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "berrydb/vfs.h"

#include <cstring>
#include <random>
#include <string>

#include "gtest/gtest.h"

#include "berrydb/status.h"
#include "../test/file_deleter.h"

namespace berrydb {

class VfsTest : public ::testing::Test {
 protected:
  VfsTest() : vfs_(DefaultVfs()), file_deleter_(kFileName) { }

  const std::string kFileName = "test_vfs.berry";
  constexpr static size_t kBlockShift = 12;
  Vfs* vfs_;
  FileDeleter file_deleter_;
  std::mt19937 rnd_;
};

TEST_F(VfsTest, OpenForBlockAccessOptions) {
  BlockAccessFile* file = nullptr;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  // Setup guarantees that the file does not exist.
  ASSERT_NE(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, false, false, &file, &file_size));
  EXPECT_EQ(nullptr, file);
  EXPECT_EQ(kInvalidSize, file_size);

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, true, true, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());

  // The ASSERT above guarantees that the file was created.
  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_NE(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, true, true, &file, &file_size));
  EXPECT_EQ(nullptr, file);
  EXPECT_EQ(kInvalidSize, file_size);

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, true, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, false, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());
}

TEST_F(VfsTest, BlockAccessFilePersistence) {
  uint8_t buffer[1 << kBlockShift], read_buffer[1 << kBlockShift];
  BlockAccessFile* file = nullptr;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  for (size_t i = 0; i < 1 << kBlockShift; ++i)
    buffer[i] = static_cast<uint8_t>(rnd_());

  ASSERT_EQ(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, true, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Write(buffer, 0, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, file->Close());

  file = nullptr;
  ASSERT_EQ(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, false, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(1U << kBlockShift, file_size);
  EXPECT_EQ(Status::kSuccess, file->Read(0, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(Status::kSuccess, file->Close());

  EXPECT_EQ(0, std::memcmp(buffer, read_buffer, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, vfs_->RemoveFile(kFileName));
}

TEST_F(VfsTest, BlockAccessFileReadWriteOffsets) {
  uint8_t buffer[4][1 << kBlockShift], read_buffer[1 << kBlockShift];
  BlockAccessFile* file = nullptr;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 1 << kBlockShift; ++j)
      buffer[i][j] = static_cast<uint8_t>(rnd_());
  }

  ASSERT_EQ(Status::kSuccess, vfs_->OpenForBlockAccess(
      kFileName, kBlockShift, true, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);

  // Fill up the file with blocks [2, 1, 3, 0].
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[2], 0 << kBlockShift, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[1], 1 << kBlockShift, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[3], 2 << kBlockShift, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[0], 3 << kBlockShift, 1 << kBlockShift));

  // Read the blocks back out of order.
  EXPECT_EQ(Status::kSuccess, file->Read(
      2 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[3], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Read(
      1 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[1], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Read(
      0 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[2], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Read(
      3 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[0], read_buffer, 1 << kBlockShift));

  // Rewrite blocks 0, 2, and 3.
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[2], 2 << kBlockShift, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[0], 0 << kBlockShift, 1 << kBlockShift));
  EXPECT_EQ(Status::kSuccess, file->Write(
      buffer[3], 3 << kBlockShift, 1 << kBlockShift));

  // Read the blocks back out of order.
  EXPECT_EQ(Status::kSuccess, file->Read(
      1 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[1], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Read(
      0 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[0], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Read(
      3 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[3], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Read(
      2 << kBlockShift, 1 << kBlockShift, read_buffer));
  EXPECT_EQ(0, std::memcmp(buffer[2], read_buffer, 1 << kBlockShift));

  EXPECT_EQ(Status::kSuccess, file->Close());
  EXPECT_EQ(Status::kSuccess, vfs_->RemoveFile(kFileName));
}

TEST_F(VfsTest, OpenForRandomAccessOptions) {
  RandomAccessFile* file = nullptr;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  // Setup guarantees that the file does not exist.
  ASSERT_NE(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, false, false, &file, &file_size));
  EXPECT_EQ(nullptr, file);
  EXPECT_EQ(kInvalidSize, file_size);

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, true, true, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());

  // The ASSERT above guarantees that the file was created.
  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_NE(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, true, true, &file, &file_size));
  EXPECT_EQ(nullptr, file);
  EXPECT_EQ(kInvalidSize, file_size);

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, true, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, false, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());
}

TEST_F(VfsTest, RandomAccessFilePersistence) {
  uint8_t buffer[9000], read_buffer[9000];
  RandomAccessFile* file = nullptr;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  for (size_t i = 0; i < sizeof(buffer); ++i)
    buffer[i] = static_cast<uint8_t>(rnd_());

  ASSERT_EQ(Status::kSuccess, vfs_->OpenForRandomAccess(
      kFileName, true, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Write(buffer +    0,    0, 2000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 2000, 2000, 1000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 3000, 3000, 3000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 6000, 6000,  500));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 6500, 6500, 2500));
  EXPECT_EQ(Status::kSuccess, file->Close());

  file = nullptr;
  file_size = kInvalidSize;
  ASSERT_EQ(Status::kSuccess, vfs_->OpenForRandomAccess(
      kFileName, false, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(9000U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Read(   0, 2500, read_buffer +    0));
  EXPECT_EQ(Status::kSuccess, file->Read(2500,  500, read_buffer + 2500));
  EXPECT_EQ(Status::kSuccess, file->Read(3000, 3000, read_buffer + 3000));
  EXPECT_EQ(Status::kSuccess, file->Read(6000, 1000, read_buffer + 6000));
  EXPECT_EQ(Status::kSuccess, file->Read(7000, 2000, read_buffer + 7000));
  EXPECT_EQ(Status::kSuccess, file->Close());

  static_assert(
      sizeof(buffer) == sizeof(read_buffer), "Mismatched buffer size");
  EXPECT_EQ(0, std::memcmp(buffer, read_buffer, sizeof(buffer)));
  EXPECT_EQ(Status::kSuccess, vfs_->RemoveFile(kFileName));
}

TEST_F(VfsTest, RandomAccessFileReadWriteOffsets) {
  uint8_t buffer[9000], read_buffer[9000];
  RandomAccessFile* file;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  for (size_t i = 0; i < sizeof(buffer); ++i)
    buffer[i] = static_cast<uint8_t>(rnd_());

  ASSERT_EQ(Status::kSuccess, vfs_->OpenForRandomAccess(
      kFileName, true, false, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);

  // Write the data in order.
  EXPECT_EQ(Status::kSuccess, file->Write(buffer +    0,    0, 2000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 2000, 2000, 1000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 3000, 3000, 3000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 6000, 6000,  500));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 6500, 6500, 2500));

  // Read the data out of order.
  EXPECT_EQ(Status::kSuccess, file->Read(3000, 3000, read_buffer + 3000));
  EXPECT_EQ(Status::kSuccess, file->Read(7000, 2000, read_buffer + 7000));
  EXPECT_EQ(Status::kSuccess, file->Read(   0, 2500, read_buffer +    0));
  EXPECT_EQ(Status::kSuccess, file->Read(6000, 1000, read_buffer + 6000));
  EXPECT_EQ(Status::kSuccess, file->Read(2500,  500, read_buffer + 2500));

  static_assert(
      sizeof(buffer) == sizeof(read_buffer), "Mismatched buffer size");
  EXPECT_EQ(0, std::memcmp(buffer, read_buffer, sizeof(buffer)));

  // Reset the data.
  for (size_t i = 0; i < sizeof(buffer); ++i)
    buffer[i] = static_cast<uint8_t>(rnd_());

  // Write the new data out of order.
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 3000, 3000, 3000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 6000, 6000,  500));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer +    0,    0, 2000));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 6500, 6500, 2500));
  EXPECT_EQ(Status::kSuccess, file->Write(buffer + 2000, 2000, 1000));

  // Read the new data out of order.
  EXPECT_EQ(Status::kSuccess, file->Read(6000, 1000, read_buffer + 6000));
  EXPECT_EQ(Status::kSuccess, file->Read(3000, 3000, read_buffer + 3000));
  EXPECT_EQ(Status::kSuccess, file->Read(   0, 2500, read_buffer +    0));
  EXPECT_EQ(Status::kSuccess, file->Read(7000, 2000, read_buffer + 7000));
  EXPECT_EQ(Status::kSuccess, file->Read(2500,  500, read_buffer + 2500));

  EXPECT_EQ(0, std::memcmp(buffer, read_buffer, sizeof(buffer)));

  EXPECT_EQ(Status::kSuccess, file->Close());
  EXPECT_EQ(Status::kSuccess, vfs_->RemoveFile(kFileName));
}

TEST_F(VfsTest, RemoveFile) {
  RandomAccessFile* file = nullptr;
  const size_t kInvalidSize = 0x0badc0de;
  size_t file_size = kInvalidSize;

  ASSERT_EQ(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, true, true, &file, &file_size));
  ASSERT_NE(nullptr, file);
  EXPECT_EQ(0U, file_size);
  EXPECT_EQ(Status::kSuccess, file->Close());

  ASSERT_EQ(Status::kSuccess, vfs_->RemoveFile(kFileName));

  ASSERT_NE(
      Status::kSuccess,
      vfs_->OpenForRandomAccess(kFileName, false, false, &file, &file_size));
}

}  // namespace berrydb
