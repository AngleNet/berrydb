// Copyright 2017 The BerryDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "./block_access_file_wrapper.h"

#include "berrydb/platform.h"
#include "berrydb/status.h"

namespace berrydb {

BlockAccessFileWrapper::BlockAccessFileWrapper(BlockAccessFile* file)
    : file_(file), access_error_(Status::kSuccess) { }

BlockAccessFileWrapper::~BlockAccessFileWrapper() {
  if (!wrapped_file_is_closed_)
    file_->Close();
}

Status BlockAccessFileWrapper::Read(
    size_t offset, size_t byte_count, uint8_t* buffer) {
  DCHECK(!is_closed_);
  if (access_error_ != Status::kSuccess)
    return access_error_;
  return file_->Read(offset, byte_count, buffer);
}

Status BlockAccessFileWrapper::Write(
    uint8_t* buffer, size_t offset, size_t byte_count) {
  DCHECK(!is_closed_);
  if (access_error_ != Status::kSuccess)
    return access_error_;
  return file_->Write(buffer, offset, byte_count);
}

Status BlockAccessFileWrapper::Sync() {
  DCHECK(!is_closed_);
  if (access_error_ != Status::kSuccess)
    return access_error_;
  return file_->Sync();
}

Status BlockAccessFileWrapper::Lock() {
  DCHECK(!is_closed_);
  if (access_error_ != Status::kSuccess)
    return access_error_;
  return file_->Lock();
}


Status BlockAccessFileWrapper::Close() {
  DCHECK(!is_closed_);
  is_closed_ = true;

  if (access_error_ != Status::kSuccess)
    return access_error_;

  wrapped_file_is_closed_ = true;
  return file_->Close();
}

}  // namespace berrydb
