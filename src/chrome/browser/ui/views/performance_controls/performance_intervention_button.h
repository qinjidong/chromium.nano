// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller_delegate.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"

class BrowserView;
class PerformanceInterventionButtonController;

namespace views {
class BubbleDialogModelHost;
class Widget;
}  // namespace views

class PerformanceInterventionButton
    : public ToolbarButton,
      public PerformanceInterventionButtonControllerDelegate,
      public views::WidgetObserver {
  METADATA_HEADER(PerformanceInterventionButton, ToolbarButton)

 public:
  explicit PerformanceInterventionButton(BrowserView* browser_view);
  ~PerformanceInterventionButton() override;

  PerformanceInterventionButton(const PerformanceInterventionButton&) = delete;
  PerformanceInterventionButton& operator=(
      const PerformanceInterventionButton&) = delete;

  // PerformanceInterventionButtonControllerDelegate:
  void Show() override;
  void Hide() override;
  bool IsButtonShowing() override;
  bool IsBubbleShowing() override;

  // views::View:
  void OnThemeChanged() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  PerformanceInterventionButtonController* controller() {
    return controller_.get();
  }

 private:
  void OnClicked();
  void CreateBubble();

  std::unique_ptr<PerformanceInterventionButtonController> controller_;
  const raw_ptr<BrowserView> browser_view_;
  raw_ptr<views::BubbleDialogModelHost> bubble_dialog_model_host_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_widget_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_PERFORMANCE_INTERVENTION_BUTTON_H_
