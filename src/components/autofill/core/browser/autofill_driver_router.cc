// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_driver_router.h"

#include <algorithm>
#include <functional>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

namespace {

// Calls |fun| for all drivers in |form_forest|.
void ForEachFrame(internal::FormForest& form_forest,
                  base::FunctionRef<void(AutofillDriver&)> fun) {
  for (const std::unique_ptr<internal::FormForest::FrameData>& some_frame :
       form_forest.frame_datas()) {
    if (some_frame->driver) {
      std::invoke(fun, *some_frame->driver);
    }
  }
}

}  // namespace

AutofillDriverRouter::AutofillDriverRouter() = default;
AutofillDriverRouter::~AutofillDriverRouter() = default;

AutofillDriver* AutofillDriverRouter::DriverOfFrame(LocalFrameToken frame) {
  const auto& frames = form_forest_.frame_datas();
  auto it = frames.find(frame);
  return it != frames.end() ? static_cast<AutofillDriver*>((*it)->driver.get())
                            : nullptr;
}

void AutofillDriverRouter::UnregisterDriver(AutofillDriver& driver,
                                            bool driver_is_dying) {
  for (const std::unique_ptr<internal::FormForest::FrameData>& frame :
       form_forest_.frame_datas()) {
    if (frame->driver == &driver) {
      form_forest_.EraseFormsOfFrame(frame->frame_token,
                                     /*keep_frame=*/!driver_is_dying);
      break;
    }
  }
}

// Routing of events called by the renderer:

// Calls TriggerFormExtraction() on all ContentAutofillDrivers in |form_forest_|
// as well as their ancestor ContentAutofillDrivers.
//
// An ancestor might not be contained in the form tree known to FormForest: if
// the ancestor contained only invisible iframe(s) and no interesting fields, it
// would not be sent to the browser. In the meantime, these frames may have
// become visible. Therefore, we also call TriggerFormExtraction() in all
// ancestors.
//
// The typical use case is that some frame triggers form extractions on its own
// initiative and triggers an event. Then AutofillDriverRouter's event handler
// tells the other frames to form extraction, too, using
// TriggerFormExtractionExcept(source).
void AutofillDriverRouter::TriggerFormExtractionExcept(
    AutofillDriver& exception) {
  base::flat_set<AutofillDriver*> already_triggered;
  ForEachFrame(form_forest_, [&](AutofillDriver& driver_ref) {
    AutofillDriver* driver = &driver_ref;
    do {
      if (!already_triggered.insert(driver).second) {
        // An earlier invocation of this lambda has executed the rest of this
        // loop's body for `driver` and hence also for all its ancestors.
        break;
      }
      if (driver == &exception) {
        continue;
      }
      driver->TriggerFormExtractionInDriverFrame();
    } while ((driver = driver->GetParent()) != nullptr);
  });
}

void AutofillDriverRouter::FormsSeen(
    AutofillDriver& source,
    std::vector<FormData> renderer_forms,
    const std::vector<FormGlobalId>& removed_forms,
    RoutedCallback<const std::vector<FormData>&,
                   const std::vector<FormGlobalId>&> callback) {
  base::flat_set<FormGlobalId> forms_with_removed_fields =
      form_forest_.EraseForms(removed_forms);

  std::vector<FormGlobalId> renderer_form_ids;
  renderer_form_ids.reserve(renderer_forms.size());
  for (const FormData& renderer_form : renderer_forms) {
    renderer_form_ids.push_back(renderer_form.global_id());
  }

  for (FormData& form : std::move(renderer_forms)) {
    form_forest_.UpdateTreeOfRendererForm(std::move(form), source);
  }

  // Collects the browser forms of the |renderer_forms_ids|. If all forms in
  // |renderer_forms_ids| are root forms, each of them has a different browser
  // form. Otherwise, all forms in |renderer_forms_ids| are non-root forms in
  // the same tree, and |browser_forms| will contain the flattened root of this
  // tree.
  std::vector<FormData> browser_forms;
  browser_forms.reserve(renderer_form_ids.size());
  for (FormGlobalId renderer_form_id : renderer_form_ids) {
    const FormData& browser_form =
        form_forest_.GetBrowserForm(renderer_form_id);
    if (!base::Contains(browser_forms, browser_form.global_id(),
                        &FormData::global_id)) {
      browser_forms.push_back(browser_form);
    }
  }
  DCHECK(browser_forms.size() == renderer_form_ids.size() ||
         browser_forms.size() == 1);

  for (const FormGlobalId form_id : forms_with_removed_fields) {
    const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
    if (!base::Contains(browser_forms, browser_form.global_id(),
                        &FormData::global_id)) {
      browser_forms.push_back(browser_form);
    }
  }

  // Send the browser forms to the individual frames.
  if (!browser_forms.empty()) {
    LocalFrameToken frame = browser_forms.front().host_frame;
    DCHECK(base::ranges::all_of(browser_forms, [frame](const FormData& f) {
      return f.host_frame == frame;
    }));
    AutofillDriver* target = DriverOfFrame(frame);
    callback(CHECK_DEREF(target), browser_forms, removed_forms);
  } else if (!removed_forms.empty()) {
    callback(source, {}, removed_forms);
  }
}

void AutofillDriverRouter::SetFormToBeProbablySubmitted(
    AutofillDriver& source,
    std::optional<FormData> form,
    RoutedCallback<base::optional_ref<const FormData>> callback) {
  if (!form) {
    callback(source, std::nullopt);
    return;
  }

  FormGlobalId form_id = form->global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form).value(), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), &browser_form);
}

void AutofillDriverRouter::FormSubmitted(
    AutofillDriver& source,
    FormData form,
    bool known_success,
    mojom::SubmissionSource submission_source,
    RoutedCallback<const FormData&, bool, mojom::SubmissionSource> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, known_success, submission_source);
}

void AutofillDriverRouter::CaretMovedInFormField(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    RoutedCallback<const FormData&, const FormFieldData&, const gfx::Rect&>
        callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field, caret_bounds);
}

void AutofillDriverRouter::TextFieldDidChange(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    base::TimeTicks timestamp,
    RoutedCallback<const FormData&, const FormFieldData&, base::TimeTicks>
        callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field, timestamp);
}

void AutofillDriverRouter::TextFieldDidScroll(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    RoutedCallback<const FormData&, const FormFieldData&> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field);
}

void AutofillDriverRouter::SelectControlDidChange(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    RoutedCallback<const FormData&, const FormFieldData&> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field);
}

void AutofillDriverRouter::AskForValuesToFill(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source,
    RoutedCallback<const FormData&,
                   const FormFieldData&,
                   const gfx::Rect&,
                   AutofillSuggestionTriggerSource> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field, caret_bounds,
           trigger_source);
}

void AutofillDriverRouter::HidePopup(AutofillDriver& source,
                                     RoutedCallback<> callback) {
  // We don't know which AutofillManager is currently displaying the popup.
  // Since the the general approach of popup hiding in Autofill seems to be
  // "better safe than sorry", broadcasting this event is fine.
  // TODO(crbug.com/40284890): This event should go away when the popup-hiding
  // mechanism has been cleaned up.
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::FocusOnNonFormField(AutofillDriver& source,
                                               bool had_interacted_form,
                                               RoutedCallback<bool> callback) {
  // Suppresses FocusOnNonFormField() if the focus has already moved to a
  // different frame.
  if (focused_frame_ != source.GetFrameToken()) {
    return;
  }

  // Prevents FocusOnFormField() from calling FocusOnNonFormField().
  focus_no_longer_on_form_has_fired_ = true;

  TriggerFormExtractionExcept(source);

  // The last-focused form is not known at this time. Even if
  // FocusOnNonFormField() had a FormGlobalId parameter, we couldn't call
  // `form_forest_.GetBrowserForm()` because this is admissible only after a
  // `form_forest_.UpdateTreeOfRendererForm()` for the same form.
  //
  // Therefore, we simply broadcast the event.
  ForEachFrame(form_forest_, [&](AutofillDriver& some_driver) {
    callback(some_driver, had_interacted_form);
  });
}

void AutofillDriverRouter::FocusOnFormField(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    RoutedCallback<const FormData&, const FormFieldData&> callback,
    RoutedCallback<> focus_no_longer_on_form) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  // Calls FocusOnNonFormField() if the focus has already moved from a
  // different frame and FocusOnNonFormField() hasn't been called yet.
  if (focused_frame_ != source.GetFrameToken() &&
      !focus_no_longer_on_form_has_fired_) {
    ForEachFrame(form_forest_, focus_no_longer_on_form);
  }

  // Suppresses late FocusOnNonFormField().
  focused_frame_ = source.GetFrameToken();
  focus_no_longer_on_form_has_fired_ = false;

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field);
}

void AutofillDriverRouter::DidFillAutofillFormData(
    AutofillDriver& source,
    FormData form,
    base::TimeTicks timestamp,
    RoutedCallback<const FormData&, base::TimeTicks> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, timestamp);
}

void AutofillDriverRouter::DidEndTextFieldEditing(AutofillDriver& source,
                                                  RoutedCallback<> callback) {
  TriggerFormExtractionExcept(source);

  // The last-focused form is not known at this time. Even if
  // DidEndTextFieldEditing() had a FormGlobalId parameter, we couldn't call
  // `form_forest_.GetBrowserForm()` because this is admissible only after a
  // `form_forest_.UpdateTreeOfRendererForm()` for the same form.
  //
  // Therefore, we simply broadcast the event.
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::SelectOrSelectListFieldOptionsDidChange(
    AutofillDriver& source,
    FormData form,
    RoutedCallback<const FormData&> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form);
}

void AutofillDriverRouter::JavaScriptChangedAutofilledValue(
    AutofillDriver& source,
    FormData form,
    const FormFieldData& field,
    const std::u16string& old_value,
    bool formatting_only,
    RoutedCallback<const FormData&,
                   const FormFieldData&,
                   const std::u16string&,
                   bool> callback) {
  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerFormExtractionExcept(source);

  const FormData& browser_form = form_forest_.GetBrowserForm(form_id);
  auto* target = DriverOfFrame(browser_form.host_frame);
  callback(CHECK_DEREF(target), browser_form, field, old_value,
           formatting_only);
}

// Routing of events triggered by the browser.
//
// Below, `DriverOfFrame() == nullptr` does not necessarily indicate a bug and
// is therefore not NOTREACHED().
// The reason is that browser forms may be outdated and hence refer to frames
// that do not exist anymore.

base::flat_set<FieldGlobalId> AutofillDriverRouter::ApplyFormAction(
    AutofillDriver& source,
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, FieldType>& field_type_map,
    RoutedCallback<mojom::FormActionType,
                   mojom::ActionPersistence,
                   const std::vector<FormFieldData::FillData>&> callback) {
  // Since Undo only affects fields that were already filled, and only sets
  // values to fields to something that already existed in it prior to the
  // filling, it is okay to bypass the filling security checks and hence passing
  // `TrustAllOrigins()`.
  internal::FormForest::RendererForms renderer_forms =
      form_forest_.GetRendererFormsOfBrowserForm(
          data, action_type == mojom::FormActionType::kUndo
                    ? internal::FormForest::SecurityOptions::TrustAllOrigins()
                    : internal::FormForest::SecurityOptions(&triggered_origin,
                                                            &field_type_map));
  // Collect the fields per frame and emit a single fill operation per frame,
  // even if multiple renderer forms belong to the same iframe due to
  // flattening.
  base::flat_map<AutofillDriver*, std::vector<FormFieldData::FillData>>
      fields_of_driver;
  for (FormData& renderer_form : renderer_forms.renderer_forms) {
    if (auto* target = DriverOfFrame(renderer_form.host_frame)) {
      for (const FormFieldData& field : renderer_form.fields) {
        // Skip unsafe fields so that they do not get filled in the renderer.
        if (renderer_forms.safe_fields.contains(field.global_id())) {
          fields_of_driver[target].emplace_back(field);
        }
      }
    }
  }
  for (const auto& [target, fields] : fields_of_driver) {
    CHECK(!fields.empty());
    callback(CHECK_DEREF(target), action_type, action_persistence, fields);
  }
  return renderer_forms.safe_fields;
}

void AutofillDriverRouter::ApplyFieldAction(
    AutofillDriver& source,
    mojom::FieldActionType action_type,
    mojom::ActionPersistence action_persistence,
    const FieldGlobalId& field,
    const std::u16string& value,
    RoutedCallback<mojom::FieldActionType,
                   mojom::ActionPersistence,
                   const FieldRendererId&,
                   const std::u16string&> callback) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(*target, action_type, action_persistence, field.renderer_id,
             value);
  }
}

void AutofillDriverRouter::ExtractForm(
    AutofillDriver& source,
    FormGlobalId form_id,
    BrowserFormHandler browser_form_handler,
    RoutedCallback<const FormRendererId&, RendererFormHandler> callback) {
  if (auto* target = DriverOfFrame(form_id.frame_token)) {
    // `renderer_form_handler` converts a received renderer `form` into a
    // browser form and passes that to `browser_form_handler`.
    // Binding `*this` and `*target` is safe because
    // - `*this` outlives `*target`, and
    // - `*target` outlives all pending callbacks of `*target`'s AutofillAgent.
    auto renderer_form_handler = base::BindOnce(
        [](raw_ref<AutofillDriverRouter> self,
           raw_ref<AutofillDriver> response_source,
           BrowserFormHandler browser_form_handler,
           const std::optional<FormData>& form) {
          if (!form) {
            std::move(browser_form_handler).Run(nullptr, std::nullopt);
            return;
          }
          self->form_forest_.UpdateTreeOfRendererForm(*form, *response_source);
          const FormData& browser_form =
              self->form_forest_.GetBrowserForm(form->global_id());
          auto* response_target = self->DriverOfFrame(browser_form.host_frame);
          std::move(browser_form_handler).Run(response_target, browser_form);
        },
        raw_ref(*this), raw_ref(*target), std::move(browser_form_handler));
    callback(*target, form_id.renderer_id, std::move(renderer_form_handler));
  } else {
    std::move(browser_form_handler).Run(nullptr, std::nullopt);
  }
}

void AutofillDriverRouter::SendAutofillTypePredictionsToRenderer(
    AutofillDriver& source,
    const std::vector<FormDataPredictions>& browser_fdps,
    RoutedCallback<const std::vector<FormDataPredictions>&> callback) {
  // Splits each FrameDataPredictions according to the respective FormData's
  // renderer forms, and groups these FormDataPredictions by the renderer form's
  // frame. We uso "fdp" as abbreviation of FormDataPredictions.
  std::map<LocalFrameToken, std::vector<FormDataPredictions>> renderer_fdps;
  for (const FormDataPredictions& browser_fdp : browser_fdps) {
    // Builds an index of the field predictions by the field's global ID.
    std::map<FieldGlobalId, FormFieldDataPredictions> field_predictions;
    DCHECK_EQ(browser_fdp.data.fields.size(), browser_fdp.fields.size());
    for (size_t i = 0; i < std::min(browser_fdp.data.fields.size(),
                                    browser_fdp.fields.size());
         ++i) {
      field_predictions.emplace(browser_fdp.data.fields[i].global_id(),
                                browser_fdp.fields[i]);
    }

    // Builds the FormDataPredictions of each renderer form and groups them by
    // the renderer form's frame in |renderer_fdps|.
    internal::FormForest::RendererForms renderer_forms =
        form_forest_.GetRendererFormsOfBrowserForm(
            browser_fdp.data,
            {&browser_fdp.data.main_frame_origin, /*field_type_map=*/nullptr});
    for (FormData& renderer_form : renderer_forms.renderer_forms) {
      LocalFrameToken frame = renderer_form.host_frame;
      FormDataPredictions renderer_fdp;
      renderer_fdp.data = std::move(renderer_form);
      renderer_fdp.signature = browser_fdp.signature;
      renderer_fdp.alternative_signature = browser_fdp.alternative_signature;
      for (const FormFieldData& field : renderer_fdp.data.fields) {
        renderer_fdp.fields.push_back(
            std::move(field_predictions[field.global_id()]));
      }
      renderer_fdps[frame].push_back(std::move(renderer_fdp));
    }
  }

  // Sends the predictions of the renderer forms to the individual frames.
  for (const auto& p : renderer_fdps) {
    LocalFrameToken frame = p.first;
    const std::vector<FormDataPredictions>& renderer_fdp = p.second;
    if (auto* target = DriverOfFrame(frame)) {
      callback(*target, renderer_fdp);
    }
  }
}

void AutofillDriverRouter::RendererShouldAcceptDataListSuggestion(
    AutofillDriver& source,
    const FieldGlobalId& field,
    const std::u16string& value,
    RoutedCallback<const FieldRendererId&, const std::u16string&> callback) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(*target, field.renderer_id, value);
  }
}

void AutofillDriverRouter::RendererShouldClearPreviewedForm(
    AutofillDriver& source,
    RoutedCallback<> callback) {
  ForEachFrame(form_forest_, callback);
}

void AutofillDriverRouter::RendererShouldTriggerSuggestions(
    AutofillDriver& source,
    const FieldGlobalId& field,
    AutofillSuggestionTriggerSource trigger_source,
    RoutedCallback<const FieldRendererId&, AutofillSuggestionTriggerSource>
        callback) {
  if (AutofillDriver* target = DriverOfFrame(field.frame_token)) {
    callback(*target, field.renderer_id, trigger_source);
  }
}

void AutofillDriverRouter::RendererShouldSetSuggestionAvailability(
    AutofillDriver& source,
    const FieldGlobalId& field,
    mojom::AutofillSuggestionAvailability suggestion_availability,
    RoutedCallback<const FieldRendererId&,
                   mojom::AutofillSuggestionAvailability> callback) {
  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(*target, field.renderer_id, suggestion_availability);
  }
}

std::vector<FormData> AutofillDriverRouter::GetRendererForms(
    const FormData& browser_form) const {
  return form_forest_
      .GetRendererFormsOfBrowserForm(
          browser_form,
          internal::FormForest::SecurityOptions::TrustAllOrigins())
      .renderer_forms;
}

}  // namespace autofill