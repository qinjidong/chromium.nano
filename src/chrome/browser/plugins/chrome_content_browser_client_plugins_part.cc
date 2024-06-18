// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_content_browser_client_plugins_part.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/plugins/plugin_info_host_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#endif

#if defined(ENABLE_PPAPI)
#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#endif  // defined(ENABLE_PPAPI)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/common/webui_url_constants.h"
#endif

namespace plugins {

namespace {

void BindPluginInfoHost(
    int render_process_id,
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginInfoHost> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host)
    return;

  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<PluginInfoHostImpl>(render_process_id, profile),
      std::move(receiver));
}

}  // namespace

ChromeContentBrowserClientPluginsPart::ChromeContentBrowserClientPluginsPart() =
    default;

ChromeContentBrowserClientPluginsPart::
    ~ChromeContentBrowserClientPluginsPart() = default;

void ChromeContentBrowserClientPluginsPart::
    ExposeInterfacesToRendererForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<chrome::mojom::PluginInfoHost>(
      base::BindRepeating(&BindPluginInfoHost,
                          render_frame_host.GetProcess()->GetID()));
}

bool ChromeContentBrowserClientPluginsPart::
    IsPluginAllowedToCallRequestOSFileHandle(
        content::BrowserContext* browser_context,
        const GURL& url) {
  return true;
}

bool ChromeContentBrowserClientPluginsPart::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const content::SocketPermissionRequest* params) {
  return false;
}

bool ChromeContentBrowserClientPluginsPart::IsPepperVpnProviderAPIAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return false;

  const extensions::ExtensionSet* extension_set =
      &extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  if (!extension_set)
    return false;

  // Access to the vpnProvider API is controlled by extension permissions.
  if (url.is_valid() && url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension = extension_set->GetByID(url.host());
    if (extension) {
      if (extension->permissions_data()->HasAPIPermission(
              extensions::mojom::APIPermissionID::kVpnProvider)) {
        return true;
      }
    }
  }
#endif

  return false;
}

bool ChromeContentBrowserClientPluginsPart::IsPluginAllowedToUseDevChannelAPIs(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return true;
}

}  // namespace plugins
