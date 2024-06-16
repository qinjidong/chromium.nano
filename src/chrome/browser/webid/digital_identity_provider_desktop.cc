// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/digital_identity_provider_desktop.h"

#include <memory>

#include "base/containers/span.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/digital_identity_provider.h"
#include "crypto/random.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_handshake.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

DigitalIdentityProviderDesktop::DigitalIdentityProviderDesktop() = default;
DigitalIdentityProviderDesktop::~DigitalIdentityProviderDesktop() = default;

void DigitalIdentityProviderDesktop::Request(content::WebContents* web_contents,
                                             const url::Origin& rp_origin,
                                             const std::string& request,
                                             DigitalIdentityCallback callback) {
  callback_ = std::move(callback);
}

void DigitalIdentityProviderDesktop::ShowQrCodeDialog(
    content::WebContents* web_contents,
    const url::Origin& rp_origin,
    const std::string& qr_url) {}

void DigitalIdentityProviderDesktop::OnQrCodeDialogCanceled() {
  if (callback_.is_null()) {
    return;
  }

  std::move(callback_).Run(
      base::unexpected(RequestStatusForMetrics::kErrorOther));
}
