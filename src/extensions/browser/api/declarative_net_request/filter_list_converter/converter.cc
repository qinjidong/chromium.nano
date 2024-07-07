// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/filter_list_converter/converter.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/indexed_rule.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "url/gurl.h"

namespace extensions::declarative_net_request {

namespace filter_list_converter {

bool ConvertRuleset(const std::vector<base::FilePath>& filter_list_inputs,
                    const base::FilePath& output_path,
                    WriteType type,
                    bool noisy) {
  return true;
}

}  // namespace filter_list_converter
}  // namespace extensions::declarative_net_request
