// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/form_autofill_util.h"

#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_label_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_option_element.h"
#include "third_party/blink/public/web/web_remote_frame.h"
#include "third_party/blink/public/web/web_select_element.h"
#include "third_party/blink/public/web/web_select_list_element.h"
#include "third_party/re2/src/re2/re2.h"

using blink::WebAutofillState;
using blink::WebDocument;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebLabelElement;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebSelectElement;
using blink::WebSelectListElement;
using blink::WebString;
using blink::WebVector;

namespace autofill::form_util {

struct ShadowFieldData {
  ShadowFieldData() = default;
  ShadowFieldData(ShadowFieldData&& other) = default;
  ShadowFieldData& operator=(ShadowFieldData&& other) = default;
  ShadowFieldData(const ShadowFieldData& other) = delete;
  ShadowFieldData& operator=(const ShadowFieldData& other) = delete;
  ~ShadowFieldData() = default;

  // If the form control is inside shadow DOM, then these lists will contain
  // id and name attributes of the parent shadow host elements. There may be
  // more than one if the form control is in nested shadow DOM.
  std::vector<std::u16string> shadow_host_id_attributes;
  std::vector<std::u16string> shadow_host_name_attributes;
};

namespace {

using LabelSource = FormFieldData::LabelSource;

// Maximal length of a button's title.
constexpr int kMaxLengthForSingleButtonTitle = 30;
// Maximal length of all button titles.
constexpr int kMaxLengthForAllButtonTitles = 200;

// Number of shadow roots to traverse upwards when looking for relevant forms
// and labels of an input element inside a shadow root.
constexpr size_t kMaxShadowLevelsUp = 2;

// Text features to detect form submission buttons. Features are selected based
// on analysis of real forms and their buttons.
// TODO(crbug.com/41429204): Consider to add more features (e.g. non-English
// features).
const char* const kButtonFeatures[] = {"button", "btn", "submit",
                                       "boton" /* "button" in Spanish */};

// Number of form neighbor nodes to traverse in search of four digit
// combinations on the webpage.
constexpr int kFormNeighborNodesToTraverse = 50;

// Maximum number of consecutive numbers to allow in the four digit combination
// matches.
constexpr int kMaxConsecutiveInFourDigitCombinationMatches = 2;

// Maximum number of four digit combination matches to find in the DOM.
constexpr size_t kMaxFourDigitCombinationMatches = 5;

// Constants to be passed to GetWebString<kConstant>().
constexpr std::string_view kAnchor = "a";
constexpr std::string_view kAutocomplete = "autocomplete";
constexpr std::string_view kAriaDescribedBy = "aria-describedby";
constexpr std::string_view kAriaLabel = "aria-label";
constexpr std::string_view kAriaLabelledBy = "aria-labelledby";
constexpr std::string_view kBold = "b";
constexpr std::string_view kBreak = "br";
constexpr std::string_view kButton = "button";
constexpr std::string_view kClass = "class";
constexpr std::string_view kColspan = "colspan";
constexpr std::string_view kDefinitionDescriptionTag = "dd";
constexpr std::string_view kDefinitionTermTag = "dt";
constexpr std::string_view kDiv = "div";
constexpr std::string_view kFieldset = "fieldset";
constexpr std::string_view kFont = "font";
constexpr std::string_view kFor = "for";
constexpr std::string_view kForm = "form";
constexpr std::string_view kFormControlSelector = "input, select, textarea";
constexpr std::string_view kId = "id";
constexpr std::string_view kIframe = "iframe";
constexpr std::string_view kImage = "img";
constexpr std::string_view kInput = "input";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kListItem = "li";
constexpr std::string_view kMeta = "meta";
constexpr std::string_view kName = "name";
constexpr std::string_view kNoScript = "noscript";
constexpr std::string_view kOption = "option";
constexpr std::string_view kParagraph = "p";
constexpr std::string_view kPlaceholder = "placeholder";
constexpr std::string_view kRole = "role";
constexpr std::string_view kScript = "script";
constexpr std::string_view kSpan = "span";
constexpr std::string_view kStrong = "strong";
constexpr std::string_view kSubmit = "submit";
constexpr std::string_view kTable = "table";
constexpr std::string_view kTableCell = "td";
constexpr std::string_view kTableHeader = "th";
constexpr std::string_view kTableRow = "tr";
constexpr std::string_view kTitle = "title";
constexpr std::string_view kType = "type";
constexpr std::string_view kValue = "value";

// Wrapper for frequently used WebString constants.
template <const std::string_view& string>
const WebString& GetWebString() {
  static const base::NoDestructor<WebString> web_string(
      WebString::FromUTF8(string));
  return *web_string;
}

template <const std::string_view& tag_name>
bool HasTagName(const WebElement& element) {
  return element.HasHTMLTagName(GetWebString<tag_name>());
}

template <const std::string_view& tag_name>
bool HasTagName(const WebNode& node) {
  return node.IsElementNode() && HasTagName<tag_name>(node.To<WebElement>());
}

template <const std::string_view& attribute>
bool HasAttribute(const WebElement& element) {
  return element.HasAttribute(GetWebString<attribute>());
}

template <const std::string_view& attribute>
WebString GetAttribute(const WebElement& element) {
  return element.GetAttribute(GetWebString<attribute>());
}

// Returns true if |node| is an element and it is a container type that
// InferLabelForElement() can traverse.
bool IsTraversableContainerElement(const WebNode& node) {
  if (!node.IsElementNode()) {
    return false;
  }

  const WebElement element = node.To<WebElement>();
  return HasTagName<kDefinitionDescriptionTag>(element) ||
         HasTagName<kDiv>(element) || HasTagName<kFieldset>(element) ||
         HasTagName<kListItem>(element) || HasTagName<kTableCell>(element) ||
         HasTagName<kTable>(element);
}

// Returns the colspan for a <td> / <th>. Defaults to 1.
size_t CalculateTableCellColumnSpan(const WebElement& element) {
  DCHECK(HasTagName<kTableCell>(element) || HasTagName<kTableHeader>(element));

  size_t span = 1;
  if (HasAttribute<kColspan>(element)) {
    std::u16string colspan = GetAttribute<kColspan>(element).Utf16();
    // Do not check return value to accept imperfect conversions.
    base::StringToSizeT(colspan, &span);
    // Handle overflow.
    if (span == std::numeric_limits<size_t>::max())
      span = 1;
    span = std::max(span, static_cast<size_t>(1));
  }

  return span;
}

// Appends |suffix| to |prefix| so that any intermediary whitespace is collapsed
// to a single space.  If |force_whitespace| is true, then the resulting string
// is guaranteed to have a space between |prefix| and |suffix|.  Otherwise, the
// result includes a space only if |prefix| has trailing whitespace or |suffix|
// has leading whitespace.
// A few examples:
//  * CombineAndCollapseWhitespace("foo", "bar", false)       -> "foobar"
//  * CombineAndCollapseWhitespace("foo", "bar", true)        -> "foo bar"
//  * CombineAndCollapseWhitespace("foo ", "bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", false)      -> "foo bar"
//  * CombineAndCollapseWhitespace("foo", " bar", true)       -> "foo bar"
//  * CombineAndCollapseWhitespace("foo   ", "   bar", false) -> "foo bar"
//  * CombineAndCollapseWhitespace(" foo", "bar ", false)     -> " foobar "
//  * CombineAndCollapseWhitespace(" foo", "bar ", true)      -> " foo bar "
const std::u16string CombineAndCollapseWhitespace(const std::u16string& prefix,
                                                  const std::u16string& suffix,
                                                  bool force_whitespace) {
  std::u16string prefix_trimmed;
  base::TrimPositions prefix_trailing_whitespace =
      base::TrimWhitespace(prefix, base::TRIM_TRAILING, &prefix_trimmed);

  // Recursively compute the children's text.
  std::u16string suffix_trimmed;
  base::TrimPositions suffix_leading_whitespace =
      base::TrimWhitespace(suffix, base::TRIM_LEADING, &suffix_trimmed);

  if (prefix_trailing_whitespace || suffix_leading_whitespace ||
      force_whitespace) {
    return prefix_trimmed + u" " + suffix_trimmed;
  }
  return prefix_trimmed + suffix_trimmed;
}

// This is a helper function for the FindChildText() function (see below).
// Search depth is limited with the |depth| parameter.
// |divs_to_skip| is a list of <div> tags to ignore if encountered.
std::u16string FindChildTextInner(const WebNode& node,
                                  int depth,
                                  const std::set<WebNode>& divs_to_skip) {
  if (depth <= 0 || !node) {
    return std::u16string();
  }

  // Skip over comments.
  if (node.IsCommentNode())
    return FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);

  if (!node.IsElementNode() && !node.IsTextNode())
    return std::u16string();

  // Ignore elements known not to contain inferable labels.
  bool skip_node = false;
  if (node.IsElementNode()) {
    const WebElement element = node.To<WebElement>();
    if (HasTagName<kOption>(element) ||
        (HasTagName<kDiv>(element) && base::Contains(divs_to_skip, node)) ||
        (element.IsFormControlElement() &&
         IsAutofillableElement(element.To<WebFormControlElement>()))) {
      return std::u16string();
    }
    skip_node = HasTagName<kScript>(element) || HasTagName<kNoScript>(element);
  }

  std::u16string node_text;

  if (!skip_node) {
    // Extract the text exactly at this node.
    node_text = node.NodeValue().Utf16();

    // Recursively compute the children's text.
    // Preserve inter-element whitespace separation.
    std::u16string child_text =
        FindChildTextInner(node.FirstChild(), depth - 1, divs_to_skip);
    bool add_space = node.IsTextNode() && node_text.empty();
    node_text = CombineAndCollapseWhitespace(node_text, child_text, add_space);
  }

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  std::u16string sibling_text =
      FindChildTextInner(node.NextSibling(), depth - 1, divs_to_skip);
  bool add_space = node.IsTextNode() && node_text.empty();
  node_text = CombineAndCollapseWhitespace(node_text, sibling_text, add_space);

  return node_text;
}

// Shared function for InferLabelFromPrevious() and InferLabelFromNext().
std::optional<InferredLabel> InferLabelFromSibling(
    const WebFormControlElement& element,
    bool forward) {
  std::u16string inferred_label;
  WebNode sibling = element;
  while (true) {
    sibling = forward ? sibling.NextSibling() : sibling.PreviousSibling();
    if (!sibling) {
      break;
    }

    // Skip over comments.
    if (sibling.IsCommentNode())
      continue;

    // Otherwise, only consider normal HTML elements and their contents.
    if (!sibling.IsElementNode() && !sibling.IsTextNode())
      break;

    // A label might be split across multiple "lightweight" nodes.
    // Coalesce any text contained in multiple consecutive
    //  (a) plain text nodes or
    //  (b) inline HTML elements that are essentially equivalent to text nodes.
    if (sibling.IsTextNode() || HasTagName<kBold>(sibling) ||
        HasTagName<kStrong>(sibling) || HasTagName<kSpan>(sibling) ||
        HasTagName<kFont>(sibling)) {
      std::u16string value = FindChildText(sibling);
      // A text node's value will be empty if it is for a line break.
      bool add_space = sibling.IsTextNode() && value.empty();
      if (forward) {
        inferred_label =
            CombineAndCollapseWhitespace(inferred_label, value, add_space);
      } else {
        inferred_label =
            CombineAndCollapseWhitespace(value, inferred_label, add_space);
      }
      continue;
    }

    // If we have identified a partial label and have reached a non-lightweight
    // element, consider the label to be complete.
    if (auto r = InferredLabel::BuildIfValid(inferred_label,
                                             LabelSource::kCombined)) {
      return r;
    }

    // <img> and <br> tags often appear between the input element and its
    // label text, so skip over them.
    if (HasTagName<kImage>(sibling) || HasTagName<kBreak>(sibling)) {
      continue;
    }

    // We only expect <p> and <label> tags to contain the full label text.
    bool has_label_tag = HasTagName<kLabel>(sibling);
    if (HasTagName<kParagraph>(sibling) || has_label_tag) {
      return InferredLabel::BuildIfValid(
          FindChildText(sibling),
          has_label_tag ? LabelSource::kLabelTag : LabelSource::kPTag);
    }

    break;
  }
  return InferredLabel::BuildIfValid(inferred_label, LabelSource::kCombined);
}

// Helper function to add a button's |title| to the |list|.
void AddButtonTitleToList(std::u16string title,
                          mojom::ButtonTitleType button_type,
                          ButtonTitleList* list) {
  title = base::CollapseWhitespace(std::move(title), false);
  if (title.empty()) {
    return;
  }
  list->emplace_back(std::move(title).substr(0, kMaxLengthForSingleButtonTitle),
                     button_type);
}

// Returns true iff |attribute| contains one of |kButtonFeatures|.
bool AttributeHasButtonFeature(const WebString& attribute) {
  if (attribute.IsNull())
    return false;
  std::string value = attribute.Utf8();
  base::ranges::transform(value, value.begin(), ::tolower);
  for (const char* const button_feature : kButtonFeatures) {
    if (value.find(button_feature, 0) != std::string::npos)
      return true;
  }
  return false;
}

// Returns true if |element|'s id, name or css class contain |kButtonFeatures|.
bool ElementAttributesHasButtonFeature(const WebElement& element) {
  return AttributeHasButtonFeature(GetAttribute<kId>(element)) ||
         AttributeHasButtonFeature(GetAttribute<kName>(element)) ||
         AttributeHasButtonFeature(GetAttribute<kClass>(element));
}

// Finds elements from |elements| that contains |kButtonFeatures| and appends it
// to the |list|. If |extract_value_attribute|, the "value" attribute is
// extracted as a button title. Otherwise, |WebElement::TextContent| (aka
// innerText in Javascript) is extracted as a title.
void FindElementsWithButtonFeatures(const WebElementCollection& elements,
                                    mojom::ButtonTitleType button_type,
                                    bool extract_value_attribute,
                                    ButtonTitleList* list) {
  for (WebElement item = elements.FirstItem(); item;
       item = elements.NextItem()) {
    if (!ElementAttributesHasButtonFeature(item))
      continue;
    std::u16string title =
        extract_value_attribute
            ? (HasAttribute<kValue>(item) ? GetAttribute<kValue>(item).Utf16()
                                          : std::u16string())
            : item.TextContent().Utf16();
    if (extract_value_attribute && title.empty())
      title = item.TextContent().Utf16();
    AddButtonTitleToList(std::move(title), button_type, list);
  }
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// a previous sibling of |element|,
// e.g. Some Text <input ...>
// or   Some <span>Text</span> <input ...>
// or   <p>Some Text</p><input ...>
// or   <label>Some Text</label> <input ...>
// or   Some Text <img><input ...>
// or   <b>Some Text</b><br/> <input ...>.
std::optional<InferredLabel> InferLabelFromPrevious(
    const WebFormControlElement& element) {
  return InferLabelFromSibling(element, /*forward=*/false);
}

// Same as InferLabelFromPrevious(), but in the other direction.
// Useful for cases like: <span><input type="checkbox">Label For Checkbox</span>
std::optional<InferredLabel> InferLabelFromNext(
    const WebFormControlElement& element) {
  return InferLabelFromSibling(element, /*forward=*/true);
}

// Helper for |InferLabelForElement()| that infers a label, if possible, from
// the placeholder text. e.g. <input placeholder="foo">
std::optional<InferredLabel> InferLabelFromPlaceholder(
    const WebFormControlElement& element) {
  if (HasAttribute<kPlaceholder>(element)) {
    return InferredLabel::BuildIfValid(
        GetAttribute<kPlaceholder>(element).Utf16(), LabelSource::kPlaceHolder);
  }
  return std::nullopt;
}

std::optional<InferredLabel> InferLabelFromAriaLabel(
    const WebFormControlElement& element) {
  return InferredLabel::BuildIfValid(
      GetAriaLabel(element.GetDocument(), element), LabelSource::kAriaLabel);
}

// Detects a label declared after the `element`, which is visually positioned
// above the element (usually using CSS). Such labels often act as
// placeholders. E.g.
// <div>
//  <input>
//  <span>Placeholder</span>
// </div>
// We want to consider placeholders which are either positioned over the input
// element or placed on the top left (or top right in RTL languages) of the
// input element (they need to overlap a bit). We want to disregard elements
// that are primarily below the input element (even if they overlap) because
// that place is often used to indicate incorrect inputs.
std::optional<InferredLabel> InferLabelFromOverlayingSuccessor(
    const WebFormControlElement& element) {
  WebNode next = element.NextSibling();
  while (next && !next.IsElementNode()) {
    next = next.NextSibling();
  }
  if (next) {
    gfx::Rect element_bounds = element.BoundsInWidget();
    gfx::Rect next_bounds = next.To<WebElement>().BoundsInWidget();
    // Reduce size by 1 pixel in all dimensions to resolve intersection due to
    // rounding errors.
    next_bounds.Inset(1);
    // We don't rely on element_bounds.Contains(next_bounds) because some
    // websites render the label partially above the input element.
    // We check the following conditions: 1) horizontally we want the `next`
    // element to be contained by `element`
    //    to consider `next` a label:
    //    |<----- element ----->|
    //     |<----- next ------>|
    // 2) vertically we often see three cases:
    //              (a)
    //             -----
    //               ^       (b)
    //   --------    |      -----
    //      ^       next      ^
    //      |        |        |
    //      |        v        |      (c) (not a placeholder)
    //   element   -----     next   -----
    //      |                 |       ^
    //      |                 |       |
    //      v                 v      next
    //   --------           -----     |
    //                                v
    //                              -----
    // a) a label is presented on the top left corner of an input element,
    //    possibly even exceeding it a bit.
    // b) a label is presented inside the input element.
    // c) an error message is presented at the bottom of an input element.
    if (!next_bounds.IsEmpty() &&
        // `next` needs to overlap `element` to be even considered.
        element_bounds.Intersects(next_bounds) &&
        // `next` must be horizontally contained.
        next_bounds.x() >= element_bounds.x() &&
        next_bounds.right() <= element_bounds.right() &&
        // bottom of `next` does not exceed the bounds of `element` because that
        // may represent an error label (case c above). The top of `next` may,
        // however exceed the `element` (case a above), so that condition is not
        // tested.
        !(next_bounds.bottom() > element_bounds.bottom())) {
      return InferredLabel::BuildIfValid(FindChildText(next),
                                         LabelSource::kOverlayingLabel);
    }
  }
  return std::nullopt;
}

// Helper for |InferLabelForElement()| that infers a label, from
// the value attribute when it is present and user has not typed in (if
// element's value attribute is same as the element's value).
std::optional<InferredLabel> InferLabelFromValueAttribute(
    const WebFormControlElement& element) {
  if (HasAttribute<kValue>(element) &&
      GetAttribute<kValue>(element) == element.Value()) {
    return InferredLabel::BuildIfValid(GetAttribute<kValue>(element).Utf16(),
                                       LabelSource::kValue);
  }
  return std::nullopt;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// surrounding table structure,
// e.g. <tr><td>Some Text</td><td><input ...></td></tr>
// or   <tr><th>Some Text</th><td><input ...></td></tr>
// or   <tr><td><b>Some Text</b></td><td><b><input ...></b></td></tr>
// or   <tr><th><b>Some Text</b></th><td><b><input ...></b></td></tr>
// `cell` represents the <td> tag containing the input element.
std::optional<InferredLabel> InferLabelFromTableColumn(const WebNode& cell) {
  DCHECK(HasTagName<kTableCell>(cell));
  // Check all previous siblings, skipping non-element nodes, until we find a
  // non-empty text block.
  std::optional<InferredLabel> r;
  WebNode previous = cell.PreviousSibling();
  while (!r && previous) {
    if (HasTagName<kTableCell>(previous) ||
        HasTagName<kTableHeader>(previous)) {
      r = InferredLabel::BuildIfValid(FindChildText(previous),
                                      LabelSource::kTdTag);
    }
    previous = previous.PreviousSibling();
  }
  return r;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// surrounding table structure.
//
// If there are multiple cells and the row with the input matches up with the
// previous row, then look for a specific cell within the previous row.
// e.g. <tr><td>Input 1 label</td><td>Input 2 label</td></tr>
//      <tr><td><input name="input 1"></td><td><input name="input2"></td></tr>
//
// Otherwise, just look in the entire previous row.
// e.g. <tr><td>Some Text</td></tr><tr><td><input ...></td></tr>
// `cell` represents the <td> tag containing the input element.
std::optional<InferredLabel> InferLabelFromTableRow(const WebNode& cell) {
  DCHECK(HasTagName<kTableCell>(cell));

  // Count the cell holding the input element.
  size_t cell_count = CalculateTableCellColumnSpan(cell.To<WebElement>());
  size_t cell_position = 0;
  size_t cell_position_end = cell_count - 1;

  // Count cells to the left to figure out |element|'s cell's position.
  for (WebNode cell_it = cell.PreviousSibling(); cell_it;
       cell_it = cell_it.PreviousSibling()) {
    if (HasTagName<kTableCell>(cell_it)) {
      cell_position += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Count cells to the right.
  for (WebNode cell_it = cell.NextSibling(); cell_it;
       cell_it = cell_it.NextSibling()) {
    if (HasTagName<kTableCell>(cell_it)) {
      cell_count += CalculateTableCellColumnSpan(cell_it.To<WebElement>());
    }
  }

  // Combine left + right.
  cell_count += cell_position;
  cell_position_end += cell_position;

  // Find the current row.
  WebNode parent = cell.ParentNode();
  while (parent && !HasTagName<kTableRow>(parent)) {
    parent = parent.ParentNode();
  }
  if (!parent) {
    return std::nullopt;
  }

  // Now find the previous row.
  WebNode row_it = parent.PreviousSibling();
  while (row_it && !HasTagName<kTableRow>(row_it)) {
    row_it = row_it.PreviousSibling();
  }

  // If there exists a previous row, check its cells and size. If they align
  // with the current row, infer the label from the cell above.
  if (row_it) {
    WebNode matching_cell;
    size_t prev_row_count = 0;
    WebNode prev_row_it = row_it.FirstChild();
    while (prev_row_it) {
      if (prev_row_it.IsElementNode()) {
        WebElement prev_row_element = prev_row_it.To<WebElement>();
        if (prev_row_element.HasHTMLTagName(GetWebString<kTableCell>()) ||
            prev_row_element.HasHTMLTagName(GetWebString<kTableHeader>())) {
          size_t span = CalculateTableCellColumnSpan(prev_row_element);
          size_t prev_row_count_end = prev_row_count + span - 1;
          if (prev_row_count == cell_position &&
              prev_row_count_end == cell_position_end) {
            matching_cell = prev_row_it;
          }
          prev_row_count += span;
        }
      }
      prev_row_it = prev_row_it.NextSibling();
    }
    if ((cell_count == prev_row_count) && matching_cell) {
      if (auto r = InferredLabel::BuildIfValid(FindChildText(matching_cell),
                                               LabelSource::kTdTag)) {
        return r;
      }
    }
  }

  // If there is no previous row, or if the previous row and current row do not
  // align, check all previous siblings, skipping non-element nodes, until we
  // find a non-empty text block.
  WebNode previous = parent.PreviousSibling();
  std::optional<InferredLabel> r;
  while (!r && previous) {
    if (HasTagName<kTableRow>(previous)) {
      r = InferredLabel::BuildIfValid(FindChildText(previous),
                                      LabelSource::kTdTag);
    }
    previous = previous.PreviousSibling();
  }
  return r;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// a surrounding div table,
// e.g. <div>Some Text<span><input ...></span></div>
// e.g. <div>Some Text</div><div><input ...></div>
//
// Contrary to the other InferLabelFrom* functions, this functions walks up
// the DOM tree from the original input, instead of down from the surrounding
// tag. While doing so, if a <label> or text node sibling are found along the
// way, a label is inferred from them directly. For example, <div>First
// name<div><input></div>Last name<div><input></div></div> infers "First name"
// and "Last name" for the two inputs, respectively, by picking up the text
// nodes on the way to the surrounding div. Without doing so, the label of both
// inputs becomes "First nameLast name".
std::optional<InferredLabel> InferLabelFromDivTable(
    const WebFormControlElement& element) {
  WebNode node = element.ParentNode();
  bool looking_for_parent = true;
  std::set<WebNode> divs_to_skip;

  // Search the sibling and parent <div>s until we find a candidate label.
  std::optional<InferredLabel> r;
  while (!r && node) {
    if (HasTagName<kDiv>(node)) {
      r = InferredLabel::BuildIfValid(
          looking_for_parent ? FindChildTextWithIgnoreList(node, divs_to_skip)
                             : FindChildText(node),
          LabelSource::kDivTable);

      // Avoid sibling DIVs that contain autofillable fields.
      if (!looking_for_parent && r) {
        WebElement result_element =
            node.QuerySelector(GetWebString<kFormControlSelector>());
        if (result_element) {
          r = std::nullopt;
          divs_to_skip.insert(node);
        }
      }

      looking_for_parent = false;
    } else if (!looking_for_parent) {
      // Infer a label from text nodes and unassigned <label> siblings.
      if (node.IsTextNode() ||
          (HasTagName<kLabel>(node) &&
           !node.To<WebLabelElement>().CorrespondingControl())) {
        r = InferredLabel::BuildIfValid(FindChildText(node),
                                        LabelSource::kDivTable);
      }
    } else if (IsTraversableContainerElement(node)) {
      // If the element is in a non-div container, its label most likely is too.
      break;
    }

    if (!node.PreviousSibling()) {
      // If there are no more siblings, continue walking up the tree.
      looking_for_parent = true;
    }

    node = looking_for_parent ? node.ParentNode() : node.PreviousSibling();
  }
  return r;
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// a surrounding definition list,
// e.g. <dl><dt>Some Text</dt><dd><input ...></dd></dl>
// e.g. <dl><dt><b>Some Text</b></dt><dd><b><input ...></b></dd></dl>
std::optional<InferredLabel> InferLabelFromDefinitionList(const WebNode& dd) {
  DCHECK(HasTagName<kDefinitionDescriptionTag>(dd));

  // Skip by any intervening text nodes.
  WebNode previous = dd.PreviousSibling();
  while (previous && previous.IsTextNode()) {
    previous = previous.PreviousSibling();
  }

  if (!previous || !HasTagName<kDefinitionTermTag>(previous)) {
    return std::nullopt;
  }
  return InferredLabel::BuildIfValid(FindChildText(previous),
                                     LabelSource::kDdTag);
}

// Helper for `InferLabelForElement()` that infers a label, if possible, from
// the first surrounding <label>, <div>, <td>, <dd> or <li> tag (if any).
// See `FindChildText()`, `InferLabelFromDivTable()`,
// `InferLabelFromTableColumn()`, `InferLabelFromTableRow()` and
// `InferLabelFromDefinitionList()` for examples how a label is extracted from
// the different tags.
std::optional<InferredLabel> InferLabelFromAncestors(
    const WebFormControlElement& element) {
  std::set<std::string> seen_tag_names;
  WebNode parent = element;
  while ((parent = parent.ParentNode())) {
    if (!parent.IsElementNode())
      continue;

    std::string tag_name = parent.To<WebElement>().TagName().Utf8();
    if (base::Contains(seen_tag_names, tag_name))
      continue;
    seen_tag_names.insert(tag_name);

    std::optional<InferredLabel> r;
    if (tag_name == "LABEL") {
      r = InferredLabel::BuildIfValid(FindChildText(parent),
                                      LabelSource::kLabelTag);
    } else if (tag_name == "DIV") {
      r = InferLabelFromDivTable(element);
    } else if (tag_name == "TD") {
      r = InferLabelFromTableColumn(parent);
      if (!r) {
        r = InferLabelFromTableRow(parent);
      }
    } else if (tag_name == "DD") {
      r = InferLabelFromDefinitionList(parent);
    } else if (tag_name == "LI") {
      r = InferredLabel::BuildIfValid(FindChildText(parent),
                                      LabelSource::kLiTag);
    } else if (tag_name == "FIELDSET") {
      break;
    }
    if (r) {
      return r;
    }
  }
  return std::nullopt;
}

// Removes the duplicate titles and limits totals length. The order of the list
// is preserved as first elements are more reliable features than following
// ones.
void RemoveDuplicatesAndLimitTotalLength(ButtonTitleList* result) {
  std::set<ButtonTitleInfo> already_added;
  ButtonTitleList unique_titles;
  int total_length = 0;
  for (auto title : *result) {
    if (already_added.find(title) != already_added.end())
      continue;
    already_added.insert(title);

    total_length += title.first.length();
    if (total_length > kMaxLengthForAllButtonTitles) {
      int new_length =
          title.first.length() - (total_length - kMaxLengthForAllButtonTitles);
      title.first = std::move(title.first).substr(0, new_length);
    }
    unique_titles.push_back(std::move(title));

    if (total_length >= kMaxLengthForAllButtonTitles) {
      break;
    }
  }
  *result = std::move(unique_titles);
}

// Return button titles with highest priority based on credibility of their HTML
// tags and attributes.
ButtonTitleList InferButtonTitlesForForm(const WebFormElement& web_form) {
  // Different button types have different credibility of being the main button.
  // Highest - <input type='submit'>, <button type='submit'>, <button>.
  // Moderate - <input type='button'> <button type='button'>.
  // Least - <a>, <div>. <span> with attributes having button features.
  ButtonTitleList highest_priority_buttons;
  ButtonTitleList moderate_priority_buttons;

  WebElementCollection input_elements =
      web_form.GetElementsByHTMLTagName(GetWebString<kInput>());
  for (WebElement item = input_elements.FirstItem(); item;
       item = input_elements.NextItem()) {
    DCHECK(item.IsFormControlElement());
    WebFormControlElement control_element = item.To<WebFormControlElement>();
    blink::mojom::FormControlType type =
        control_element.FormControlTypeForAutofill();
    bool is_submit_type = type == blink::mojom::FormControlType::kInputSubmit ||
                          type == blink::mojom::FormControlType::kButtonSubmit;
    bool is_button_type = type == blink::mojom::FormControlType::kInputButton ||
                          type == blink::mojom::FormControlType::kButtonButton;
    if (!is_submit_type && !is_button_type) {
      continue;
    }
    std::u16string title = control_element.Value().Utf16();
    AddButtonTitleToList(
        std::move(title),
        is_submit_type ? mojom::ButtonTitleType::INPUT_ELEMENT_SUBMIT_TYPE
                       : mojom::ButtonTitleType::INPUT_ELEMENT_BUTTON_TYPE,
        is_submit_type ? &highest_priority_buttons
                       : &moderate_priority_buttons);
  }

  WebElementCollection button_elements =
      web_form.GetElementsByHTMLTagName(GetWebString<kButton>());
  for (WebElement item = button_elements.FirstItem(); item;
       item = button_elements.NextItem()) {
    const WebString& type_attribute = GetAttribute<kType>(item);
    if (!type_attribute.IsNull() && type_attribute != GetWebString<kButton>() &&
        type_attribute != GetWebString<kSubmit>()) {
      // Neither type='submit' nor type='button'. Skip this button.
      continue;
    }

    bool is_submit_type =
        type_attribute.IsNull() || type_attribute == GetWebString<kSubmit>();
    std::u16string title = item.TextContent().Utf16();
    AddButtonTitleToList(
        std::move(title),
        is_submit_type ? mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE
                       : mojom::ButtonTitleType::BUTTON_ELEMENT_BUTTON_TYPE,
        is_submit_type ? &highest_priority_buttons
                       : &moderate_priority_buttons);
  }

  if (!highest_priority_buttons.empty()) {
    RemoveDuplicatesAndLimitTotalLength(&highest_priority_buttons);
    return highest_priority_buttons;
  }
  if (!moderate_priority_buttons.empty()) {
    RemoveDuplicatesAndLimitTotalLength(&moderate_priority_buttons);
    return moderate_priority_buttons;
  }

  ButtonTitleList least_priority_buttons;
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kAnchor>()),
      mojom::ButtonTitleType::HYPERLINK,
      /*extract_value_attribute=*/true, &least_priority_buttons);
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kDiv>()),
      mojom::ButtonTitleType::DIV,
      /*extract_value_attribute=*/false, &least_priority_buttons);
  FindElementsWithButtonFeatures(
      web_form.GetElementsByHTMLTagName(GetWebString<kSpan>()),
      mojom::ButtonTitleType::SPAN,
      /*extract_value_attribute=*/false, &least_priority_buttons);
  RemoveDuplicatesAndLimitTotalLength(&least_priority_buttons);
  return least_priority_buttons;
}

// Returns the list items for the passed-in <select> or <selectlist>.
WebVector<WebElement> GetListItemsForSelectOrSelectList(
    const WebFormControlElement& element) {
  if (IsSelectElement(element)) {
    return element.To<WebSelectElement>().GetListItems();
  } else {
    DCHECK(IsSelectListElement(element));
    return element.To<WebSelectListElement>().GetListItems();
  }
}

// Fills `options` with the <option> elements present in `option_elements`.
void FilterOptionElementsAndGetOptionStrings(
    const WebVector<WebElement>& option_elements,
    std::vector<SelectOption>* options) {
  options->clear();

  // Constrain the maximum list length to prevent a malicious site from DOS'ing
  // the browser, without entirely breaking autocomplete for some extreme
  // legitimate sites: http://crbug.com/49332 and http://crbug.com/363094
  if (option_elements.size() > kMaxListSize) {
    return;
  }

  options->reserve(option_elements.size());
  for (const auto& option_element : option_elements) {
    if (HasTagName<kOption>(option_element)) {
      const WebOptionElement option = option_element.To<WebOptionElement>();
      std::u16string content = option.GetText().Utf16();
      if (content.empty()) {
        content = GetAriaLabel(option_element.GetDocument(), option_element);
      }
      options->push_back(
          {.value = option.Value().Utf16().substr(0, kMaxStringLength),
           .content = content.substr(0, kMaxStringLength)});
    }
  }
}

bool ShouldSkipFillField(const FormFieldData::FillData& field,
                         const WebFormControlElement& element,
                         bool is_initiating_element) {
  enum class SkipReason {
    kUnfillable = 0,
    kNoValueToFill = 1,
    kPreviouslyAutofilled = 2,
    kUserEditedText = 3,
    kUserEditedSelect = 4,
    kMaxValue = kUserEditedSelect
  };
  constexpr char kSkipReasonHistogram[] = "Autofill.RendererFillSkipReason";
  // Skip all checkable or non-modifiable elements, except select fields because
  // some synthetic select element use a hidden select element.
  if (!IsAutofillableElement(element) || !element.IsEnabled() ||
      element.IsReadOnly() || IsCheckableElement(element) ||
      (!IsWebElementFocusableForAutofill(element) &&
       !IsSelectElement(element))) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kUnfillable);
    return true;
  }
  // Skip if there is no value to fill.
  if (field.value.empty() || !field.is_autofilled) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kNoValueToFill);
    return true;
  }
  if (is_initiating_element) {
    return false;
  }
  if (field.force_override) {
    return false;
  }
  // Skip filling previously autofilled fields unless autofill is instructed to
  // override it.
  if (element.IsAutofilled()) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kPreviouslyAutofilled);
    return true;
  }
  // A text field is skipped if it has a non-empty value that is entered by
  // the user and is NOT the value of the input field's "value" or "placeholder"
  // attribute. (The "value" attribute in <input value="foo"> indicates the
  // value of the input element at loading time, not its runtime value after the
  // user entered something into the field.)
  //
  // Some sites fill the fields with a formatting string like (___)-___-____.
  // To tell the difference between the values entered by the user nd the site,
  // we'll sanitize the value. If the sanitized value is empty, it means that
  // the site has filled the field, in this case, the field is not skipped.
  // Nevertheless the below condition does not hold for sites set the |kValue|
  // attribute to the user-input value.
  auto HasAttributeWithValue = [&element](const auto& attribute,
                                          const auto& value) {
    return element.HasAttribute(attribute) &&
           base::i18n::ToLower(element.GetAttribute(attribute).Utf16()) ==
               base::i18n::ToLower(value);
  };
  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  const std::u16string current_element_value = element.Value().Utf16();
  if ((IsAutofillableInputElement(input_element) ||
       IsTextAreaElement(element)) &&
      element.UserHasEditedTheField() &&
      !SanitizedFieldIsEmpty(current_element_value) &&
      !HasAttributeWithValue(GetWebString<kValue>(), current_element_value) &&
      !HasAttributeWithValue(GetWebString<kPlaceholder>(),
                             current_element_value)) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kUserEditedText);
    return true;
  }
  // Check if we should autofill/preview/clear a select element or leave it.
  if (IsSelectOrSelectListElement(element) && element.UserHasEditedTheField() &&
      !SanitizedFieldIsEmpty(current_element_value)) {
    base::UmaHistogramEnumeration(kSkipReasonHistogram,
                                  SkipReason::kUserEditedSelect);
    return true;
  }
  return false;
}

// Sets the |field|'s value to the value in |data|, and specifies the section
// for filled fields.  Also sets the "autofilled" attribute,
// causing the background to be blue.
void FillFormField(const FormFieldData::FillData& data,
                   bool is_initiating_node,
                   WebFormControlElement& field,
                   FieldDataManager& field_data_manager) {
  CHECK(!IsCheckableElement(field));
  WebInputElement input_element = field.DynamicTo<WebInputElement>();
  WebAutofillState new_autofill_state = data.is_autofilled
                                            ? WebAutofillState::kAutofilled
                                            : WebAutofillState::kNotFilled;

  std::u16string value = data.value;
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    value = std::move(value).substr(0, input_element.MaxLength());
  }

  if (IsTextInput(input_element)) {
    field_data_manager.UpdateFieldDataMap(GetFieldRendererId(field), value,
                                          FieldPropertiesFlags::kAutofilled);
  }
  field.SetAutofillValue(WebString::FromUTF16(value), new_autofill_state);
  // Changing the field's value might trigger JavaScript, which is capable of
  // destroying the frame.
  if (!field.GetDocument().GetFrame()) {
    return;
  }

  if (is_initiating_node &&
      (IsTextInput(input_element) || IsMonthInput(input_element) ||
       IsTextAreaElement(field))) {
    auto length = base::checked_cast<unsigned>(field.Value().length());
    field.SetSelectionRange(length, length);
    // selectionchange event is capable of destroying the frame.
    if (!field.GetDocument().GetFrame()) {
      return;
    }
    // Clear the current IME composition (the underline), if there is one.
    field.GetDocument().GetFrame()->UnmarkText();
  }
}

// Sets the |field|'s "suggested" (non JS visible) value to the value in |data|.
// Also sets the "autofilled" attribute, causing the background to be blue.
void PreviewFormField(const FormFieldData::FillData& data,
                      WebFormControlElement& field,
                      FieldDataManager& field_data_manager) {
  CHECK(!IsCheckableElement(field));
  // Preview input, textarea and select fields. For input fields, excludes
  // checkboxes and radio buttons, as there is no provision for
  // setSuggestedCheckedValue in WebInputElement.
  WebInputElement input_element = field.DynamicTo<WebInputElement>();
  WebAutofillState new_autofill_state = data.is_autofilled
                                            ? WebAutofillState::kPreviewed
                                            : WebAutofillState::kNotFilled;
  if (IsTextInput(input_element) || IsMonthInput(input_element)) {
    // If the maxlength attribute contains a negative value, maxLength()
    // returns the default maxlength value.
    input_element.SetSuggestedValue(
        WebString::FromUTF16(data.value.substr(0, input_element.MaxLength())));
    input_element.SetAutofillState(new_autofill_state);
  } else if (IsTextAreaElement(field) || IsSelectOrSelectListElement(field)) {
    field.SetSuggestedValue(WebString::FromUTF16(data.value));
    field.SetAutofillState(new_autofill_state);
  }
}

// A less-than comparator for FormFieldData's pointer by their FieldRendererId.
// It also supports direct comparison of a FieldRendererId with a FormFieldData
// pointer.
struct CompareByRendererId {
  using is_transparent = void;
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(f.first && g.first);
    return f.first->renderer_id() < g.first->renderer_id();
  }
  bool operator()(const FieldRendererId f,
                  const std::pair<FormFieldData*, ShadowFieldData>& g) const {
    DCHECK(g.first);
    return f < g.first->renderer_id();
  }
  bool operator()(const std::pair<FormFieldData*, ShadowFieldData>& f,
                  FieldRendererId g) const {
    DCHECK(f.first);
    return f.first->renderer_id() < g;
  }
};

// Searches |field_set| for a unique field with name |field_name|. If there is
// none or more than one field with that name, the fields' shadow hosts' name
// and id attributes are tested, and the first match is returned. Returns
// nullptr if no match was found.
FormFieldData* SearchForFormControlByName(
    const std::u16string& field_name,
    const base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                         CompareByRendererId>& field_set,
    LabelSource& label_source) {
  if (field_name.empty())
    return nullptr;

  auto get_field_name = [](const auto& p) { return p.first->name(); };
  auto it = base::ranges::find(field_set, field_name, get_field_name);
  auto end = field_set.end();
  if (it == end ||
      base::ranges::find(it + 1, end, field_name, get_field_name) != end) {
    auto ShadowHostHasTargetName = [&](const auto& p) {
      return base::Contains(p.second.shadow_host_name_attributes, field_name) ||
             base::Contains(p.second.shadow_host_id_attributes, field_name);
    };
    it = base::ranges::find_if(field_set, ShadowHostHasTargetName);
    if (it != end) {
      label_source =
          base::Contains(it->second.shadow_host_name_attributes, field_name)
              ? LabelSource::kForShadowHostName
              : LabelSource::kForShadowHostId;
    }
  } else {
    label_source = LabelSource::kForName;
  }
  return it != end ? it->first : nullptr;
}

// Considers all <label> descendents of `root`, looks at their corresponding
// control and matches them to the fields in `field_set`. The corresponding
// control is either a descendent of the label or an input specified by id in
// the label's for-attribute.
// In case no corresponding control exists, but a for-attribute is specified,
// we look for fields with matching name as a fallback. Moreover, the ids and
// names of shadow root ancestors of the fields are considered as a fallback.
void MatchLabelsAndFields(
    const WebDocument& root,
    const base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                         CompareByRendererId>& field_set) {
  WebElementCollection labels =
      root.GetElementsByHTMLTagName(GetWebString<kLabel>());
  DCHECK(labels);

  for (WebElement item = labels.FirstItem(); item; item = labels.NextItem()) {
    WebLabelElement label = item.To<WebLabelElement>();
    WebElement control = label.CorrespondingControl();
    FormFieldData* field_data = nullptr;
    LabelSource label_source = LabelSource::kForId;

    if (!control) {
      // Sometimes site authors will incorrectly specify the corresponding
      // field element's name rather than its id, so we compensate here.
      field_data = SearchForFormControlByName(GetAttribute<kFor>(label).Utf16(),
                                              field_set, label_source);
    } else if (control.IsFormControlElement()) {
      WebFormControlElement form_control = control.To<WebFormControlElement>();
      if (form_control.FormControlTypeForAutofill() ==
          blink::mojom::FormControlType::kInputHidden) {
        continue;
      }
      // Typical case: look up |field_data| in |field_set|.
      auto iter = field_set.find(GetFieldRendererId(form_control));
      if (iter == field_set.end())
        continue;
      field_data = iter->first;
    }

    // Skip `label` if we could not find an associated form control.
    if (!field_data)
      continue;

    std::u16string label_text = FindChildText(label);
    if (label_text.empty()) {
      if (HasAttribute<kFor>(label)) {
        continue;
      }
      DCHECK(control && control.IsFormControlElement());
      // An associated form control was found, but the `label` does not have a
      // for-attribute, so the form control must be a descendant of the `label`.
      // Since `FindChildText()` stops at autofillable elements, the
      // `label_text` can be empty if the "text" is declared behind the <input>.
      // For example:
      // <label>
      //  <input>
      //  text
      // </label>
      // Thus, consider text behind the <input> as a fallback.
      // Since associated labels are counted as `kFor`, the source is ignored.
      if (auto inferred_label =
              InferLabelFromNext(control.To<WebFormControlElement>())) {
        label_text = inferred_label->label;
      }
      if (label_text.empty()) {
        continue;
      }
    }
    // Concatenate labels because some sites might have multiple label
    // candidates.
    if (!field_data->label().empty()) {
      field_data->set_label(field_data->label() + u" ");
    }
    field_data->set_label(field_data->label() + label_text);
    field_data->set_label_source(label_source);
  }
}

bool IsAdIframe(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  WebFrame* iframe = WebFrame::FromFrameOwnerElement(element);
  return iframe && iframe->IsAdFrame();
}

// Indicates if an iframe |element| is considered actually visible to the user.
//
// This function is not intended to implement a perfect visibility check. It
// rather aims to strike balance between cheap tests and filtering invisible
// frames, which can then be skipped during parsing.
//
// The current visibility check requires focusability and a sufficiently large
// bounding box. Thus, particularly elements with "visibility: invisible",
// "display: none", and "width: 0; height: 0" are considered invisible.
//
// Future potential improvements include:
// * Detect potential visibility of elements with "overflow: visible".
//   (See WebElement::GetScrollSize().)
// * Detect invisibility of elements with
//   - "position: absolute; {left,top,right,bottom}: -100px"
//   - "opacity: 0.0"
//   - "clip: rect(0,0,0,0)"
//
// TODO(crbug.com/40846971): This check is very similar to IsWebElementVisible()
// (see the documentation there for the subtle differences: zoom factor and
// scroll size). We can probably merge them but should do a Finch experiment
// about it.
bool IsVisibleIframe(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  // It is common for not-humanly-visible elements to have very small yet
  // positive bounds. The threshold of 10 pixels is chosen rather arbitrarily.
  constexpr int kMinPixelSize = 10;
  gfx::Rect bounds = element.BoundsInWidget();
  return IsWebElementFocusableForAutofill(element) &&
         bounds.width() > kMinPixelSize && bounds.height() > kMinPixelSize;
}

// A necessary condition for an iframe to be added to FormData::child_frames.
//
// We also extract invisible iframes for the following reason. An iframe may be
// invisible at page load (for example, when it contains parts of a credit card
// form and the user hasn't chosen a payment method yet). Autofill is not
// notified when the iframe becomes visible. That is, Autofill may have not
// re-extracted the main frame's form by the time the iframe has become visible
// and the user has focused a field in that iframe. This outdated form is
// missing the link in FormData::child_frames between the parent form and the
// iframe's form, which prevents Autofill from filling across frames.
//
// The current implementation extracts visible ad frames. Assuming IsAdIframe()
// has no false positives, we could omit the IsVisibleIframe() disjunct. We
// could even take this further and disable Autofill in ad frames.
//
// For further details, see crbug.com/1117028#c8 and crbug.com/1245631.
bool IsRelevantChildFrame(const WebElement& element) {
  DCHECK(element.HasHTMLTagName(GetWebString<kIframe>()));
  return !IsAdIframe(element) ||
         (!base::FeatureList::IsEnabled(
              features::kAutofillExtractOnlyNonAdFrames) &&
          IsVisibleIframe(element));
}

// Returns the <iframe> elements that are associated with `form_element`.
// An iframe is associated with `form_element` iff
// - if `form_element` is non-null:
//   `form_element` is the iframe's closest <form> ancestor
// - if `form_element` is null:
//   the iframe has no <form> ancestor.
std::vector<WebElement> GetIframeElements(const WebDocument& document,
                                          const WebFormElement& form_element) {
  std::vector<WebElement> relevant_iframes;
  WebElementCollection iframes =
      document.GetElementsByHTMLTagName(GetWebString<kIframe>());
  for (WebElement iframe = iframes.FirstItem(); iframe;
       iframe = iframes.NextItem()) {
    if (GetClosestAncestorFormElement(iframe) == form_element &&
        IsRelevantChildFrame(iframe)) {
      relevant_iframes.push_back(iframe);
    }
  }
  return relevant_iframes;
}

std::optional<FormData> ExtractFormDataWithFieldsAndFrames(
    const WebDocument& document,
    const WebFormElement& form_element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options) {
  FormData form = [&form_element]() {
    FormData form;
    if (!form_element) {
      DCHECK(form.renderer_id.is_null());
      DCHECK(form.main_frame_origin.opaque());
      form.is_action_empty = true;
      return form;
    }
    form.name = GetFormIdentifier(form_element);
    form.id_attribute = form_element.GetIdAttribute().Utf16();
    form.name_attribute = GetAttribute<kName>(form_element).Utf16();
    form.renderer_id = GetFormRendererId(form_element);
    form.action = GetCanonicalActionForForm(form_element);
    if (!form.action.is_valid()) {
      form.action = blink::WebStringToGURL(form_element.Action());
    }
    form.is_action_empty =
        form_element.Action().IsNull() || form_element.Action().IsEmpty();
    return form;
  }();

  std::vector<WebFormControlElement> control_elements =
      GetFormControlElements(document, form_element);
  std::vector<WebElement> iframe_elements =
      GetIframeElements(document, form_element);

  // Extracts fields from |control_elements| into `form->fields` and sets
  // `form->child_frames[i].predecessor` to the field index of the last field
  // that precedes the |i|th child frame.
  //
  // If `control_elements[i]` is autofillable, `fields_extracted[i]` is set to
  // true and the corresponding FormFieldData is appended to `form->fields`.
  //
  // After each iteration, `iframe_elements[next_iframe]` is the first iframe
  // that comes after `control_elements[i]`.
  //
  // After the loop,
  // - `form->fields` is completely populated;
  // - `form->child_frames` has the correct size and
  //   `form->child_frames[i].predecessor` is set to the correct value, but
  //   `form->child_frames[i].token` is not initialized yet.
  form.fields.reserve(control_elements.size());
  form.child_frames.resize(iframe_elements.size());

  std::vector<bool> fields_extracted(control_elements.size(), false);
  std::vector<ShadowFieldData> shadow_fields;
  for (size_t i = 0, next_iframe = 0; i < control_elements.size(); ++i) {
    const WebFormControlElement& control_element = control_elements[i];
    if (!IsAutofillableElement(control_element)) {
      continue;
    }
    form.fields.emplace_back();
    shadow_fields.emplace_back();
    WebFormControlElementToFormField(
        form_element, control_element, &field_data_manager, extract_options,
        &form.fields.back(), &shadow_fields.back());
    fields_extracted[i] = true;

    // Finds the last frame that precedes |control_element|.
    while (next_iframe < iframe_elements.size() &&
           !IsDOMPredecessor(control_element, iframe_elements[next_iframe],
                             form_element)) {
      ++next_iframe;
    }
    // The |next_frame|th frame precedes `control_element` and thus the last
    // added FormFieldData. The |k|th frames for |k| > |next_frame| may also
    // precede that FormFieldData. If they do not,
    // `form->child_frames[i].predecessor` will be updated in a later
    // iteration.
    for (size_t k = next_iframe; k < iframe_elements.size(); ++k) {
      form.child_frames[k].predecessor = form.fields.size() - 1;
    }
    if (form.fields.size() > kMaxExtractableFields) {
      return std::nullopt;
    }
    DCHECK_LE(form.fields.size(), control_elements.size());
  }

  // Extracts field labels from the <label for="..."> tags.
  // This is done by iterating through all <label>s and looking them up in the
  // `field_set` built below.
  // Iterating through the fields and looking at their `WebElement::Labels()`
  // unfortunately doesn't scale, as each call corresponds to a DOM traverse.
  std::vector<std::pair<FormFieldData*, ShadowFieldData>> items;
  DCHECK_EQ(form.fields.size(), shadow_fields.size());
  for (size_t i = 0; i < form.fields.size(); i++) {
    items.emplace_back(&form.fields[i], std::move(shadow_fields[i]));
  }
  base::flat_set<std::pair<FormFieldData*, ShadowFieldData>,
                 CompareByRendererId>
      field_set(std::move(items));

  // All `control_elements` share the same document. By providing it as the
  // `root` of `MatchLabelsAndFields()` all label tags are considered. This is
  // necessary to support label-for inference in unowned forms and in owned
  // forms utilizing the form-attribute.
  if (!control_elements.empty()) {
    MatchLabelsAndFields(control_elements[0].GetDocument(), field_set);
  }

  // Infers field labels from other tags or <labels> without for="...".
  DCHECK_EQ(control_elements.size(), fields_extracted.size());
  DCHECK_EQ(form.fields.size(),
            base::as_unsigned(base::ranges::count(fields_extracted, true)));
  for (size_t element_index = 0, field_index = 0;
       element_index < control_elements.size(); ++element_index) {
    if (!fields_extracted[element_index]) {
      continue;
    }
    const WebFormControlElement& control_element =
        control_elements[element_index];
    FormFieldData& field = form.fields[field_index++];
    if (field.label().empty()) {
      if (auto label = InferLabelForElement(control_element)) {
        field.set_label(std::move(label->label));
        field.set_label_source(label->source);
      }
    }
    field.set_label(std::move(field.label()).substr(0, kMaxStringLength));
  }

  // Extracts the frame tokens of |iframe_elements|.
  DCHECK_EQ(form.child_frames.size(), iframe_elements.size());
  for (size_t i = 0; i < iframe_elements.size(); ++i) {
    WebFrame* iframe = WebFrame::FromFrameOwnerElement(iframe_elements[i]);
    if (iframe && iframe->IsWebLocalFrame()) {
      form.child_frames[i].token = LocalFrameToken(
          iframe->ToWebLocalFrame()->GetLocalFrameToken().value());
    } else if (iframe && iframe->IsWebRemoteFrame()) {
      form.child_frames[i].token = RemoteFrameToken(
          iframe->ToWebRemoteFrame()->GetRemoteFrameToken().value());
    }
  }
  std::erase_if(form.child_frames, [](const auto& child_frame) {
    return absl::visit([](const auto& token) { return token.is_empty(); },
                       child_frame.token);
  });
  if (form.child_frames.size() > kMaxExtractableChildFrames) {
    form.child_frames.clear();
  }
  const bool success = (!form.fields.empty() || !form.child_frames.empty()) &&
                       form.fields.size() < kMaxExtractableFields;
  if (!success) {
    return std::nullopt;
  }

  base::UmaHistogramCounts100(!form_element
                                  ? "Autofill.ExtractFormUnowned.FieldCount"
                                  : "Autofill.ExtractFormOwned.FieldCount",
                              form.fields.size());
  return form;
}

// Returns if a script-modified username or credit card number is suitable to
// store in Password Manager/Autofill given `typed_value`.
bool ScriptModifiedUsernameOrCreditCardNumberAcceptable(
    const std::u16string& value,
    const std::u16string& typed_value,
    const FieldDataManager& field_data_manager) {
  // The minimal size of a field value that will be substring-matched.
  constexpr size_t kMinMatchSize = 3u;
  const auto lowercase = base::i18n::ToLower(value);
  const auto typed_lowercase = base::i18n::ToLower(typed_value);
  // If the page-generated value is just a completion of the typed value, that's
  // likely acceptable.
  if (lowercase.starts_with(typed_lowercase)) {
    return true;
  }
  if (typed_lowercase.size() >= kMinMatchSize &&
      lowercase.find(typed_lowercase) != std::u16string::npos) {
    return true;
  }

  // If the page-generated value comes from user typed or autofilled values in
  // other fields, that's also likely OK.
  return field_data_manager.FindMatchedValue(value);
}

// Returns whether `node` has a shadow-tree-including ancestor that is a
// `<form>`.
bool HasFormAncestor(WebNode node) {
  node = node.ParentOrShadowHostNode();
  while (node) {
    if (HasTagName<kForm>(node)) {
      return true;
    }
    node = node.ParentOrShadowHostNode();
  }
  return false;
}

// Returns the unowned form control elements of `document`. Note that if
// `blink::features::kAutofillIncludeShadowDomInUnassociatedListedElements` is
// enabled, unassociated elements need not be unowned: A form control element
// may be unassociated inside its Shadow DOM, but owned (in the Autofill sense)
// by a <form> containing the shadow host.
std::vector<WebFormControlElement> GetUnownedFormControlElements(
    const WebDocument& document) {
  std::vector<WebFormControlElement> form_control_elements =
      document.UnassociatedFormControls().ReleaseVector();
  if (base::FeatureList::IsEnabled(
          blink::features::
              kAutofillIncludeShadowDomInUnassociatedListedElements)) {
    std::erase_if(form_control_elements, [](const WebFormControlElement& e) {
      return e.OwnerShadowHost() && HasFormAncestor(e);
    });
  }
  return form_control_elements;
}

}  // namespace

InferredLabel::InferredLabel(std::u16string label, LabelSource source)
    : label(std::move(label)), source(source) {}

// static
std::optional<InferredLabel> InferredLabel::BuildIfValid(std::u16string label,
                                                         LabelSource source) {
  // List of characters a label can't be entirely made of (this list can grow).
  const std::u16string_view invalid_chars =
      base::FeatureList::IsEnabled(
          features::kAutofillConsiderPhoneNumberSeparatorsValidLabels)
          ? u"*:"
          : u"*:-\u2013()";  // U+2013 is the En Dash "–".
  auto is_valid_label_character = [&invalid_chars](char16_t c) {
    return !base::Contains(invalid_chars, c) &&
           !base::Contains(std::u16string_view(base::kWhitespaceUTF16), c);
  };
  if (base::ranges::any_of(label, is_valid_label_character)) {
    base::TrimWhitespace(label, base::TRIM_ALL, &label);
    return InferredLabel{std::move(label), source};
  }
  return std::nullopt;
}

std::vector<WebElement> GetWebElementsFromIdList(const WebDocument& document,
                                                 const WebString& id_list) {
  std::vector<WebElement> web_elements;
  std::u16string id_list_utf16 = id_list.Utf16();
  for (std::u16string_view id : base::SplitStringPiece(
           id_list_utf16, base::kWhitespaceUTF16, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    web_elements.push_back(document.GetElementById(WebString(id)));
  }
  return web_elements;
}

std::string GetAutocompleteAttribute(const WebElement& element) {
  std::string autocomplete_attribute =
      GetAttribute<kAutocomplete>(element).Utf8();
  if (autocomplete_attribute.size() > kMaxStringLength) {
    // Discard overly long attribute values to avoid DOS-ing the browser
    // process.  However, send over a default string to indicate that the
    // attribute was present.
    return "x-max-data-length-exceeded";
  }
  return autocomplete_attribute;
}

WebFormElement GetClosestAncestorFormElement(WebNode n) {
  while (n) {
    if (HasTagName<kForm>(n)) {
      return n.To<WebFormElement>();
    }
    n = n.ParentNode();
  }
  return WebFormElement();
}

bool IsDOMPredecessor(const WebNode& x,
                      const WebNode& y,
                      const WebNode& ancestor_hint) {
  DCHECK(x.GetDocument() == y.GetDocument());
  DCHECK(!ancestor_hint || x.GetDocument() == ancestor_hint.GetDocument());
  // Extends the `path` up to `end` (exclusive) or the document root.
  // Paths are backwards: the last element is the top-most node.
  auto BuildPath = [](std::vector<WebNode> path, const WebNode& end) {
    DCHECK(!path.empty());
    path.reserve(path.size() + 16);
    WebNode parent;
    while ((parent = path.back().ParentNode()) && parent != end) {
      path.push_back(parent);
    }
    return path;
  };
  // Returns true iff `lhs` is strictly to the left of `rhs`, provided both
  // nodes are siblings.
  auto IsLeftSiblingOf = [](const WebNode& lhs, const WebNode& rhs) {
    DCHECK(lhs.ParentNode() == rhs.ParentNode());
    for (WebNode n = rhs; n; n = n.NextSibling()) {
      if (n == lhs)
        return false;
    }
    return true;
  };
  // Both paths are successors of either `ancestor_hint` or the document root.
  // If their parents aren't the same, we extend the paths to the document root.
  std::vector<WebNode> x_path = BuildPath({x}, ancestor_hint);
  std::vector<WebNode> y_path = BuildPath({y}, ancestor_hint);
  if (x_path.back().ParentNode() != y_path.back().ParentNode()) {
    x_path = BuildPath(std::move(x_path), WebNode());
    y_path = BuildPath(std::move(y_path), WebNode());
  }
  auto x_it = x_path.rbegin();
  auto y_it = y_path.rbegin();
  // Find the first different nodes in the paths. If such nodes exist, they are
  // siblings and their sibling order determines |x| and |y|'s relationship.
  while (x_it != x_path.rend() && y_it != y_path.rend()) {
    if (*x_it != *y_it)
      return IsLeftSiblingOf(*x_it, *y_it);
    ++x_it;
    ++y_it;
  }
  // If the paths don't differ in a node, the shorter path indicates a
  // predecessor since DOM traversal is in-order.
  return x_it == x_path.rend() && y_it != y_path.rend();
}

void GetDataListSuggestions(const WebInputElement& element,
                            std::vector<SelectOption>* options) {
  for (const auto& option_element : element.FilteredDataListOptions()) {
    if (options->size() > kMaxListSize) {
      break;
    }
    std::u16string value =
        option_element.Value().Utf16().substr(0, kMaxStringLength);
    std::u16string content =
        option_element.Value() != option_element.Label()
            ? option_element.Label().Utf16().substr(0, kMaxStringLength)
            : std::u16string();
    options->push_back(
        {.value = std::move(value), .content = std::move(content)});
  }
}

std::optional<FormData> ExtractFormData(
    const WebDocument& document,
    const WebFormElement& form_element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options) {
  return ExtractFormDataWithFieldsAndFrames(
      document, form_element, field_data_manager, extract_options);
}

GURL GetCanonicalActionForForm(const WebFormElement& form) {
  WebString action = form.Action();
  if (action.IsNull())
    action = WebString("");  // missing 'action' attribute implies current URL.
  GURL full_action(form.GetDocument().CompleteURL(action));
  return StripAuthAndParams(full_action);
}

GURL GetDocumentUrlWithoutAuth(const WebDocument& document) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  GURL full_origin(document.Url());
  return full_origin.ReplaceComponents(rep);
}

bool IsMonthInput(const WebInputElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kInputMonth;
}

bool IsMonthInput(const WebFormControlElement& element) {
  return IsMonthInput(element.DynamicTo<WebInputElement>());
}

// All text fields, including password fields, should be extracted.
bool IsTextInput(const WebInputElement& element) {
  return element && element.IsTextField();
}

bool IsTextInput(const WebFormControlElement& element) {
  return IsTextInput(element.DynamicTo<WebInputElement>());
}

bool IsSelectOrSelectListElement(const WebFormControlElement& element) {
  return IsSelectElement(element) || IsSelectListElement(element);
}

bool IsSelectElement(const WebFormControlElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kSelectOne;
}

bool IsSelectListElement(const WebFormControlElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kSelectList;
}

bool IsTextAreaElement(const WebFormControlElement& element) {
  return element && element.FormControlTypeForAutofill() ==
                        blink::mojom::FormControlType::kTextArea;
}

bool IsTextAreaElementOrTextInput(const WebFormControlElement& element) {
  return IsTextAreaElement(element) || IsTextInput(element);
}

bool IsCheckableElement(const WebFormControlElement& element) {
  WebInputElement input_element = element.DynamicTo<WebInputElement>();
  return input_element &&
         (input_element.IsCheckbox() || input_element.IsRadioButton());
}

bool IsCheckableElement(const WebElement& element) {
  return IsCheckableElement(element.DynamicTo<WebInputElement>());
}

bool IsAutofillableInputElement(const WebInputElement& element) {
  return IsTextInput(element) || IsMonthInput(element) ||
         IsCheckableElement(element);
}

bool IsAutofillableElement(const WebFormControlElement& element) {
  const WebInputElement input_element = element.DynamicTo<WebInputElement>();
  return IsAutofillableInputElement(input_element) ||
         IsSelectElement(element) || IsTextAreaElement(element) ||
         (IsSelectListElement(element) &&
          base::FeatureList::IsEnabled(features::kAutofillEnableSelectList));
}

FormControlType ToAutofillFormControlType(blink::mojom::FormControlType type) {
  switch (type) {
    case blink::mojom::FormControlType::kInputCheckbox:
      return FormControlType::kInputCheckbox;
    case blink::mojom::FormControlType::kInputEmail:
      return FormControlType::kInputEmail;
    case blink::mojom::FormControlType::kInputMonth:
      return FormControlType::kInputMonth;
    case blink::mojom::FormControlType::kInputNumber:
      return FormControlType::kInputNumber;
    case blink::mojom::FormControlType::kInputPassword:
      return FormControlType::kInputPassword;
    case blink::mojom::FormControlType::kInputRadio:
      return FormControlType::kInputRadio;
    case blink::mojom::FormControlType::kInputSearch:
      return FormControlType::kInputSearch;
    case blink::mojom::FormControlType::kInputTelephone:
      return FormControlType::kInputTelephone;
    case blink::mojom::FormControlType::kInputText:
      return FormControlType::kInputText;
    case blink::mojom::FormControlType::kInputUrl:
      return FormControlType::kInputUrl;
    case blink::mojom::FormControlType::kSelectOne:
      return FormControlType::kSelectOne;
    case blink::mojom::FormControlType::kSelectMultiple:
      return FormControlType::kSelectMultiple;
    case blink::mojom::FormControlType::kSelectList:
      return FormControlType::kSelectList;
    case blink::mojom::FormControlType::kTextArea:
      return FormControlType::kTextArea;
    default:
      NOTREACHED_NORETURN();
  }
}

bool IsCheckable(FormControlType form_control_type) {
  switch (form_control_type) {
    case FormControlType::kInputCheckbox:
    case FormControlType::kInputRadio:
      return true;
    case FormControlType::kContentEditable:
    case FormControlType::kInputEmail:
    case FormControlType::kInputMonth:
    case FormControlType::kInputNumber:
    case FormControlType::kInputPassword:
    case FormControlType::kInputSearch:
    case FormControlType::kInputTelephone:
    case FormControlType::kInputText:
    case FormControlType::kInputUrl:
    case FormControlType::kSelectOne:
    case FormControlType::kSelectMultiple:
    case FormControlType::kSelectList:
    case FormControlType::kTextArea:
      return false;
  }
  NOTREACHED_NORETURN();
}

bool IsWebauthnTaggedElement(const WebFormControlElement& element) {
  const std::optional<AutocompleteParsingResult> parsing_result =
      ParseAutocompleteAttribute(GetAutocompleteAttribute(element));
  return parsing_result.has_value() && parsing_result->webauthn;
}

bool IsElementEditable(const WebInputElement& element) {
  return element.IsEnabled() && !element.IsReadOnly();
}

bool IsWebElementFocusableForAutofill(const WebElement& element) {
  return element.IsFocusable() ||
         // The <selectlist> shadow root is not focusable.
         (IsSelectListElement(element.DynamicTo<WebFormControlElement>()) &&
          element.To<WebSelectListElement>().HasFocusableChild());
}

bool IsWebElementVisible(const WebElement& element) {
  auto HasMinSize = [](auto size) {
    constexpr int kMinPixelSize = 10;
    return size.width() >= kMinPixelSize && size.height() >= kMinPixelSize;
  };
  return element && IsWebElementFocusableForAutofill(element) &&
         (IsCheckableElement(element) || HasMinSize(element.GetClientSize()) ||
          HasMinSize(element.GetScrollSize()));
}

uint64_t GetMaxLength(const WebFormControlElement& element) {
  if (IsTextInput(element) || element.FormControlTypeForAutofill() ==
                                  blink::mojom::FormControlType::kTextArea) {
    auto max_length = element.MaxLength();
    static_assert(uint64_t{std::numeric_limits<decltype(max_length)>::max()} <=
                  FormFieldData::kDefaultMaxLength);
    return max_length < 0 ? FormFieldData::kDefaultMaxLength : max_length;
  }
  return 0;
}

std::u16string GetFormIdentifier(const WebFormElement& form) {
  std::u16string identifier = form.GetName().Utf16();
  if (identifier.empty())
    identifier = form.GetIdAttribute().Utf16();
  return identifier;
}

FormRendererId GetFormRendererId(const WebElement& e) {
  // This function is intended only for WebFormElements and for contenteditables
  // that aren't WebFormControlElement. However, an element that used to be
  // contenteditable may dynamically change to a non-contenteditable. Therefore,
  // instead of checking that `e` is a WebFormControlElement or contenteditable,
  // we just that `e` is not a WebFormControlElement to protect against
  // confusions between Get{Form,Field}RendererId().
  CHECK(!e.DynamicTo<WebFormControlElement>());

  if (!e) {
    return FormRendererId();
  }
  return FormRendererId(e.GetDomNodeId());
}

FieldRendererId GetFieldRendererId(const WebElement& e) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. However, an element that used
  // to be contenteditable may dynamically change to a non-contenteditable.
  // Therefore, instead of checking that `e` is a WebFormControlElement or
  // contenteditable, we just that `e` is not a WebFormElement to protect
  // against confusions between Get{Form,Field}RendererId().
  CHECK(!e.DynamicTo<WebFormElement>());
  return FieldRendererId(e.GetDomNodeId());
}

base::i18n::TextDirection GetTextDirectionForElement(
    const WebFormControlElement& element) {
  // Use 'text-align: left|right' if set or 'direction' otherwise.
  // See https://crbug.com/482339
  base::i18n::TextDirection direction = element.DirectionForFormData() == "rtl"
                                            ? base::i18n::RIGHT_TO_LEFT
                                            : base::i18n::LEFT_TO_RIGHT;
  if (element.AlignmentForFormData() == "left")
    direction = base::i18n::LEFT_TO_RIGHT;
  else if (element.AlignmentForFormData() == "right")
    direction = base::i18n::RIGHT_TO_LEFT;
  return direction;
}

std::vector<WebFormControlElement> GetFormControlElements(
    const WebDocument& document,
    const WebFormElement& form_element) {
  return form_element ? form_element.GetFormControlElements().ReleaseVector()
                      : GetUnownedFormControlElements(document);
}

std::vector<WebFormControlElement> GetAutofillableFormControlElements(
    const WebDocument& document,
    const WebFormElement& form_element) {
  std::vector<WebFormControlElement> elements =
      GetFormControlElements(document, form_element);
  std::erase_if(elements, std::not_fn(&IsAutofillableElement));
  return elements;
}

void WebFormControlElementToFormField(
    const WebFormElement& form_element,
    const WebFormControlElement& element,
    const FieldDataManager* field_data_manager,
    DenseSet<ExtractOption> extract_options,
    FormFieldData* field,
    ShadowFieldData* shadow_data) {
  DCHECK(field);
  DCHECK(element);
  DCHECK(element.GetDocument().GetFrame());
  DCHECK(IsAutofillableElement(element));

  const FieldRendererId renderer_id = GetFieldRendererId(element);
  // Save both id and name attributes, if present. If there is only one of them,
  // it will be saved to |name|. See HTMLFormControlElement::nameForAutofill.
  field->set_name(element.NameForAutofill().Utf16());
  field->set_id_attribute(element.GetIdAttribute().Utf16());
  field->set_name_attribute(GetAttribute<kName>(element).Utf16());
  field->set_renderer_id(renderer_id);
  field->set_host_form_id(GetFormRendererId(form_element));
  field->set_form_control_ax_id(element.GetAxId());
  field->set_form_control_type(
      ToAutofillFormControlType(element.FormControlTypeForAutofill()));
  field->set_max_length(GetMaxLength(element));
  field->set_autocomplete_attribute(GetAutocompleteAttribute(element));
  field->set_parsed_autocomplete(
      ParseAutocompleteAttribute(field->autocomplete_attribute()));

  if (base::EqualsCaseInsensitiveASCII(GetAttribute<kRole>(element).Utf16(),
                                       "presentation")) {
    field->set_role(FormFieldData::RoleAttribute::kPresentation);
  }

  field->set_placeholder(GetAttribute<kPlaceholder>(element).Utf16());
  if (HasAttribute<kClass>(element)) {
    field->set_css_classes(GetAttribute<kClass>(element).Utf16());
  }

  if (field_data_manager && field_data_manager->HasFieldData(renderer_id)) {
    field->set_properties_mask(
        field_data_manager->GetFieldPropertiesMask(renderer_id));
  }

  field->set_aria_label(GetAriaLabel(element.GetDocument(), element));
  field->set_aria_description(
      GetAriaDescription(element.GetDocument(), element));

  const bool kAutofillDetectFieldVisibilityEnabled =
      base::FeatureList::IsEnabled(features::kAutofillDetectFieldVisibility);

  // Traverse up through shadow hosts to see if we can gather missing
  // attributes.
  // TODO(crbug.com/40204601): Make sure this works for all shadow DOM cases,
  // including cases in which the owning form is multiple (shadow DOM) levels
  // apart from the form control element. Also check whether we cannot simplify
  // some of the shadow DOM traversals here.
  size_t levels_up = kMaxShadowLevelsUp;
  for (WebElement host = element.OwnerShadowHost();
       host && levels_up > 0 && form_element &&
       form_element.OwnerShadowHost() != host;
       host = host.OwnerShadowHost(), --levels_up) {
    std::u16string shadow_host_id = host.GetIdAttribute().Utf16();
    if (shadow_data && !shadow_host_id.empty())
      shadow_data->shadow_host_id_attributes.push_back(shadow_host_id);
    std::u16string shadow_host_name = GetAttribute<kName>(host).Utf16();
    if (shadow_data && !shadow_host_name.empty())
      shadow_data->shadow_host_name_attributes.push_back(shadow_host_name);

    if (field->id_attribute().empty()) {
      field->set_id_attribute(host.GetIdAttribute().Utf16());
    }
    if (field->name_attribute().empty()) {
      field->set_name_attribute(GetAttribute<kName>(host).Utf16());
    }
    if (field->name().empty()) {
      field->set_name(field->name_attribute().empty()
                          ? field->id_attribute()
                          : field->name_attribute());
    }
    if (field->autocomplete_attribute().empty()) {
      field->set_autocomplete_attribute(GetAutocompleteAttribute(host));
      field->set_parsed_autocomplete(
          ParseAutocompleteAttribute(field->autocomplete_attribute()));
    }
    if (field->css_classes().empty() && HasAttribute<kClass>(host)) {
      field->set_css_classes(GetAttribute<kClass>(host).Utf16());
    }
    if (field->aria_label().empty()) {
      field->set_aria_label(GetAriaLabel(host.GetDocument(), host));
    }
    if (field->aria_description().empty()) {
      field->set_aria_description(GetAriaDescription(host.GetDocument(), host));
    }
  }

  // The browser doesn't need to differentiate between preview and autofill.
  field->set_is_autofilled(element.IsAutofilled());
  field->set_is_user_edited(element.UserHasEditedTheField());
  field->set_is_focusable(IsWebElementFocusableForAutofill(element));
  field->set_is_visible(kAutofillDetectFieldVisibilityEnabled
                            ? IsWebElementVisible(element)
                            : field->is_focusable());
  field->set_should_autocomplete(
      element.AutoComplete() &&
      !(field->parsed_autocomplete().has_value() &&
        field->parsed_autocomplete().value().field_type ==
            HtmlFieldType::kOneTimeCode));
  field->set_text_direction(GetTextDirectionForElement(element));
  field->set_is_enabled(element.IsEnabled());
  field->set_is_readonly(element.IsReadOnly());

  if (auto input_element = element.DynamicTo<WebInputElement>();
      IsAutofillableInputElement(input_element)) {
    SetCheckStatus(field, IsCheckableElement(input_element),
                   input_element.IsChecked());
  } else if (IsTextAreaElement(element)) {
    // Nothing more to do in this case.
  } else if (extract_options.contains(ExtractOption::kOptions)) {
    // Set option strings on the field if available.
    DCHECK(IsSelectOrSelectListElement(element));
    WebVector<WebElement> element_list_items =
        GetListItemsForSelectOrSelectList(element);
    std::vector<SelectOption> options;
    FilterOptionElementsAndGetOptionStrings(element_list_items, &options);
    field->set_options(std::move(options));
  }
  if (extract_options.contains(ExtractOption::kBounds)) {
    if (auto* local_frame = element.GetDocument().GetFrame()) {
      if (auto* render_frame =
              content::RenderFrame::FromWebFrame(local_frame)) {
        field->set_bounds(gfx::RectF(
            render_frame->ConvertViewportToWindow(element.BoundsInWidget())));
      }
    }
  }
  if (extract_options.contains(ExtractOption::kDatalist)) {
    if (WebInputElement input = element.DynamicTo<WebInputElement>(); input) {
      std::vector<SelectOption> datalist_options;
      GetDataListSuggestions(input, &datalist_options);
      field->set_datalist_options(std::move(datalist_options));
    }
  }

  if (!extract_options.contains(ExtractOption::kValue)) {
    return;
  }

  std::u16string value = element.Value().Utf16();

  if (IsSelectOrSelectListElement(element) &&
      extract_options.contains(ExtractOption::kOptionText)) {
    // Convert the |element| value to text if requested.
    WebVector<WebElement> list_items =
        GetListItemsForSelectOrSelectList(element);
    for (const auto& list_item : list_items) {
      if (HasTagName<kOption>(list_item)) {
        const WebOptionElement option_element =
            list_item.To<WebOptionElement>();
        if (option_element.Value().Utf16() == value) {
          value = option_element.GetText().Utf16();
          break;
        }
      }
    }
  }

  field->set_value(std::move(value).substr(0, kMaxStringLength));
  field->set_selected_text(
      element.SelectedText().Utf16().substr(0, kMaxSelectedTextLength));
  field->set_allows_writing_suggestions(element.WritingSuggestions());

  // If the field was autofilled or the user typed into it, check the value
  // stored in |field_data_manager| against the value property of the DOM
  // element. If they differ, then the scripts on the website modified the
  // value afterwards. Store the original value as the |typed_value|, unless
  // this is one of recognised situations when the site-modified value is more
  // useful for filling.
  if (field_data_manager &&
      field->properties_mask() & (FieldPropertiesFlags::kUserTyped |
                                  FieldPropertiesFlags::kAutofilled)) {
    const std::u16string user_input =
        field_data_manager->GetUserInput(GetFieldRendererId(element));

    // The typed value is preserved for all passwords. It is also preserved for
    // potential usernames and credit cards, as long as the |value| is not
    // deemed acceptable.
    if (field->form_control_type() == FormControlType::kInputPassword ||
        !ScriptModifiedUsernameOrCreditCardNumberAcceptable(
            field->value(), user_input, *field_data_manager)) {
      field->set_user_input(user_input.substr(0, kMaxStringLength));
    }
  }
}

WebFormElement GetOwningForm(const WebFormControlElement& form_control) {
  // When `kAutofillIncludeFormElementsInShadowDom` is enabled, the owning form
  // is the furthest ancestor form element, if there is one.
  if (base::FeatureList::IsEnabled(
          blink::features::kAutofillIncludeFormElementsInShadowDom)) {
    WebFormElement parent;
    WebNode node = form_control;
    if (form_control.Form()) {
      node = form_control.Form();
    }
    for (; node; node = node.ParentOrShadowHostNode()) {
      if (auto form = node.DynamicTo<WebFormElement>()) {
        parent = form;
      }
    }
    return parent;
  }

  if (WebFormElement form = form_control.Form()) {
    return form;
  }
  // If we are in a shadow DOM, then look to see if the host(s) are inside a
  // form element we can use.
  size_t levels_up = kMaxShadowLevelsUp;
  for (WebElement host = form_control.OwnerShadowHost(); host && levels_up > 0;
       host = host.OwnerShadowHost(), --levels_up) {
    for (WebNode parent = host; parent; parent = parent.ParentNode()) {
      if (parent.IsElementNode()) {
        WebElement parentElement = parent.To<WebElement>();
        if (HasTagName<kForm>(parentElement)) {
          return parentElement.To<WebFormElement>();
        }
      }
    }
  }
  return WebFormElement();
}

std::optional<std::pair<FormData, FormFieldData>>
FindFormAndFieldForFormControlElement(
    const WebFormControlElement& element,
    const FieldDataManager& field_data_manager,
    DenseSet<ExtractOption> extract_options) {
  DCHECK(element);

  if (!IsAutofillableElement(element)) {
    return std::nullopt;
  }

  extract_options.insert_all({ExtractOption::kValue, ExtractOption::kOptions});
  WebDocument document = element.GetDocument();
  WebFormElement form_element = GetOwningForm(element);
  std::optional<FormData> form = ExtractFormData(
      document, form_element, field_data_manager, extract_options);
  if (form) {
    if (auto it = base::ranges::find(form->fields, GetFieldRendererId(element),
                                     &FormFieldData::renderer_id);
        it != form->fields.end()) {
      return std::pair<FormData, FormFieldData>(std::move(*form), *it);
    }
  }
  return std::nullopt;
}

std::optional<FormData> FindFormForContentEditable(
    const WebElement& content_editable) {
  if (content_editable.DynamicTo<WebFormElement>() ||
      content_editable.DynamicTo<WebFormControlElement>() ||
      !content_editable.IsContentEditable() ||
      content_editable != content_editable.RootEditableElement()) {
    return std::nullopt;
  }

  FormData form;
  form.renderer_id = GetFormRendererId(content_editable);
  form.id_attribute = content_editable.GetIdAttribute().Utf16();
  form.name_attribute = GetAttribute<kName>(content_editable).Utf16();
  form.name =
      !form.id_attribute.empty() ? form.id_attribute : form.name_attribute;
  form.is_action_empty = true;
  form.fields.emplace_back();

  FormFieldData& field = form.fields.back();
  WebDocument document = content_editable.GetDocument();
  field.set_id_attribute(content_editable.GetIdAttribute().Utf16());
  field.set_name_attribute(GetAttribute<kName>(content_editable).Utf16());
  field.set_name(!field.id_attribute().empty() ? field.id_attribute()
                                               : field.name_attribute());
  field.set_renderer_id(GetFieldRendererId(content_editable));
  field.set_host_form_id(GetFormRendererId(content_editable));
  field.set_form_control_type(FormControlType::kContentEditable);
  field.set_autocomplete_attribute(GetAutocompleteAttribute(content_editable));
  field.set_parsed_autocomplete(
      ParseAutocompleteAttribute(field.autocomplete_attribute()));
  if (auto* local_frame = document.GetFrame()) {
    if (auto* render_frame = content::RenderFrame::FromWebFrame(local_frame)) {
      field.set_bounds(gfx::RectF(render_frame->ConvertViewportToWindow(
          content_editable.BoundsInWidget())));
    }
  }
  if (base::EqualsCaseInsensitiveASCII(
          GetAttribute<kRole>(content_editable).Utf16(), "presentation")) {
    field.set_role(FormFieldData::RoleAttribute::kPresentation);
  }
  if (HasAttribute<kClass>(content_editable)) {
    field.set_css_classes(GetAttribute<kClass>(content_editable).Utf16());
  }
  field.set_aria_label(GetAriaLabel(document, content_editable));
  field.set_aria_description(GetAriaDescription(document, content_editable));
  // TextContentAbridged() includes hidden elements and does not add linebreaks.
  // If this is not sufficient in the future, consider calling
  // HTMLElement::innerText(), which returns the text "as rendered" (i.e., it
  // inserts whitespace at the right places and it ignores "display:none"
  // subtrees), but is significantly more expensive because it triggers a
  // layout.
  field.set_value(
      content_editable.TextContentAbridged(kMaxStringLength).Utf16());
  DCHECK_LE(field.value().length(), kMaxStringLength);
  field.set_selected_text(content_editable.SelectedText().Utf16().substr(
      0, kMaxSelectedTextLength));
  field.set_allows_writing_suggestions(content_editable.WritingSuggestions());
  return form;
}

std::vector<std::pair<FieldRef, WebAutofillState>> ApplyFieldsAction(
    const WebDocument& document,
    base::span<const FormFieldData::FillData> fields,
    mojom::FormActionType action_type,
    mojom::ActionPersistence action_persistence,
    FieldDataManager& field_data_manager) {
  // This container stores the FormFieldData::FillData* of `form.fields` that
  // will be filled into their corresponding blink elements.
  std::vector<std::pair<FieldRef, WebAutofillState>> filled_fields;
  filled_fields.reserve(fields.size());

  struct Field {
    explicit operator bool() const {
      DCHECK_EQ(!data, !element);
      return data;
    }

    raw_ptr<const FormFieldData::FillData> data = nullptr;
    WebFormControlElement element;
  };

  // We first collect the focused (if one exists) and the unfocused autofillable
  // fields, and the autofill them in the following order:
  //
  // 1. Autofill the focused field.
  // 2. Send a blur event for the initially focused field.
  // 3. For each unfocused field, focus -> autofill -> blur.
  // 4. Send a focus event for the initially focused field.
  //
  // We currently do not emit other events like keydown/keyup or paste and
  // beforeinput/textInput/input.
  Field focused_field;
  std::vector<Field> unfocused_fields;
  unfocused_fields.reserve(fields.size());

  // Step 0: Find the focused and the unfocused fields to fill.
  for (const FormFieldData::FillData& field : fields) {
    WebFormControlElement element =
        GetFormControlByRendererId(field.renderer_id);
    if (!element) {
      continue;
    }
    if ((action_type == mojom::FormActionType::kFill &&
         ShouldSkipFillField(field, element,
                             /*is_initiating_element=*/element.Focused())) ||
        (action_type == mojom::FormActionType::kUndo &&
         !element.IsAutofilled())) {
      continue;
    }
    if (element.Focused()) {
      focused_field = {&field, element};
    } else {
      unfocused_fields.emplace_back(&field, element);
    }
  }

  // Step 1: Autofill the initiating element.
  if (focused_field) {
    // In preview mode, only fill the field if it changes the fields value.
    // With this, the WebAutofillState is not changed from kAutofilled to
    // kPreviewed. This prevents the highlighting to change.
    filled_fields.emplace_back(focused_field.element,
                               focused_field.element.GetAutofillState());
    if (action_persistence == mojom::ActionPersistence::kFill) {
      FillFormField(*focused_field.data, /*is_initiating_node=*/true,
                    focused_field.element, field_data_manager);
    } else {
      PreviewFormField(*focused_field.data, focused_field.element,
                       field_data_manager);
    }
  }

  // If there is no other field to be autofilled, sending the blur event and
  // then the focus event for the initiating element does not make sense.
  if (unfocused_fields.empty()) {
    return filled_fields;
  }

  // Step 2: A blur event is emitted for the focused element if it is the
  // initiating element before all other elements are autofilled.
  if (action_persistence == mojom::ActionPersistence::kFill && focused_field) {
    focused_field.element.DispatchBlurEvent();
  }

  // Step 3: Autofill the non-initiating elements.
  // blink::WebFormControlElement::SetAutofillValue fires the focus and blur
  // events.
  for (Field& field : unfocused_fields) {
    filled_fields.emplace_back(field.element, field.element.GetAutofillState());
    if (action_persistence == mojom::ActionPersistence::kFill) {
      FillFormField(*field.data, /*is_initiating_node=*/false, field.element,
                    field_data_manager);
    } else {
      PreviewFormField(*field.data, field.element, field_data_manager);
    }
  }

  // Step 4: A focus event is emitted for the initiating element after
  // autofilling is completed. It is not intended to work for preview.
  if (action_persistence == mojom::ActionPersistence::kFill && focused_field) {
    focused_field.element.DispatchFocusEvent();
  }

  return filled_fields;
}

void ClearPreviewedElements(
    base::span<std::pair<WebFormControlElement, WebAutofillState>>
        previewed_elements) {
  for (auto& [control_element, prior_autofill_state] : previewed_elements) {
    // We do not add null elements to `previewed_elements_` in AutofillAgent.
    DCHECK(control_element);

    // Clear the suggested value.
    control_element.SetSuggestedValue(WebString());
    control_element.SetAutofillState(prior_autofill_state);

    // Clearing the suggested value in the focused node can cause the selection
    // to be lost. We force-set selection range in order to restore the text
    // cursor.
    if (control_element.Focused()) {
      auto length =
          base::checked_cast<unsigned>(control_element.Value().length());
      control_element.SetSelectionRange(length, length);
    }
  }
}

bool IsOwnedByFrame(const WebNode& node, content::RenderFrame* frame) {
  if (!node || !frame) {
    return false;
  }
  const WebDocument& doc = node.GetDocument();
  WebLocalFrame* node_frame = doc ? doc.GetFrame() : nullptr;
  WebLocalFrame* expected_frame = frame->GetWebFrame();
  return expected_frame && node_frame &&
         expected_frame->GetLocalFrameToken() ==
             node_frame->GetLocalFrameToken();
}

bool MaybeWasOwnedByFrame(const WebNode& node, content::RenderFrame* frame) {
  if (!node || !frame) {
    return true;
  }
  const WebDocument& doc = node.GetDocument();
  WebLocalFrame* node_frame = doc ? doc.GetFrame() : nullptr;
  WebLocalFrame* expected_frame = frame->GetWebFrame();
  return !expected_frame || !node_frame ||
         expected_frame->GetLocalFrameToken() ==
             node_frame->GetLocalFrameToken();
}

bool IsWebpageEmpty(const WebLocalFrame* frame) {
  WebDocument document = frame->GetDocument();

  return IsWebElementEmpty(document.Head()) &&
         IsWebElementEmpty(document.Body());
}

bool IsWebElementEmpty(const WebElement& root) {
  if (!root) {
    return true;
  }

  for (WebNode child = root.FirstChild(); child; child = child.NextSibling()) {
    if (child.IsTextNode() && !base::ContainsOnlyChars(child.NodeValue().Utf8(),
                                                       base::kWhitespaceASCII))
      return false;

    if (!child.IsElementNode())
      continue;

    WebElement element = child.To<WebElement>();
    if (!element.HasHTMLTagName(GetWebString<kScript>()) &&
        !element.HasHTMLTagName(GetWebString<kMeta>()) &&
        !element.HasHTMLTagName(GetWebString<kTitle>())) {
      return false;
    }
  }
  return true;
}

std::u16string FindChildText(const WebNode& node) {
  return FindChildTextWithIgnoreList(node, std::set<WebNode>());
}

ButtonTitleList GetButtonTitles(const WebFormElement& web_form,
                                ButtonTitlesCache* button_titles_cache) {
  if (!button_titles_cache) {
    // Button titles scraping is disabled for this form.
    return ButtonTitleList();
  }

  DCHECK(web_form);

  auto [form_position, cache_miss] = button_titles_cache->emplace(
      GetFormRendererId(web_form), ButtonTitleList());
  if (!cache_miss)
    return form_position->second;

  form_position->second = InferButtonTitlesForForm(web_form);
  return form_position->second;
}

std::u16string FindChildTextWithIgnoreList(
    const WebNode& node,
    const std::set<WebNode>& divs_to_skip) {
  if (node.IsTextNode()) {
    return node.NodeValue().Utf16();
  }

  WebNode child = node.FirstChild();

  const int kChildSearchDepth = 10;
  std::u16string node_text =
      FindChildTextInner(child, kChildSearchDepth, divs_to_skip);
  base::TrimWhitespace(node_text, base::TRIM_ALL, &node_text);
  return node_text;
}

std::optional<InferredLabel> InferLabelForElement(
    const WebFormControlElement& element) {
  if (IsCheckableElement(element)) {
    if (auto r = InferLabelFromNext(element)) {
      return r;
    }
  }
  if (auto r = InferLabelFromPrevious(element)) {
    return r;
  }
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAlwaysParsePlaceholders)) {
    if (auto r = InferLabelFromPlaceholder(element)) {
      return r;
    }
  }
  if (auto r = InferLabelFromOverlayingSuccessor(element)) {
    return r;
  }
  // If we didn't find a placeholder, check for aria-label text.
  if (auto r = InferLabelFromAriaLabel(element)) {
    return r;
  }
  // If we didn't find a label, check the `element`'s ancestors.
  if (auto r = InferLabelFromAncestors(element)) {
    return r;
  }
  // If we didn't find a label, check the value attr used as the placeholder.
  if (auto r = InferLabelFromValueAttribute(element)) {
    return r;
  }
  return std::nullopt;
}

WebFormElement GetFormByRendererId(FormRendererId form_renderer_id) {
  if (!form_renderer_id) {
    return WebFormElement();
  }
  WebNode node = WebNode::FromDomNodeId(form_renderer_id.value());
  WebFormElement form = node.DynamicTo<WebFormElement>();
  return form && form.IsConnected() && form.GetDocument().GetFrame()
             ? form
             : WebFormElement();
}

WebFormControlElement GetFormControlByRendererId(
    FieldRendererId queried_form_control) {
  if (!queried_form_control) {
    return WebFormControlElement();
  }
  WebNode node = WebNode::FromDomNodeId(queried_form_control.value());
  WebFormControlElement form_control = node.DynamicTo<WebFormControlElement>();
  return form_control && form_control.IsConnected() &&
                 form_control.GetDocument().GetFrame()
             ? form_control
             : WebFormControlElement();
}

WebElement GetContentEditableByRendererId(FieldRendererId field_renderer_id) {
  WebElement field =
      WebNode::FromDomNodeId(*field_renderer_id).DynamicTo<WebElement>();
  return field && field.IsContentEditable() ? field : WebElement();
}

namespace {

// Returns the coalesced child of the elements who's ids are found in
// |id_list|.
//
// For example, given this document...
//
//      <div id="billing">Billing</div>
//      <div>
//        <div id="name">Name</div>
//        <input id="field1" type="text" aria-labelledby="billing name"/>
//     </div>
//     <div>
//       <div id="address">Address</div>
//       <input id="field2" type="text" aria-labelledby="billing address"/>
//     </div>
//
// The coalesced text by the id_list found in the aria-labelledby attribute
// of the field1 input element would be "Billing Name" and for field2 it would
// be "Billing Address".
std::u16string CoalesceTextByIdList(const WebDocument& document,
                                    const WebString& id_list) {
  const std::u16string kSpace = u" ";

  std::u16string text;
  for (const auto& node : GetWebElementsFromIdList(document, id_list)) {
    if (node) {
      std::u16string child_text = FindChildText(node);
      if (!child_text.empty()) {
        if (!text.empty())
          text.append(kSpace);
        text.append(child_text);
      }
    }
  }
  base::TrimWhitespace(text, base::TRIM_ALL, &text);
  return text;
}

}  // namespace

std::u16string GetAriaLabel(const WebDocument& document,
                            const WebElement& element) {
  if (HasAttribute<kAriaLabelledBy>(element)) {
    WebString aria_label_attribute = GetAttribute<kAriaLabelledBy>(element);
    std::u16string text = CoalesceTextByIdList(document, aria_label_attribute);
    if (!text.empty())
      return text;
  }

  if (HasAttribute<kAriaLabel>(element)) {
    return GetAttribute<kAriaLabel>(element).Utf16();
  }

  return std::u16string();
}

std::u16string GetAriaDescription(const WebDocument& document,
                                  const WebElement& element) {
  return CoalesceTextByIdList(document,
                              GetAttribute<kAriaDescribedBy>(element));
}

WebNode NextWebNode(const WebNode& current_node, bool forward) {
  if (forward) {
    if (current_node.FirstChild()) {
      return current_node.FirstChild();
    }
    if (current_node.NextSibling()) {
      return current_node.NextSibling();
    }
    WebNode parent = current_node.ParentNode();
    while (parent) {
      if (parent.NextSibling()) {
        return parent.NextSibling();
      }
      parent = parent.ParentNode();
    }
    return parent;
  } else {
    if (current_node.PreviousSibling()) {
      WebNode previous = current_node.PreviousSibling();
      while (previous.LastChild()) {
        previous = previous.LastChild();
      }
      return previous;
    }
    return current_node.ParentNode();
  }
}

void TraverseDomForFourDigitCombinations(
    const WebDocument& document,
    base::OnceCallback<void(const std::vector<std::string>&)>
        potential_matches) {
  re2::RE2 kFourDigitRegex("(?:\\D|^)(\\d{4})(?:\\D|$)");
  base::flat_set<std::string> matches;
  // Iterate through each form control element in the DOM and extract the
  // elements nearby in search of four digit combinations.
  std::vector<WebFormControlElement> form_control_elements;

  for (const WebFormElement& form :
       base::FeatureList::IsEnabled(
           blink::features::kAutofillIncludeFormElementsInShadowDom)
           ? document.GetTopLevelForms()
           : document.Forms()) {
    base::ranges::move(form.GetFormControlElements().ReleaseVector(),
                       std::back_inserter(form_control_elements));
  }

  base::ranges::move(GetUnownedFormControlElements(document),
                     std::back_inserter(form_control_elements));

  auto extract_four_digit_combinations = [&](WebNode node) {
    if (!node.IsTextNode()) {
      return;
    }
    std::string node_text = node.NodeValue().Utf8();
    std::string_view input(node_text);
    std::string match;
    while (matches.size() < kMaxFourDigitCombinationMatches &&
           re2::RE2::FindAndConsume(&input, kFourDigitRegex, &match)) {
      matches.insert(match);
    }
  };

  // Returns whether the traversal reached a form control element.
  auto iterate_and_extract_four_digit_combinations = [&](WebNode node,
                                                         bool forward) {
    for (int i = 0; i < kFormNeighborNodesToTraverse; ++i) {
      if (!node) {
        break;
      }
      extract_four_digit_combinations(node);
      node = NextWebNode(node, forward);
      if (auto form_control_element = node.DynamicTo<WebFormControlElement>()) {
        // Reached next form control element.
        return true;
      }
    }
    return false;
  };

  bool reached_form_control_before = false;
  for (const WebFormControlElement& element : form_control_elements) {
    // If a forward search ended at a form control, we don't need a backward
    // search for that form control.
    if (!reached_form_control_before) {
      iterate_and_extract_four_digit_combinations(element,
                                                  /*forward=*/false);
    }
    reached_form_control_before =
        iterate_and_extract_four_digit_combinations(element, /*forward=*/true);

    if (matches.size() >= kMaxFourDigitCombinationMatches) {
      break;
    }
  }

  // Check for consecutive numbers as a potential indicator that we've parsed
  // a year <select> element of a credit card form. This indicates that a CVC
  // field is not a standalone CVC element.
  if (matches.size() > 2) {
    auto iter = matches.begin();
    int consecutive_numbers = 0;
    int previous_combination = 0;
    base::StringToInt(*iter, &previous_combination);
    iter++;
    for (; iter != matches.end(); ++iter) {
      int current_combination = 0;
      base::StringToInt(*iter, &current_combination);
      if (current_combination == previous_combination + 1) {
        consecutive_numbers++;
      } else {
        consecutive_numbers = 0;
      }
      if (consecutive_numbers > kMaxConsecutiveInFourDigitCombinationMatches) {
        // Clear all matches as we presume this is not standalone cvc if
        // there is a year input field.
        matches.clear();
        break;
      }
      previous_combination = current_combination;
    }
  }

  std::move(potential_matches)
      .Run(std::vector<std::string>(matches.begin(), matches.end()));
}

bool IsVisibleIframeForTesting(const WebElement& iframe_element) {
  return IsVisibleIframe(iframe_element);
}

WebFormElement GetFormElementForPasswordInput(const WebInputElement& element) {
  return base::FeatureList::IsEnabled(
             password_manager::features::kShadowDomSupport)
             ? form_util::GetOwningForm(element)
             : element.Form();
}

}  // namespace autofill::form_util