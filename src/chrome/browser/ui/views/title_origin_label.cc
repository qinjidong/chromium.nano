// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/title_origin_label.h"

#include "ui/base/ui_base_features.h"

std::unique_ptr<views::Label> CreateTitleOriginLabel(
    const std::u16string& text) {
  auto label =
      std::make_unique<views::Label>(text, views::style::CONTEXT_DIALOG_TITLE);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetCollapseWhenHidden(true);

  // Show the full origin in multiple lines. Breaking characters in the middle
  // of a word are explicitly allowed, as long origins are treated as one word.
  // Note that in English, GetWindowTitle() returns a string "$ORIGIN wants to".
  // In other languages, the non-origin part may appear before the
  // origin (e.g., in Filipino, "Gusto ng $ORIGIN na"). See crbug.com/40095827.
  label->SetElideBehavior(gfx::NO_ELIDE);
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);

  if (features::IsChromeRefresh2023()) {
    label->SetTextStyle(views::style::STYLE_HEADLINE_4);
  }

  return label;
}
