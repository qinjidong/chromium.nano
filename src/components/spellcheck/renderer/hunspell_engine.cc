// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/hunspell_engine.h"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/files/memory_mapped_file.h"
#include "base/time/time.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/local_interface_provider.h"

using content::RenderThread;

namespace {
  // Maximum length of words we actually check.
  // 64 is the observed limits for OSX system checker.
  const size_t kMaxCheckedLen = 64;

  // Maximum length of words we provide suggestions for.
  // 24 is the observed limits for OSX system checker.
  const size_t kMaxSuggestLen = 24;

  static_assert(kMaxSuggestLen <= kMaxCheckedLen,
                "MaxSuggestLen too long");
}  // namespace

HunspellEngine::HunspellEngine(
    service_manager::LocalInterfaceProvider* embedder_provider)
    : hunspell_enabled_(false),
      initialized_(false),
      dictionary_requested_(false),
      embedder_provider_(embedder_provider) {
  // Wait till we check the first word before doing any initializing.
}

HunspellEngine::~HunspellEngine() {
}

void HunspellEngine::Init(base::File file) {
  initialized_ = true;
  bdict_file_.reset();
  file_ = std::move(file);
  // Delay the actual initialization of hunspell until it is needed.
}

void HunspellEngine::InitializeHunspell() {}

bool HunspellEngine::CheckSpelling(const std::u16string& word_to_check,
                                   spellcheck::mojom::SpellCheckHost& host) {
  return true;
}

void HunspellEngine::FillSuggestionList(
    const std::u16string& wrong_word,
    spellcheck::mojom::SpellCheckHost& host,
    std::vector<std::u16string>* optional_suggestions) {}

bool HunspellEngine::InitializeIfNeeded() {
  if (!initialized_ && !dictionary_requested_) {
    mojo::Remote<spellcheck::mojom::SpellCheckInitializationHost>
        spell_check_init_host;
    embedder_provider_->GetInterface(
        spell_check_init_host.BindNewPipeAndPassReceiver());
    spell_check_init_host->RequestDictionary();
    dictionary_requested_ = true;
    return true;
  }

  // Don't initialize if hunspell is disabled.
  if (file_.IsValid())
    InitializeHunspell();

  return !initialized_;
}

bool HunspellEngine::IsEnabled() {
  return hunspell_enabled_;
}
