// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"

#include <numbers>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_math.h"
#include "components/lens/lens_features.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_polygon.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

bool ShouldDownscaleSize(const gfx::Size& size) {
  // This returns true if the area is larger than the max area AND one of the
  // width OR height exceeds the configured max values.
  return size.GetArea() > lens::features::GetLensOverlayImageMaxArea() &&
         (size.width() > lens::features::GetLensOverlayImageMaxWidth() ||
          size.height() > lens::features::GetLensOverlayImageMaxHeight());
}

double GetPreferredScale(const gfx::Size& original_size) {
  return std::min(
      base::ClampDiv(
          static_cast<double>(lens::features::GetLensOverlayImageMaxWidth()),
          original_size.width()),
      base::ClampDiv(
          static_cast<double>(lens::features::GetLensOverlayImageMaxHeight()),
          original_size.height()));
}

gfx::Size GetPreferredSize(const gfx::Size& original_size) {
  double scale = GetPreferredScale(original_size);
  int width = std::clamp<int>(scale * original_size.width(), 1,
                              lens::features::GetLensOverlayImageMaxWidth());
  int height = std::clamp<int>(scale * original_size.height(), 1,
                               lens::features::GetLensOverlayImageMaxHeight());
  return gfx::Size(width, height);
}

SkBitmap DownscaleImageIfNeeded(const SkBitmap& image) {
  auto size = gfx::Size(image.width(), image.height());
  if (ShouldDownscaleSize(size)) {
    auto preferred_size = GetPreferredSize(size);
    return skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, preferred_size.width(),
        preferred_size.height());
  }
  return image;
}

SkBitmap CropAndDownscaleImageIfNeeded(const SkBitmap& image,
                                       gfx::Rect region) {
  auto full_image_size = gfx::Size(image.width(), image.height());
  auto region_size = gfx::Size(region.width(), region.height());
  if (ShouldDownscaleSize(region_size)) {
    double scale = GetPreferredScale(region_size);
    auto downscaled_region_size = GetPreferredSize(region_size);
    int scaled_full_image_width =
        std::max<int>(scale * full_image_size.width(), 1);
    int scaled_full_image_height =
        std::max<int>(scale * full_image_size.height(), 1);
    int scaled_x = int(scale * region.x());
    int scaled_y = int(scale * region.y());

    SkIRect dest_subset = {scaled_x, scaled_y,
                           scaled_x + downscaled_region_size.width(),
                           scaled_y + downscaled_region_size.height()};
    return skia::ImageOperations::Resize(
        image, skia::ImageOperations::RESIZE_BEST, scaled_full_image_width,
        scaled_full_image_height, dest_subset);
  }

  SkIRect dest_subset = {region.x(), region.y(), region.x() + region.width(),
                         region.y() + region.height()};
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, image.width(), image.height(),
      dest_subset);
}

}  // namespace

namespace lens {

bool EncodeImage(const SkBitmap& image,
                 int compression_quality,
                 scoped_refptr<base::RefCountedBytes>* output) {
  *output = base::MakeRefCounted<base::RefCountedBytes>();
  return gfx::JPEGCodec::Encode(image, compression_quality,
                                &(*output)->as_vector());
}

lens::ImageData DownscaleAndEncodeBitmap(const SkBitmap& image) {
  lens::ImageData image_data;
  scoped_refptr<base::RefCountedBytes> data;
  auto resized_bitmap = DownscaleImageIfNeeded(image);
  if (EncodeImage(resized_bitmap,
                  lens::features::GetLensOverlayImageCompressionQuality(),
                  &data)) {
    image_data.mutable_image_metadata()->set_height(resized_bitmap.height());
    image_data.mutable_image_metadata()->set_width(resized_bitmap.width());

    image_data.mutable_payload()->mutable_image_bytes()->assign(data->begin(),
                                                                data->end());
  }
  return image_data;
}

std::optional<lens::ImageCrop> DownscaleAndEncodeBitmapRegionIfNeeded(
    const SkBitmap& image,
    lens::mojom::CenterRotatedBoxPtr region) {
  if (!region) {
    return std::nullopt;
  }

  bool use_normalized_coordinates =
      region->coordinate_type ==
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  double x_scale = use_normalized_coordinates ? image.width() : 1;
  double y_scale = use_normalized_coordinates ? image.height() : 1;
  gfx::Rect region_rect(
      static_cast<int>((region->box.x() - 0.5 * region->box.width()) * x_scale),
      static_cast<int>((region->box.y() - 0.5 * region->box.height()) *
                       y_scale),
      std::max<int>(1, region->box.width() * x_scale),
      std::max<int>(1, region->box.height() * y_scale));

  lens::ImageCrop image_crop;
  scoped_refptr<base::RefCountedBytes> data;
  auto region_bitmap = CropAndDownscaleImageIfNeeded(image, region_rect);
  if (EncodeImage(region_bitmap,
                  lens::features::GetLensOverlayImageCompressionQuality(),
                  &data)) {
    auto* mutable_zoomed_crop = image_crop.mutable_zoomed_crop();
    mutable_zoomed_crop->set_parent_height(image.height());
    mutable_zoomed_crop->set_parent_width(image.width());
    double scale = static_cast<double>(region_bitmap.width()) /
                   static_cast<double>(region_rect.width());
    mutable_zoomed_crop->set_zoom(scale);
    mutable_zoomed_crop->mutable_crop()->set_center_x(
        region_rect.CenterPoint().x());
    mutable_zoomed_crop->mutable_crop()->set_center_y(
        region_rect.CenterPoint().y());
    mutable_zoomed_crop->mutable_crop()->set_width(region_rect.width());
    mutable_zoomed_crop->mutable_crop()->set_height(region_rect.height());
    mutable_zoomed_crop->mutable_crop()->set_coordinate_type(
        lens::CoordinateType::IMAGE);

    image_crop.mutable_image()->mutable_image_content()->assign(data->begin(),
                                                                data->end());
  }
  return image_crop;
}

lens::mojom::CenterRotatedBoxPtr GetCenterRotatedBoxFromTabViewAndImageBounds(
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    gfx::Rect image_bounds) {
  // Image bounds are relative to view bounds, so create a copy of the view
  // bounds with the offset removed. Use this to clip the image bounds.
  auto view_bounds_for_clipping = gfx::Rect(view_bounds.size());
  image_bounds.Intersect(view_bounds_for_clipping);

  float left =
      static_cast<float>(view_bounds.x() + image_bounds.x() - tab_bounds.x()) /
      tab_bounds.width();
  float right = static_cast<float>(view_bounds.x() + image_bounds.x() +
                                   image_bounds.width() - tab_bounds.x()) /
                tab_bounds.width();
  float top =
      static_cast<float>(view_bounds.y() + image_bounds.y() - tab_bounds.y()) /
      tab_bounds.height();
  float bottom = static_cast<float>(view_bounds.y() + image_bounds.y() +
                                    image_bounds.height() - tab_bounds.y()) /
                 tab_bounds.height();

  // Clip to remain inside tab bounds.
  if (left < 0) {
    left = 0;
  }
  if (right > 1) {
    right = 1;
  }
  if (top < 0) {
    top = 0;
  }
  if (bottom > 1) {
    bottom = 1;
  }

  float width = right - left;
  float height = bottom - top;
  float x = (left + right) / 2;
  float y = (top + bottom) / 2;

  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(x, y, width, height);
  region->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;
  return region;
}

SkColor ExtractVibrantOrDominantColorFromImage(const SkBitmap& image,
                                               float min_population_pct) {
  if (image.empty() || image.isNull()) {
    return SK_ColorTRANSPARENT;
  }

  min_population_pct = std::clamp(min_population_pct, 0.0f, 1.0f);

  std::vector<color_utils::ColorProfile> profiles;
  // vibrant color profile
  profiles.emplace_back(color_utils::LumaRange::ANY,
                        color_utils::SaturationRange::VIBRANT);
  // any color profile
  profiles.emplace_back(color_utils::LumaRange::ANY,
                        color_utils::SaturationRange::ANY);

  auto vibrantAndDominantColors = color_utils::CalculateProminentColorsOfBitmap(
      image, profiles, /*region=*/nullptr, color_utils::ColorSwatchFilter());

  for (const auto& swatch : vibrantAndDominantColors) {
    // Valid color. Extraction failure returns 0 alpha channel.
    // Population Threshold.
    if (SkColorGetA(swatch.color) != SK_AlphaTRANSPARENT &&
        (float)swatch.population >=
            (float)(image.width() * image.height()) * min_population_pct) {
      return swatch.color;
    }
  }
  return SK_ColorTRANSPARENT;
}

std::optional<float> CalculateHueAngle(
    const std::tuple<float, float, float>& lab_color) {
  float a = std::get<1>(lab_color);
  float b = std::get<2>(lab_color);
  if (a == 0) {
    return std::nullopt;
  }
  return atan2(b, a);
}

float CalculateChroma(const std::tuple<float, float, float>& lab_color) {
  return hypotf(std::get<1>(lab_color), std::get<2>(lab_color));
}

std::optional<float> CalculateHueAngleDistance(
    const std::tuple<float, float, float>& lab_color1,
    const std::tuple<float, float, float>& lab_color2) {
  auto angle1 = CalculateHueAngle(lab_color1);
  auto angle2 = CalculateHueAngle(lab_color2);
  if (!angle1.has_value() || !angle2.has_value()) {
    return std::nullopt;
  }
  float distance = std::abs(angle1.value() - angle2.value());
  return std::min(distance, (float)(std::numbers::pi * 2.0 - distance));
}

// This conversion goes from legacy int based RGB to sRGB floats to
// XYZD50 to Lab, leveraging gfx conver_conversion functions.
std::tuple<float, float, float> ConvertColorToLab(SkColor color) {
  // Legacy RGB -> float sRGB -> XYZD50 -> LAB.
  auto [r, g, b] = gfx::SRGBLegacyToSRGB((float)SkColorGetR(color),
                                         (float)SkColorGetG(color),
                                         (float)SkColorGetB(color));
  auto [x, y, z] = gfx::SRGBToXYZD50(r, g, b);
  return gfx::XYZD50ToLab(x, y, z);
}

SkColor FindBestMatchedColorOrTransparent(
    const std::vector<SkColor>& candidate_colors,
    SkColor seed_color,
    float min_chroma) {
  if (SkColorGetA(seed_color) == SK_AlphaTRANSPARENT) {
    return SK_ColorTRANSPARENT;
  }
  if (candidate_colors.empty()) {
    return SK_ColorTRANSPARENT;
  }

  const auto& seed_lab = ConvertColorToLab(seed_color);
  // Check seed has enough chroma, calculated as hypot of a & b channels.
  if (CalculateChroma(seed_lab) < min_chroma) {
    return SK_ColorTRANSPARENT;
  }

  auto closest_color = std::min_element(
      candidate_colors.begin(), candidate_colors.end(),
      [&seed_lab](const auto& color1, const auto& color2) -> bool {
        const auto& theme1_lab = ConvertColorToLab(color1);
        const auto& theme2_lab = ConvertColorToLab(color2);
        auto angle1 = CalculateHueAngleDistance(theme1_lab, seed_lab);
        auto angle2 = CalculateHueAngleDistance(theme2_lab, seed_lab);
        return angle1.has_value() && angle2.has_value() &&
               angle1.value() < angle2.value();
      });
  if (closest_color == candidate_colors.end()) {
    return SK_ColorTRANSPARENT;
  }
  return *closest_color;
}
}  // namespace lens