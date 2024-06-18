// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.
// Inclusion of all message files present in content. Keep this file
// up to date when adding a new value to the IPCMessageStart enum in
// ipc/ipc_message_start.h to ensure the corresponding message file is
// included here.
//

#include "content/common/content_message_generator.h"
#if defined(ENABLE_PPAPI)
#undef PPAPI_PROXY_PPAPI_MESSAGES_H_
#include "ppapi/proxy/ppapi_messages.h"  // nogncheck
#ifndef PPAPI_PROXY_PPAPI_MESSAGES_H_
#error "Failed to include ppapi/proxy/ppapi_messages.h"
#endif  // defined(ENABLE_PPAPI)
#endif
