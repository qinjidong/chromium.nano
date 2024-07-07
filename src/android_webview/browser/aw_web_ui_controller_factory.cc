// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_web_ui_controller_factory.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

using content::WebUI;
using content::WebUIController;

namespace {

// A function for creating a new WebUI. The caller owns the return value, which
// may be nullptr (for example, if the URL refers to an non-existent extension).
typedef WebUIController* (*WebUIFactoryFunctionPointer)(WebUI* web_ui,
                                                        const GURL& url);

// Template for handlers defined in a component layer, that take an instance of
// a delegate implemented in the android_webview layer.
template <class WEB_UI_CONTROLLER, class DELEGATE>
WebUIController* NewComponentUI(WebUI* web_ui, const GURL& url) {
  auto delegate = std::make_unique<DELEGATE>(web_ui);
  return new WEB_UI_CONTROLLER(web_ui, std::move(delegate));
}

WebUIFactoryFunctionPointer GetWebUIFactoryFunctionPointer(const GURL& url) {
  return nullptr;
}

WebUI::TypeID GetWebUITypeID(const GURL& url) {
  return WebUI::kNoWebUI;
}

}  // namespace

namespace android_webview {

// static
AwWebUIControllerFactory* AwWebUIControllerFactory::GetInstance() {
  return base::Singleton<AwWebUIControllerFactory>::get();
}

AwWebUIControllerFactory::AwWebUIControllerFactory() {}

AwWebUIControllerFactory::~AwWebUIControllerFactory() {}

WebUI::TypeID AwWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUITypeID(url);
}

bool AwWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

std::unique_ptr<WebUIController>
AwWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                      const GURL& url) {
  WebUIFactoryFunctionPointer function = GetWebUIFactoryFunctionPointer(url);
  if (!function)
    return nullptr;

  return base::WrapUnique((*function)(web_ui, url));
}

}  // namespace android_webview
