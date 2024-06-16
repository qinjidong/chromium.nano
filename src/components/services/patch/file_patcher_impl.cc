// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/patch/file_patcher_impl.h"

#include <utility>

#include "base/functional/callback.h"

namespace patch {

FilePatcherImpl::FilePatcherImpl() = default;

FilePatcherImpl::FilePatcherImpl(
    mojo::PendingReceiver<mojom::FilePatcher> receiver)
    : receiver_(this, std::move(receiver)) {}

FilePatcherImpl::~FilePatcherImpl() = default;

void FilePatcherImpl::PatchFilePuffPatch(base::File input_file,
                                         base::File patch_file,
                                         base::File output_file,
                                         PatchFilePuffPatchCallback callback) {
  std::move(callback).Run(0);
}

}  // namespace patch
