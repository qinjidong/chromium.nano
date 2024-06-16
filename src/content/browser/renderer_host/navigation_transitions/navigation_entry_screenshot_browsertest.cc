// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"

#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_transition_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/render_document_feature.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "url/url_constants.h"

namespace content {

namespace {

// A test-only alternative to
// `NavigationEntryScreenshotCache::RemoveScreenshot` that does not evict the
// cached screenshot.
NavigationEntryScreenshot* PreviewScreenshotForEntry(NavigationEntry* entry) {
  EXPECT_TRUE(entry);
  auto* data = entry->GetUserData(NavigationEntryScreenshot::kUserDataKey);
  if (!data) {
    return nullptr;
  }
  return static_cast<NavigationEntryScreenshot*>(data);
}

// Navigates the current tab to `destination`, and:
// - Makes sure the current tab is screenshotted, and the screenshot is stored
//   inside the correct `NavigationEntry`.
// - Makes sure `destination` has reached a steady state in the renderer, so
//   that its surface can be copied.
void NavigateTabAndWaitForScreenshotCached(WebContents* tab,
                                           NavigationControllerImpl& controller,
                                           const GURL& destination,
                                           bool same_doc_nav = false) {
  const int num_request_before_nav =
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting();
  const int entries_count_before_nav = controller.GetEntryCount();
  ScopedScreenshotCapturedObserverForTesting observer(
      controller.GetLastCommittedEntryIndex());
  ASSERT_TRUE(NavigateToURL(tab, destination));
  // We don't need to wait for the same-doc navigations. When the browser
  // receives the screenshot for same-doc navigations, the renderer must have
  // submitted a new frame. Using the observer on screenshot capture is enough.
  if (!same_doc_nav) {
    WaitForCopyableViewInWebContents(tab);
  }
  observer.Wait();
  ASSERT_EQ(controller.GetEntryCount(), entries_count_before_nav + 1);
  ASSERT_EQ(
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(),
      num_request_before_nav + 1);
}

// Identical functionalities as `NavigateTabAndWaitForScreenshotCached`, except
// for a history navigation.
void HistoryNavigateTabAndWaitForScreenshotCached(
    WebContents* tab,
    NavigationControllerImpl& controller,
    int offset,
    bool same_doc_nav = false) {
  const int num_request_before_nav =
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting();
  const int entries_count_before_nav = controller.GetEntryCount();
  ScopedScreenshotCapturedObserverForTesting observer(
      controller.GetLastCommittedEntryIndex());
  ASSERT_TRUE(HistoryGoToOffset(tab, offset));
  if (!same_doc_nav) {
    WaitForCopyableViewInWebContents(tab);
  }
  observer.Wait();
  ASSERT_EQ(controller.GetEntryCount(), entries_count_before_nav);
  ASSERT_EQ(
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(),
      num_request_before_nav + 1);
}

struct ScreenshotCaptureTestNavigationType {
  bool same_origin;
  bool enable_bfcache;
};

std::string DescribeBFCacheType(
    const ::testing::TestParamInfo<ScreenshotCaptureTestNavigationType>& info) {
  if (info.param.enable_bfcache) {
    return "BFCacheEnabled";
  } else {
    return "BFCacheDisabled";
  }
}

std::string DescribeNavOriginType(
    const ::testing::TestParamInfo<ScreenshotCaptureTestNavigationType>& info) {
  if (info.param.same_origin) {
    return "SameOrigin";
  } else {
    return "CrossOrigin";
  }
}

std::string DescribeNavType(
    const ::testing::TestParamInfo<ScreenshotCaptureTestNavigationType>& info) {
  return DescribeNavOriginType(info) + "_" + DescribeBFCacheType(info);
}

constexpr ScreenshotCaptureTestNavigationType kNavTypes[] = {
    ScreenshotCaptureTestNavigationType{.same_origin = true,
                                        .enable_bfcache = true},
    ScreenshotCaptureTestNavigationType{.same_origin = true,
                                        .enable_bfcache = false},
    ScreenshotCaptureTestNavigationType{.same_origin = false,
                                        .enable_bfcache = true},
    ScreenshotCaptureTestNavigationType{.same_origin = false,
                                        .enable_bfcache = false},
};

class HostGetter {
 public:
  virtual ~HostGetter() = default;
  virtual std::string Get() = 0;

 protected:
  const std::array<std::string, 2> hosts = {"a.com", "b.com"};
};

class HostGetterSameOrigin : public HostGetter {
 public:
  ~HostGetterSameOrigin() override = default;

  // Always returns "a.com";
  std::string Get() override { return hosts[0]; }
};

class HostGetterCrossOrigin : public HostGetter {
 public:
  ~HostGetterCrossOrigin() override = default;

  // Alternatingly returns "a.com" / "b.com", such that each navigation is
  // cross-origin.
  std::string Get() override {
    index_ ^= 1;
    return hosts[index_];
  }

 private:
  int index_ = 1;
};

}  // namespace

class NavigationEntryScreenshotBrowserTestBase : public ContentBrowserTest {
 public:
  NavigationEntryScreenshotBrowserTestBase() = default;
  ~NavigationEntryScreenshotBrowserTestBase() override = default;

  void SetUp() override {
    NavigationTransitionUtils::ResetNumCopyOutputRequestIssuedForTesting();
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    NavigationTransitionUtils::ResetNumCopyOutputRequestIssuedForTesting();
    ContentBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(
        base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions));

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    SetupCrossSiteRedirector(embedded_test_server());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  static void ExpectBitmapRowsAreColor(
      const SkBitmap& bitmap,
      SkColor color,
      std::optional<gfx::Rect> compare_region = std::nullopt) {
    int num_pixel_mismatch = 0;
    gfx::Rect err_bounding_box;

    int row_start = 0;
    int row_end = bitmap.height();
    int col_start = 0;
    int col_end = bitmap.width();

    if (compare_region.has_value()) {
      row_start = compare_region->y();
      row_end = compare_region->bottom();
      col_start = compare_region->x();
      col_end = compare_region->right();
    }

    for (int r = row_start; r < row_end; ++r) {
      for (int c = col_start; c < col_end; ++c) {
        if (bitmap.getColor(c, r) != color) {
          ++num_pixel_mismatch;
          err_bounding_box.Union(gfx::Rect(c, r, 1, 1));
        }
      }
    }
    if (num_pixel_mismatch != 0) {
      EXPECT_TRUE(false)
          << "Number of pixel mismatches: " << num_pixel_mismatch
          << "; error bounding box: " << err_bounding_box.ToString()
          << "; bitmap size: "
          << gfx::Size(bitmap.width(), bitmap.height()).ToString()
          << "; expect color " << std::format("{:x}", color)
          << "; actual bitmap " << cc::GetPNGDataUrl(bitmap);
    }
  }

  void ExpectScreenshotIsColor(
      NavigationEntryScreenshot* screenshot,
      SkColor color,
      std::optional<gfx::Rect> compare_region = std::nullopt) {
    EXPECT_NE(screenshot, nullptr);
    EXPECT_EQ(screenshot->GetDimensions(), GetScaledViewportSize());

    auto bitmap = screenshot->GetBitmapForTesting();
    ExpectBitmapRowsAreColor(bitmap, color, compare_region);
  }

  void AssertOrderedScreenshotsAre(
      NavigationControllerImpl& controller,
      const std::vector<std::optional<SkColor>>& expected_screenshots,
      std::optional<gfx::Rect> compare_region = std::nullopt) {
    ASSERT_EQ(controller.GetEntryCount(),
              static_cast<int>(expected_screenshots.size()));
    for (int index = 0; index < controller.GetEntryCount(); ++index) {
      auto* entry = controller.GetEntryAtIndex(index);
      if (expected_screenshots[index].has_value()) {
        auto* screenshot = PreviewScreenshotForEntry(entry);
        ExpectScreenshotIsColor(screenshot, expected_screenshots[index].value(),
                                compare_region);
      } else {
        EXPECT_EQ(PreviewScreenshotForEntry(entry), nullptr);
      }
    }
  }

  gfx::Size GetScaledViewportSize() {
    // Scale down the size to avoid memory pressure causing cache purging.
    return ScaleToRoundedSize(
        web_contents()->GetNativeView()->GetPhysicalBackingSize(),
        /*scale=*/0.1);
  }

  size_t GetScaledViewportSizeInBytes() {
    // 4 bytes per pixel.
    return 4 * GetScaledViewportSize().Area64();
  }

  NavigationEntryScreenshotManager* GetManagerForTab(WebContents* tab) {
    return BrowserContextImpl::From(tab->GetBrowserContext())
        ->GetNavigationEntryScreenshotManager();
  }

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
};

class NavigationEntryScreenshotBrowserTest
    : public NavigationEntryScreenshotBrowserTestBase,
      public ::testing::WithParamInterface<
          ScreenshotCaptureTestNavigationType> {
 public:
  NavigationEntryScreenshotBrowserTest() = default;
  ~NavigationEntryScreenshotBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}}};

    if (GetParam().enable_bfcache) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          GetDefaultEnabledBackForwardCacheFeaturesForTesting(enabled_features),
          GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
      command_line->AppendSwitch(switches::kDisableBackForwardCache);
    }

    if (GetParam().same_origin) {
      host_getter_ = std::make_unique<HostGetterSameOrigin>();
    } else {
      host_getter_ = std::make_unique<HostGetterCrossOrigin>();
    }

    InitAndEnableRenderDocumentFeature(&scoped_feature_list_render_document_,
                                       RenderDocumentFeatureFullyEnabled()[0]);

    NavigationEntryScreenshotBrowserTestBase::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    NavigationEntryScreenshotBrowserTestBase::SetUpOnMainThread();

    // The default WebContents has only the initial navigation entry. This
    // WebContents does not have a RWHV associated with it, making
    // `GetScaledViewportSize` impossible. For this reason we manually load a
    // "red" document.
    ASSERT_TRUE(web_contents()
                    ->GetController()
                    .GetLastCommittedEntry()
                    ->IsInitialEntry());
    ASSERT_TRUE(NavigateToURL(web_contents(), GetNextUrl("/red.html")));
    WaitForCopyableViewInWebContents(web_contents());

    ASSERT_FALSE(web_contents()
                     ->GetController()
                     .GetLastCommittedEntry()
                     ->IsInitialEntry());
    // We don't capture any screenshot for the initial navigation entry (which
    // is replaced by the "red" entry).
    ASSERT_EQ(PreviewScreenshotForEntry(
                  web_contents()->GetController().GetLastCommittedEntry()),
              nullptr);
    ASSERT_EQ(GetManagerForTab(web_contents())->GetCurrentCacheSize(), 0U);

    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView());
    // Explicitly limit the output size as 10% of the logical viewport size.
    // This prevents the potential out-of-memory issue during the browsertest.
    // OoM causes the screenshots to be purged from the cache, failing the
    // tests.
    NavigationTransitionUtils::SetCapturedScreenshotSizeForTesting(
        GetScaledViewportSize());
  }

  std::string GetNextHost() { return host_getter_->Get(); }

  GURL GetNextUrl(std::string_view path) {
    return embedded_test_server()->GetURL(GetNextHost(), path);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList scoped_feature_list_render_document_;

  std::unique_ptr<HostGetter> host_getter_;
};

// Test the caching, retrieving and eviction of the
// `NavigationEntryScreenshotCache`, within a single tab, with both non-history
// navigation and history navigation.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       PrimaryMainFrameNav) {
  // Max of three screenshots per Profile (BrowserContext).
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 3 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("[red*] -> [red&, green*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green*] -> [red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/blue.html"));
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green&, blue*] -> [red&, green&, blue&, red*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/red.html"));
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "[red&, green&, blue&, red*] -> [red, green&, blue&, red&, green*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    AssertOrderedScreenshotsAre(
        controller,
        {std::nullopt, SK_ColorGREEN, SK_ColorBLUE, SK_ColorRED, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "[red, green&, blue&, red&, green*] -> "
        "[red, green, blue&, red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/blue.html"));
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, std::nullopt, SK_ColorBLUE, SK_ColorRED,
                     SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "[red, green, blue&, red&, green&, blue*] -> "
        "[red, green, blue&, red&, green*, blue&]");
    {
      // This simulates:
      // - The destination screenshot is first removed from the cache and used
      //   for preview. This is mimicking the behaviour during a swipe animation
      //   where the preview is removed from the cache before navigating.
      // - Then the navigation starts and commits, and a new screenshot is
      //   cached for the origin page.
      std::unique_ptr<NavigationEntryScreenshot> screenshot =
          controller.GetNavigationEntryScreenshotCache()->RemoveScreenshot(
              controller.GetEntryAtOffset(-1));
      ExpectScreenshotIsColor(screenshot.get(), SK_ColorGREEN);
    }
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, std::nullopt, SK_ColorBLUE, SK_ColorRED,
                     std::nullopt, SK_ColorBLUE});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "[red, green, blue&, red&, green*, blue&] -> "
        "[red, green, blue&, red*, green&, blue&]");
    {
      std::unique_ptr<NavigationEntryScreenshot> screenshot =
          controller.GetNavigationEntryScreenshotCache()->RemoveScreenshot(
              controller.GetEntryAtOffset(-1));
      ExpectScreenshotIsColor(screenshot.get(), SK_ColorRED);
    }
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, std::nullopt, SK_ColorBLUE, std::nullopt,
                     SK_ColorGREEN, SK_ColorBLUE});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "[red, green, blue&, red*, green&, blue&] -> "
        "[red, green, blue*, red&, green&, blue&]");
    {
      std::unique_ptr<NavigationEntryScreenshot> screenshot =
          controller.GetNavigationEntryScreenshotCache()->RemoveScreenshot(
              controller.GetEntryAtOffset(-1));
      ExpectScreenshotIsColor(screenshot.get(), SK_ColorBLUE);
    }
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, std::nullopt, std::nullopt, SK_ColorRED,
                     SK_ColorGREEN, SK_ColorBLUE});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "[red, green, blue*, red&, green&, blue&] -> "
        "[red, green*, blue&, red&, green&, blue]");

    // No screenshots for the "green" entry to the left of the "blue*" entry.
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, std::nullopt, SK_ColorBLUE, SK_ColorRED,
                     SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
}

// Testing the back/forward history navigations that span multiple navigation
// entries.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest, MultipleEntries) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("[red&, green&, blue&, red*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/blue.html"));
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/red.html"));
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }

  // History back nav to the first entry (red).
  {
    SCOPED_TRACE("[red&, green&, blue&, red*] -> [red*, green&, blue&, red&]");
    {
      std::unique_ptr<NavigationEntryScreenshot> screenshot =
          controller.GetNavigationEntryScreenshotCache()->RemoveScreenshot(
              controller.GetEntryAtOffset(-3));
      ExpectScreenshotIsColor(screenshot.get(), SK_ColorRED);
    }
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -3);
    ASSERT_EQ(controller.GetEntryCount(), 4);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, SK_ColorGREEN, SK_ColorBLUE, SK_ColorRED});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }

  // History forward nav to the third entry (blue).
  {
    SCOPED_TRACE("[red*, green&, blue&, red&] -> [red&, green&, blue*, red&]");
    {
      std::unique_ptr<NavigationEntryScreenshot> screenshot =
          controller.GetNavigationEntryScreenshotCache()->RemoveScreenshot(
              controller.GetEntryAtOffset(2));
      ExpectScreenshotIsColor(screenshot.get(), SK_ColorBLUE);
    }
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller, 2);
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, std::nullopt, SK_ColorRED});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
}

// Testing the cache's behavior if the destination navigation already has a
// screenshot. This can happen if the user performs history navigation without
// gesture (e.g., via the back button).
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       WithoutRemovingScreenshotFromDestinationEntry) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(3 * page_size);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("[red&, green&, blue&, red*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/blue.html"));
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/red.html"));
    ASSERT_EQ(controller.GetEntryCount(), 4);
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green&, blue&, red*] -> [red*, green&, blue&, red&]");
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -3);
    ASSERT_EQ(controller.GetEntryCount(), 4);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, SK_ColorGREEN, SK_ColorBLUE, SK_ColorRED});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
  {
    SCOPED_TRACE("[red*, green&, blue&, red&] -> [red&, green&, blue*, red&]");
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller, 2);
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, std::nullopt, SK_ColorRED});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
}

// Testing the screenshots are captured / evicted properly with multiple tabs.
// These tabs are within the same profile.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest, MultipleTabs) {
  // Max of three screenshots per Profile (BrowserContext).
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 3 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("tab1: [red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/blue.html"));
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  }

  // Creates a second tab within the same profile such that two tabs share the
  // same manager. `NavigationEntryScreenshotManager` is per Profile
  // (`BrowserContext`) - it budgets the memories for all the screenshots across
  // different tabs.
  auto* shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GetNextUrl("/red.html"),
      /*site_instance=*/nullptr, gfx::Size());
  auto* tab2 = static_cast<WebContentsImpl*>(shell2->web_contents());
  ASSERT_EQ(manager, GetManagerForTab(tab2));
  ASSERT_TRUE(tab2->GetController().GetLastCommittedEntry()->IsInitialEntry());
  WaitForCopyableViewInWebContents(tab2);
  // We don't capture for the initial entry.
  ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  auto& controller2 = tab2->GetController();
  ASSERT_EQ(controller2.GetEntryCount(), 1);

  {
    SCOPED_TRACE(
        "tab1: [red&, green&, blue*] -> [red&, green&, blue*] (no change); "
        "tab2: [red*] -> [red&, green*]");
    NavigateTabAndWaitForScreenshotCached(tab2, controller2,
                                          GetNextUrl("/green.html"));
    AssertOrderedScreenshotsAre(controller2, {SK_ColorRED, std::nullopt});
    // No change in tab1, because we have one cache slot for a new screenshot in
    // tab2.
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "tab1: [red&, green&, blue*] -> [red, green&, blue*]; "
        "tab2: [red&, green*] -> [red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCached(tab2, controller2,
                                          GetNextUrl("/blue.html"));
    AssertOrderedScreenshotsAre(controller2,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    // Tab1's "red" screenshot is evicted. We always evict from the least
    // recently used tab (tab1), and always evict from the most distant
    // navigation entry (red).
    AssertOrderedScreenshotsAre(controller,
                                {std::nullopt, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "tab1: [red, green&, blue*] -> [red, green, blue*]; "
        "tab2: [red&, green&, blue*] -> [red&, green&, blue&, red*]");
    NavigateTabAndWaitForScreenshotCached(tab2, controller2,
                                          GetNextUrl("/red.html"));
    AssertOrderedScreenshotsAre(
        controller2, {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, std::nullopt});
    // Tab1's "green" is evicted.
    AssertOrderedScreenshotsAre(controller,
                                {std::nullopt, std::nullopt, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "tab1: [red, green, blue*] -> [red, green*, blue&]; "
        "tab2: [red&, green&, blue&, red*] -> [red, green&, blue&, red*]");
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(controller,
                                {std::nullopt, std::nullopt, SK_ColorBLUE});
    // Screenshot for "red" of tab2 is evicted.
    AssertOrderedScreenshotsAre(
        controller2, {std::nullopt, SK_ColorGREEN, SK_ColorBLUE, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }
  {
    SCOPED_TRACE(
        "tab1: [red, green*, blue&] -> [red*, green&, blue&]; "
        "tab2: [red, green&, blue&, red*] -> [red, green, blue&, red*]");
    // History-navigate tab1.
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(controller,
                                {std::nullopt, SK_ColorGREEN, SK_ColorBLUE});
    // Screenshot for "red" is evicted.
    AssertOrderedScreenshotsAre(
        controller2, {std::nullopt, std::nullopt, SK_ColorBLUE, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), memory_budget);
  }

  // Close tab2.
  tab2->Close();
  ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);

  // Clear the navigation entries of tab1.
  controller.PruneAllButLastCommitted();
  ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
}

// Testing the screenshots are captured / evicted properly with multiple tabs
// from different profiles.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest, MultipleProfiles) {
  // Max of two screenshots per Profile (BrowserContext).
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 2 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("tab1: [red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/blue.html"));
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  }

  // Creates a second tab but of a different profile, such that each tab is
  // managed independently.
  auto* shell2 = Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->off_the_record_browser_context(),
      GetNextUrl("/red.html"),
      /*site_instance=*/nullptr, gfx::Size());
  auto* tab2 = static_cast<WebContentsImpl*>(shell2->web_contents());
  ASSERT_TRUE(tab2->GetController().GetLastCommittedEntry()->IsInitialEntry());
  WaitForCopyableViewInWebContents(tab2);
  auto* manager2 = GetManagerForTab(tab2);
  ASSERT_NE(manager, manager2);
  // We don't capture for the initial entry.
  ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  ASSERT_EQ(manager2->GetCurrentCacheSize(), 0U);
  auto& controller2 = tab2->GetController();
  ASSERT_EQ(controller2.GetEntryCount(), 1);

  {
    SCOPED_TRACE(
        "tab1: [red&, green&, blue*]->[red&, green&, blue*] (no change);"
        "tab2: [red*] -> [red&, green*]");
    NavigateTabAndWaitForScreenshotCached(tab2, controller2,
                                          GetNextUrl("/green.html"));
    ASSERT_EQ(controller.GetEntryCount(), 3);
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    AssertOrderedScreenshotsAre(controller2, {SK_ColorRED, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
    ASSERT_EQ(manager2->GetCurrentCacheSize(), page_size);
  }
  {
    // tab1: [red&, green&, blue*] -> [red&, green&, blue*] (no change)
    // tab2: [red&, green*] -> [red&, green&, blue*]
    SCOPED_TRACE(
        "tab1: [red&, green&, blue*]->[red&, green&, blue*] (no change)"
        "tab2: [red&, green*] -> [red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCached(tab2, controller2,
                                          GetNextUrl("/blue.html"));
    // Caching a new screenshot for tab2 has no impact on tab1 since they are
    // managed independently.
    ASSERT_EQ(controller.GetEntryCount(), 3);
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    AssertOrderedScreenshotsAre(controller2,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
    ASSERT_EQ(manager2->GetCurrentCacheSize(), 2 * page_size);
  }

  // Close tab2.
  tab2->Close();
  ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  ASSERT_EQ(manager2->GetCurrentCacheSize(), 0U);

  // Clear entries from tab1.
  controller.PruneAllButLastCommitted();
  ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
  ASSERT_EQ(manager2->GetCurrentCacheSize(), 0U);
}

// Screenshots are captured for renderer-initiated navigations (e.g.,
// link-clicking).
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       RendererInitiatedNav) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  SCOPED_TRACE("[red*] -> [red&, green*]");
  ScopedScreenshotCapturedObserverForTesting observer(
      web_contents()->GetController().GetLastCommittedEntryIndex());
  ASSERT_TRUE(
      NavigateToURLFromRenderer(web_contents(), GetNextUrl("/green.html")));
  WaitForCopyableViewInWebContents(web_contents());
  observer.Wait();

  AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt});
  ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
}

// Capture for renderer initiated history back navigation via `history.back()`.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest, HistoryDotBack) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("[red*] -> [red&, green*]");
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(
        NavigateToURLFromRenderer(web_contents(), GetNextUrl("/green.html")));
    WaitForCopyableViewInWebContents(web_contents());
    observer.Wait();

    AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green*] -> [red*, green&]");
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    auto* rfh = web_contents()->GetPrimaryMainFrame();
    TestFrameNavigationObserver nav_observer(rfh);
    ASSERT_TRUE(ExecJs(rfh, "window.history.back();"));
    nav_observer.Wait();
    observer.Wait();

    AssertOrderedScreenshotsAre(controller, {std::nullopt, SK_ColorGREEN});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
  }
}

// Asserting that both the navigations from and to the about:blank triggers
// screenshot capture.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       AboutBlankCaptured) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);

  // Creates a new tab and loads about:blank.
  auto* shell = Shell::CreateNewWindow(
      ShellContentBrowserClient::Get()->browser_context(),
      GURL(url::kAboutBlankURL),
      /*site_instance=*/nullptr, gfx::Size());
  auto* tab = static_cast<WebContentsImpl*>(shell->web_contents());
  const std::string kRemoveScrollbar = R"(
    const meta = document.createElement("meta");
    meta.name = "viewport";
    meta.content = "width=device-width,minimum-scale=1";
    document.head.appendChild(meta);
  )";
  ASSERT_TRUE(ExecJs(tab, kRemoveScrollbar));
  WaitForCopyableViewInWebContents(tab);

  auto& controller = tab->GetController();
  ASSERT_EQ(controller.GetEntryCount(), 1);
  ASSERT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());
  ASSERT_EQ(controller.GetLastCommittedEntry()->GetURL(),
            GURL(url::kAboutBlankURL));

  // Navigates away from about:blank.
  ASSERT_TRUE(NavigateToURL(tab, GetNextUrl("/green.html")));
  WaitForCopyableViewInWebContents(tab);
  // Captured.
  AssertOrderedScreenshotsAre(controller, {SK_ColorWHITE, std::nullopt});

  HistoryNavigateTabAndWaitForScreenshotCached(tab, controller, -1);
  // Captured.
  AssertOrderedScreenshotsAre(controller, {std::nullopt, SK_ColorGREEN});
}

IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest, Redirect) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  const auto next_host = GetNextHost();
  const auto redirect_gurl = embedded_test_server()->GetURL(
      "/cross-site/" + next_host + "/green.html");
  const auto expected_gurl =
      embedded_test_server()->GetURL(next_host, "/green.html");
  {
    SCOPED_TRACE("[red*] -> [red&, green*]");
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), redirect_gurl, expected_gurl));
    WaitForCopyableViewInWebContents(web_contents());
    observer.Wait();
    AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green*] -> [red*, green&]");
    {
      std::unique_ptr<NavigationEntryScreenshot> screenshot =
          controller.GetNavigationEntryScreenshotCache()->RemoveScreenshot(
              controller.GetEntryAtOffset(-1));
      ExpectScreenshotIsColor(screenshot.get(), SK_ColorRED);
    }
    HistoryNavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                                 -1);
    AssertOrderedScreenshotsAre(controller, {std::nullopt, SK_ColorGREEN});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
  }
}

// We don't capture if we simply reload the page.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       Reload_NotCaptured) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  SCOPED_TRACE("Reload.");
  controller.Reload(content::ReloadType::NORMAL, false);
  ASSERT_TRUE(WaitForLoadStop(web_contents()));
  // No requests issued.
  ASSERT_EQ(
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(), 0);

  AssertOrderedScreenshotsAre(controller, {std::nullopt});
  ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
}

// Testing that the navigation via `window.location.replace` won't trigger a
// capture.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       LocationDotReplace_NotCaptured) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  SCOPED_TRACE("`window.location.replace`");
  const GURL green_url =
      embedded_test_server()->GetURL(GetNextHost(), "/green.html");
  auto* rfh = web_contents()->GetPrimaryMainFrame();
  TestFrameNavigationObserver nav_observer(rfh);
  ASSERT_TRUE(
      ExecJs(rfh, JsReplace("window.location.replace($1);", green_url)));
  // No requests issued.
  ASSERT_EQ(
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(), 0);

  AssertOrderedScreenshotsAre(controller, {std::nullopt});
  ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
}

// Testing that the navigation with a 204 response won't trigger a capture.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       NavigationTo204_NotCaptured) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  const GURL url_204 =
      embedded_test_server()->GetURL(GetNextHost(), "/page204.html");
  ASSERT_TRUE(NavigateToURLAndExpectNoCommit(shell(), url_204));
  // No requests issued.
  ASSERT_EQ(
      NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting(), 0);

  AssertOrderedScreenshotsAre(controller, {std::nullopt});
  ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
}

namespace {
// This assumes the top 50% is the embedder and the bottom 50% is an iframe.
void AssertScreenshotForPageWithIFrameIs(NavigationEntry* entry,
                                         SkColor embedder,
                                         SkColor iframe) {
  auto* screenshot = PreviewScreenshotForEntry(entry);
  ASSERT_NE(screenshot, nullptr);
  const auto size = screenshot->GetDimensions();
  auto bitmap = screenshot->GetBitmapForTesting();

  int half_height = size.height() / 2;
  bool is_height_odd = (size.height() % 2);

  // Expect the embedder's color matches.
  NavigationEntryScreenshotBrowserTest::ExpectBitmapRowsAreColor(
      bitmap, embedder, gfx::Rect(0, 0, bitmap.width(), half_height));

  // Expect the iframe's color matches. Skip checking the middle row if the
  // height is an odd number.
  int iframe_height_start = is_height_odd ? half_height + 1 : half_height;
  NavigationEntryScreenshotBrowserTest::ExpectBitmapRowsAreColor(
      bitmap, iframe,
      gfx::Rect(0, iframe_height_start, bitmap.width(), half_height));
}
}  // namespace

// Asserts that no screenshots captured for the navigations of iframes.
//
// TODO(crbug.com/40896219): Support iframe navigations.
//
// TODO(crbug.com/340929354): Reenable the test.
IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTest,
                       DISABLED_SameOriginIFrame_NotCaptured) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  const std::string kCreateIFrameWithID = R"(
    const style = document.createElement("style");
    style.textContent = "iframe { width: 100vw; height: 50vh; position: fixed; top: 50vh; left: 0; border: 0; }";
    document.head.appendChild(style);

    const iframe = document.createElement("iframe");
    iframe.id = $1;
    document.body.appendChild(iframe);
  )";

  {
    SCOPED_TRACE("[red*] -> [red(empty)*]");
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace(kCreateIFrameWithID, "iframe_id")));
    NavigateIframeToURL(web_contents(), "iframe_id", GetNextUrl("/green.html"));
    WaitForCopyableViewInWebContents(web_contents());
    // Insert an <iframe> into DOM does not create a navigation entry. This
    // navigation won't trigger a capture because it is not from the primary
    // main frame.
    AssertOrderedScreenshotsAre(controller, {std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
  }
  {
    SCOPED_TRACE("[red(green)*] -> [red(green), red(title1)*]");
    NavigateIframeToURL(web_contents(), "iframe_id",
                        GetNextUrl("/title1.html"));
    WaitForCopyableViewInWebContents(web_contents());
    AssertOrderedScreenshotsAre(controller, {std::nullopt, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
  }
  {
    // History navigation on the subframe. No capture.
    SCOPED_TRACE("[red(green), red(title1)*] -> [red(green)*, red(title1)]");
    ASSERT_TRUE(HistoryGoBack(web_contents()));
    WaitForCopyableViewInWebContents(web_contents());
    AssertOrderedScreenshotsAre(controller, {std::nullopt, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 0U);
  }
  {
    // Main frame navigation. Capture.
    SCOPED_TRACE("[red(green)*, red(title1)] -> [red(green)&, title2*]");
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    ASSERT_TRUE(NavigateToURL(web_contents(), GetNextUrl("/title2.html")));
    WaitForCopyableViewInWebContents(web_contents());
    observer.Wait();
    ASSERT_EQ(controller.GetEntryCount(), 2);
    AssertScreenshotForPageWithIFrameIs(controller.GetEntryAtIndex(0),
                                        SK_ColorRED, SK_ColorGREEN);
    ASSERT_EQ(PreviewScreenshotForEntry(controller.GetEntryAtIndex(1)),
              nullptr);
    ASSERT_EQ(manager->GetCurrentCacheSize(), page_size);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         NavigationEntryScreenshotBrowserTest,
                         ::testing::ValuesIn(kNavTypes),
                         &DescribeNavType);

class NavigationEntryScreenshotBrowserTestWithPrerender
    : public NavigationEntryScreenshotBrowserTest {
 public:
  NavigationEntryScreenshotBrowserTestWithPrerender() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // `prerender_helper_` has its own `ScopedFeatureLists`. We need to init
    // the test base's `ScopedFeatureLists` to respect the destruction order.
    NavigationEntryScreenshotBrowserTest::SetUpCommandLine(command_line);

    prerender_helper_ = std::make_unique<test::PrerenderTestHelper>(
        base::BindLambdaForTesting([&]() { return shell()->web_contents(); }));
  }

  test::PrerenderTestHelper* prerender_helper() {
    return prerender_helper_.get();
  }

 private:
  std::unique_ptr<test::PrerenderTestHelper> prerender_helper_;
};

IN_PROC_BROWSER_TEST_P(NavigationEntryScreenshotBrowserTestWithPrerender,
                       PrerenderActivation) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("[red*] -> [red&, green*]");
    NavigateTabAndWaitForScreenshotCached(web_contents(), controller,
                                          GetNextUrl("/green.html"));
    AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), page_size);
  }

  // Add a prerender and navigate the main frame to the prerendered URL. The
  // the prerender's document-fetching navigation request is not in the primary
  // main frame so no screenshot is captured.
  const auto prerender_gurl = GetNextUrl("/title1.html");
  prerender_helper()->AddPrerender(prerender_gurl);
  // No change in the screenshots.
  AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt});
  ASSERT_EQ(manager->GetCurrentCacheSize(), page_size);

  // Activate the prerendered URL by navigating the primary main frame.
  {
    SCOPED_TRACE("[red&, green*] -> [red&, green&, title1*]");
    test::PrerenderHostObserver activation_obs(*web_contents(), prerender_gurl);
    ScopedScreenshotCapturedObserverForTesting observer(
        web_contents()->GetController().GetLastCommittedEntryIndex());
    prerender_helper()->NavigatePrimaryPage(prerender_gurl);
    observer.Wait();
    activation_obs.WaitForActivation();
    ASSERT_TRUE(activation_obs.was_activated());
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt});
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NavigationEntryScreenshotBrowserTestWithPrerender,
    // Prerender requires same-origin URL (a.com).
    ::testing::ValuesIn(
        {ScreenshotCaptureTestNavigationType{.same_origin = true,
                                             .enable_bfcache = true},
         ScreenshotCaptureTestNavigationType{.same_origin = true,
                                             .enable_bfcache = false}}),
    &DescribeNavType);

namespace {

void NavigateTabAndWaitForScreenshotCachedSameDoc(
    WebContents* tab,
    NavigationControllerImpl& controller,
    const GURL& destination) {
  NavigateTabAndWaitForScreenshotCached(tab, controller, destination, true);
}

void HistoryNavigateTabAndWaitForScreenshotCachedSameDoc(
    WebContents* tab,
    NavigationControllerImpl& controller,
    int offset) {
  HistoryNavigateTabAndWaitForScreenshotCached(tab, controller, offset, true);
}

}  // namespace

class SameDocNavigationEntryScreenshotBrowserTest
    : public NavigationEntryScreenshotBrowserTestBase {
 public:
  SameDocNavigationEntryScreenshotBrowserTest() = default;
  ~SameDocNavigationEntryScreenshotBrowserTest() override = default;

  void SetUp() override {
    if (base::SysInfo::GetAndroidHardwareEGL() == "emulation") {
      // crbug.com/337886037 and crrev.com/c/5504854/comment/b81b8fb6_95fb1381/:
      // The CopyOutputRequests crash the GPU process. ANGLE is exporting the
      // native fence support on Android emulators but it doesn't work properly.
      GTEST_SKIP();
    }
    NavigationEntryScreenshotBrowserTestBase::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {blink::features::kBackForwardTransitions, {}},
        {blink::features::kIncrementLocalSurfaceIdForMainframeSameDocNavigation,
         {}}};

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});

    // Disable the vertical scroll bar, otherwise they might show up on the
    // screenshot, making the test flaky.
    command_line->AppendSwitch(switches::kHideScrollbars);
    NavigationEntryScreenshotBrowserTestBase::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    NavigationEntryScreenshotBrowserTestBase::SetUpOnMainThread();

    ASSERT_TRUE(NavigateToURL(web_contents(), embedded_test_server()->GetURL(
                                                  "/changing_color.html")));
    WaitForCopyableViewInWebContents(web_contents());

    mojo::ScopedAllowSyncCallForTesting allowed_for_testing;
    GetHostFrameSinkManager()->SetSameDocNavigationScreenshotSizeForTesting(
        GetScaledViewportSize());
  }

  gfx::Rect GetCompareRegion() { return gfx::Rect(GetScaledViewportSize()); }

  GURL GetURL(const std::string& hash) {
    return embedded_test_server()->GetURL("/changing_color.html" + hash);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(SameDocNavigationEntryScreenshotBrowserTest, Basic) {
  const size_t page_size = GetScaledViewportSizeInBytes();
  const size_t memory_budget = 10 * page_size;
  auto* manager = GetManagerForTab(web_contents());
  manager->SetMemoryBudgetForTesting(memory_budget);
  auto& controller = web_contents()->GetController();

  {
    SCOPED_TRACE("[red*] -> [red&, green*]");
    NavigateTabAndWaitForScreenshotCachedSameDoc(web_contents(), controller,
                                                 GetURL("#green"));
    AssertOrderedScreenshotsAre(controller, {SK_ColorRED, std::nullopt},
                                GetCompareRegion());
    ASSERT_EQ(manager->GetCurrentCacheSize(), 1 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green*] -> [red&, green&, blue*]");
    NavigateTabAndWaitForScreenshotCachedSameDoc(web_contents(), controller,
                                                 GetURL("#blue"));
    AssertOrderedScreenshotsAre(controller,
                                {SK_ColorRED, SK_ColorGREEN, std::nullopt},
                                GetCompareRegion());
    ASSERT_EQ(manager->GetCurrentCacheSize(), 2 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green&, blue*] -> [red&, green&, blue&, red*]");
    NavigateTabAndWaitForScreenshotCachedSameDoc(web_contents(), controller,
                                                 GetURL("#red"));
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, std::nullopt},
        GetCompareRegion());
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green&, blue&, red*] -> [red&, green&, blue*, red&]");
    HistoryNavigateTabAndWaitForScreenshotCachedSameDoc(web_contents(),
                                                        controller, -1);
    AssertOrderedScreenshotsAre(
        controller, {SK_ColorRED, SK_ColorGREEN, std::nullopt, SK_ColorRED},
        GetCompareRegion());
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
  {
    SCOPED_TRACE("[red&, green&, blue*, red&] -> [red*, green&, blue&, red&]");
    HistoryNavigateTabAndWaitForScreenshotCachedSameDoc(web_contents(),
                                                        controller, -2);
    AssertOrderedScreenshotsAre(
        controller, {std::nullopt, SK_ColorGREEN, SK_ColorBLUE, SK_ColorRED},
        GetCompareRegion());
    ASSERT_EQ(manager->GetCurrentCacheSize(), 3 * page_size);
  }
}

}  // namespace content