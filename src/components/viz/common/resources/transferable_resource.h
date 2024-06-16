// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace gpu {
class ClientSharedImage;
}

namespace viz {

using MemoryBufferId = absl::variant<gpu::Mailbox, SharedBitmapId>;

struct ReturnedResource;

struct VIZ_COMMON_EXPORT TransferableResource {
  enum class SynchronizationType : uint8_t {
    // Commands issued (SyncToken) - a resource can be reused as soon as display
    // compositor issues the latest command on it and SyncToken will be signaled
    // when this happens.
    kSyncToken = 0,
    // Commands completed (aka read lock fence) - If a gpu resource is backed by
    // a GpuMemoryBuffer, then it will be accessed out-of-band, and a gpu fence
    // needs to be waited on before the resource is returned and reused. In
    // other words, the resource will be returned only when gpu commands are
    // completed.
    kGpuCommandsCompleted,
    // Commands submitted (release fence) - a resource will be returned after
    // gpu service submitted commands to the gpu and provide the fence.
    kReleaseFence,
  };

  // Differentiates between the various sources that create a resource. They
  // have different lifetime expectations, and we want to be able to determine
  // which remain after we Evict a Surface.
  //
  // These values are persistent to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ResourceSource : uint8_t {
    kUnknown = 0,
    kAR = 1,
    kCanvas = 2,
    kDrawingBuffer = 3,
    kExoBuffer = 4,
    kHeadsUpDisplay = 5,
    kImageLayerBridge = 6,
    kPPBGraphics3D = 7,
    kPepperGraphics2D = 8,
    kSharedElementTransition = 9,
    kStaleContent = 10,
    kTest = 11,
    kTileRasterTask = 12,
    kUI = 13,
    kVideo = 14,
    kWebGPUSwapBuffer = 15,
  };

  static TransferableResource MakeSoftwareSharedBitmap(
      const SharedBitmapId& id,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      ResourceSource source = ResourceSource::kUnknown);
  static TransferableResource MakeSoftwareSharedImage(
      const scoped_refptr<gpu::ClientSharedImage>& client_shared_image,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      ResourceSource source = ResourceSource::kUnknown);
  static TransferableResource MakeGpu(
      const gpu::Mailbox& mailbox,
      uint32_t texture_target,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      bool is_overlay_candidate,
      ResourceSource source = ResourceSource::kUnknown);
  static TransferableResource MakeGpu(
      const scoped_refptr<gpu::ClientSharedImage>& client_shared_image,
      uint32_t texture_target,
      const gpu::SyncToken& sync_token,
      const gfx::Size& size,
      SharedImageFormat format,
      bool is_overlay_candidate,
      ResourceSource source = ResourceSource::kUnknown);

  TransferableResource();
  ~TransferableResource();

  TransferableResource(const TransferableResource& other);
  TransferableResource& operator=(const TransferableResource& other);

  ReturnedResource ToReturnedResource() const;
  static std::vector<ReturnedResource> ReturnResources(
      const std::vector<TransferableResource>& input);
  bool is_empty() const {
    return (absl::holds_alternative<gpu::Mailbox>(memory_buffer_id_) &&
            mailbox().IsZero()) ||
           (absl::holds_alternative<SharedBitmapId>(memory_buffer_id_) &&
            shared_bitmap_id().IsZero());
  }

  // Returns true if this resource (which must be software) is holding a
  // SharedImage ID rather than a SharedBitmapId.
  bool IsSoftwareSharedImage() const;

  // TODO(danakj): Some of these fields are only GL, some are only Software,
  // some are both but used for different purposes (like the mailbox name).
  // It would be nice to group things together and make it more clear when
  // they will be used or not, and provide easier access to fields such as the
  // mailbox that also show the intent for software for GL.
  // An |id| field that can be unique to this resource. For resources
  // generated by compositor clients, this |id| may be used for their
  // own book-keeping but need not be set at all.
  ResourceId id = kInvalidResourceId;

  // Indicates if the resource is gpu or software backed.
  bool is_software = false;

  // The number of pixels in the gpu mailbox/software bitmap.
  gfx::Size size;

  // The format of the pixels in the gpu mailbox/software bitmap. This should
  // almost always be RGBA_8888 for resources generated by compositor clients,
  // and must be RGBA_8888 always for software resources.
  SharedImageFormat format = SinglePlaneFormat::kRGBA_8888;

  void set_mailbox(const gpu::Mailbox& mailbox) { memory_buffer_id_ = mailbox; }

  void set_shared_bitmap_id(const SharedBitmapId& shared_bitmap_id) {
    memory_buffer_id_ = shared_bitmap_id;
  }
  void set_sync_token(const gpu::SyncToken& sync_token) {
    sync_token_ = sync_token;
  }
  void set_texture_target(const uint32_t texture_target) {
    texture_target_ = texture_target;
  }

  // Returns the Mailbox or SharedBitmapId that this instance is storing.
  const gpu::Mailbox& mailbox() const {
    bool holds_mailbox =
        absl::holds_alternative<gpu::Mailbox>(memory_buffer_id_);
    DUMP_WILL_BE_CHECK(holds_mailbox);
    // TODO(crbug.com/337538024): Remove this escape hatch post safe rollout of
    // the above.
    if (!holds_mailbox) {
      return empty_mailbox_;
    }

    return absl::get<gpu::Mailbox>(memory_buffer_id_);
  }
  const SharedBitmapId& shared_bitmap_id() const {
    return absl::get<SharedBitmapId>(memory_buffer_id_);
  }
  const gpu::SyncToken& sync_token() const { return sync_token_; }
  gpu::SyncToken& mutable_sync_token() { return sync_token_; }
  uint32_t texture_target() const { return texture_target_; }

  // The color space that is used for pixel path operations (e.g, TexImage,
  // CopyTexImage, DrawPixels) and when displaying as an overlay.
  //
  // TODO(b/220336463): On ChromeOS, the color space for hardware decoded video
  // frames is currently specified at the time of creating the SharedImage.
  // Therefore, for the purposes of that use case and compositing, the
  // |color_space| field here is ignored. We should consider using it.
  //
  // TODO(b/233667677): For ChromeOS NV12 hardware overlays, |color_space| is
  // only used for deciding if an NV12 resource should be promoted to a hardware
  // overlay. Instead, we should plumb this information to DRM/KMS so that if
  // the resource does get promoted to overlay, the display controller knows how
  // to perform the YUV-to-RGB conversion.
  gfx::ColorSpace color_space;
  gfx::HDRMetadata hdr_metadata;

  // A gpu resource may be possible to use directly in an overlay if this is
  // true.
  bool is_overlay_candidate = false;

  // This defines when the display compositor returns resources. Clients may use
  // different synchronization types based on their needs.
  SynchronizationType synchronization_type = SynchronizationType::kSyncToken;

  // YCbCr info for resources backed by YCbCr Vulkan images.
  std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;

#if BUILDFLAG(IS_ANDROID)
  // Indicates whether this resource may not be overlayed on Android, since
  // it's not backed by a SurfaceView.  This may be set in combination with
  // |is_overlay_candidate|, to find out if switching the resource to a
  // a SurfaceView would result in overlay promotion.  It's good to find this
  // out in advance, since one has no fallback path for displaying a
  // SurfaceView except via promoting it to an overlay.  Ideally, one _could_
  // promote SurfaceTexture via the overlay path, even if one ended up just
  // drawing a quad in the compositor.  However, for now, we use this flag to
  // refuse to promote so that the compositor will draw the quad.
  bool is_backed_by_surface_texture = false;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  // Indicates that this resource would like a promotion hint.
  bool wants_promotion_hint = false;
#endif

  // If true, we need to run a detiling image processor on the quad before we
  // can scan it out.
  bool needs_detiling = false;

  // The source that originally allocated this resource. For determining which
  // sources are maintaining lifetime after surface eviction.
  ResourceSource resource_source = ResourceSource::kUnknown;

  bool operator==(const TransferableResource& o) const {
    return id == o.id && is_software == o.is_software && size == o.size &&
           format == o.format && memory_buffer_id_ == o.memory_buffer_id_ &&
           sync_token_ == o.sync_token_ &&
           texture_target_ == o.texture_target_ &&
           color_space == o.color_space && hdr_metadata == o.hdr_metadata &&
           is_overlay_candidate == o.is_overlay_candidate &&
#if BUILDFLAG(IS_ANDROID)
           is_backed_by_surface_texture == o.is_backed_by_surface_texture &&
           wants_promotion_hint == o.wants_promotion_hint &&
#elif BUILDFLAG(IS_WIN)
           wants_promotion_hint == o.wants_promotion_hint &&
#endif
           synchronization_type == o.synchronization_type &&
           resource_source == o.resource_source;
  }
  bool operator!=(const TransferableResource& o) const { return !(*this == o); }

  // For usage only in Mojo serialization/deserialization.
  const MemoryBufferId& memory_buffer_id() const { return memory_buffer_id_; }
  void set_memory_buffer_id(MemoryBufferId memory_buffer_id) {
    memory_buffer_id_ = memory_buffer_id;
  }

 private:
  MemoryBufferId memory_buffer_id_;

  // TODO(crbug.com/337538024): Remove once DUMP_WILL_BE_CHECK() in
  // TransferableResource::mailbox() has safely rolled out.
  gpu::Mailbox empty_mailbox_;

  // The SyncToken associated with the above buffer. Allows the receiver to wait
  // until the producer has finished using the texture before it begins using
  // the texture.
  gpu::SyncToken sync_token_;

  // When the shared memory buffer is backed by a GPU texture, the
  // `texture_target` is that texture's type.
  // See here for OpenGL texture types:
  // https://www.opengl.org/wiki/Texture#Texture_Objects
  uint32_t texture_target_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_