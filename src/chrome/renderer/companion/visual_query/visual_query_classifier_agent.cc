// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/companion/visual_query/visual_query_classifier_agent.h"

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/companion/visual_query.mojom.h"
#include "chrome/common/companion/visual_query/features.h"
#include "components/optimization_guide/proto/visual_search_model_metadata.pb.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/mojom/base/proto_wrapper.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace companion::visual_query {

VisualQueryClassifierAgent::VisualQueryClassifierAgent(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  if (render_frame) {
    render_frame_ = render_frame;
    render_frame->GetAssociatedInterfaceRegistry()
        ->AddInterface<mojom::VisualSuggestionsRequestHandler>(
            base::BindRepeating(
                &VisualQueryClassifierAgent::OnRendererAssociatedRequest,
                base::Unretained(this)));
  }
}

VisualQueryClassifierAgent::~VisualQueryClassifierAgent() = default;

// static
VisualQueryClassifierAgent* VisualQueryClassifierAgent::Create(
    content::RenderFrame* render_frame) {
  return new VisualQueryClassifierAgent(render_frame);
}

void VisualQueryClassifierAgent::StartVisualClassification(
    base::File visual_model,
    std::optional<mojo_base::ProtoWrapper> config_proto,
    mojo::PendingRemote<mojom::VisualSuggestionsResultHandler> result_handler) {}

void VisualQueryClassifierAgent::OnClassificationDone(
    ClassificationResultsAndStats results) {
  is_classifying_ = false;
  is_retrying_ = false;
  std::vector<mojom::VisualQuerySuggestionPtr> final_results;
  for (auto& result : results.first) {
    final_results.emplace_back(mojom::VisualQuerySuggestion::New(
        result.image_contents, result.alt_text));
  }

  mojom::ClassificationStatsPtr stats;
  if (results.second.is_null()) {
    stats = mojom::ClassificationStats::New(mojom::ClassificationStats());
  } else {
    stats = std::move(results.second);
  }

  if (result_handler_.is_bound()) {
    result_handler_->HandleClassification(std::move(final_results),
                                          std::move(stats));
  }
  LOCAL_HISTOGRAM_COUNTS_100("Companion.VisualQuery.Agent.ClassificationDone",
                             results.first.size());
}

void VisualQueryClassifierAgent::OnRendererAssociatedRequest(
    mojo::PendingAssociatedReceiver<mojom::VisualSuggestionsRequestHandler>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void VisualQueryClassifierAgent::DidFinishLoad() {
  if (!features::IsVisualQuerySuggestionsAgentEnabled()) {
    return;
  }
  if (!render_frame_ || !render_frame_->IsMainFrame() ||
      model_provider_.is_bound()) {
    return;
  }

  render_frame_->GetBrowserInterfaceBroker()->GetInterface(
      model_provider_.BindNewPipeAndPassReceiver());

  if (model_provider_.is_bound()) {
    model_provider_->GetModelWithMetadata(
        base::BindOnce(&VisualQueryClassifierAgent::HandleGetModelCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    LOCAL_HISTOGRAM_BOOLEAN(
        "Companion.VisualQuery.Agent.ModelRequestSentSuccess", true);
  }
}

void VisualQueryClassifierAgent::HandleGetModelCallback(
    base::File file,
    std::optional<mojo_base::ProtoWrapper> config) {
  // Now that we have the result, we can unbind and reset the receiver pipe.
  model_provider_.reset();

  mojo::PendingRemote<mojom::VisualSuggestionsResultHandler> result_handler;
  StartVisualClassification(std::move(file), std::move(config),
                            std::move(result_handler));
}

void VisualQueryClassifierAgent::OnDestruct() {
  if (render_frame_) {
    render_frame_->GetAssociatedInterfaceRegistry()->RemoveInterface(
        mojom::VisualSuggestionsRequestHandler::Name_);
  }
  delete this;
}

}  // namespace companion::visual_query
