// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/hash_prefix_container.h"

#include <utility>

#include "base/check.h"

namespace safe_browsing {

// static
base::FilePath HashPrefixContainer::GetPath(const base::FilePath& store_path,
                                            const std::string& extension) {
  return store_path.AddExtensionASCII(extension);
}

HashPrefixContainer::HashPrefixContainer(
    const base::FilePath& store_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t buffer_size)
    : store_path_(store_path),
      task_runner_(task_runner
                       ? std::move(task_runner)
                       : base::SequencedTaskRunner::GetCurrentDefault()),
      buffer_size_(buffer_size) {}

HashPrefixContainer::~HashPrefixContainer() {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
}

}  // namespace safe_browsing
