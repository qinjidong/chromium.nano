// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated by:
//   tools/json_schema_compiler/compiler.py.
// NOTE: The format of types has changed. 'FooType' is now
//   'chrome.accessibilityPrivate.FooType'.
// Please run the closure compiler before committing changes.
// See https://chromium.googlesource.com/chromium/src/+/main/docs/closure_compilation.md

/**
 * @fileoverview Externs generated from namespace: accessibilityPrivate
 * @externs
 */

/** @const */
chrome.accessibilityPrivate = {};

/**
 * Information about an alert
 * @typedef {{
 *   message: string
 * }}
 */
chrome.accessibilityPrivate.AlertInfo;

/**
 * Bounding rectangle in global screen coordinates.
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
chrome.accessibilityPrivate.ScreenRect;

/**
 * Point in global screen coordinates.
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
chrome.accessibilityPrivate.ScreenPoint;

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.Gesture = {
  CLICK: 'click',
  SWIPE_LEFT1: 'swipeLeft1',
  SWIPE_UP1: 'swipeUp1',
  SWIPE_RIGHT1: 'swipeRight1',
  SWIPE_DOWN1: 'swipeDown1',
  SWIPE_LEFT2: 'swipeLeft2',
  SWIPE_UP2: 'swipeUp2',
  SWIPE_RIGHT2: 'swipeRight2',
  SWIPE_DOWN2: 'swipeDown2',
  SWIPE_LEFT3: 'swipeLeft3',
  SWIPE_UP3: 'swipeUp3',
  SWIPE_RIGHT3: 'swipeRight3',
  SWIPE_DOWN3: 'swipeDown3',
  SWIPE_LEFT4: 'swipeLeft4',
  SWIPE_UP4: 'swipeUp4',
  SWIPE_RIGHT4: 'swipeRight4',
  SWIPE_DOWN4: 'swipeDown4',
  TAP2: 'tap2',
  TAP3: 'tap3',
  TAP4: 'tap4',
  TOUCH_EXPLORE: 'touchExplore',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.MagnifierCommand = {
  MOVE_STOP: 'moveStop',
  MOVE_UP: 'moveUp',
  MOVE_DOWN: 'moveDown',
  MOVE_LEFT: 'moveLeft',
  MOVE_RIGHT: 'moveRight',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SwitchAccessCommand = {
  SELECT: 'select',
  NEXT: 'next',
  PREVIOUS: 'previous',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.PointScanState = {
  START: 'start',
  STOP: 'stop',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SwitchAccessBubble = {
  BACK_BUTTON: 'backButton',
  MENU: 'menu',
};

/**
 * @typedef {{
 *   x: number,
 *   y: number
 * }}
 */
chrome.accessibilityPrivate.PointScanPoint;

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SwitchAccessMenuAction = {
  COPY: 'copy',
  CUT: 'cut',
  DECREMENT: 'decrement',
  DICTATION: 'dictation',
  END_TEXT_SELECTION: 'endTextSelection',
  INCREMENT: 'increment',
  ITEM_SCAN: 'itemScan',
  JUMP_TO_BEGINNING_OF_TEXT: 'jumpToBeginningOfText',
  JUMP_TO_END_OF_TEXT: 'jumpToEndOfText',
  KEYBOARD: 'keyboard',
  LEFT_CLICK: 'leftClick',
  MOVE_BACKWARD_ONE_CHAR_OF_TEXT: 'moveBackwardOneCharOfText',
  MOVE_BACKWARD_ONE_WORD_OF_TEXT: 'moveBackwardOneWordOfText',
  MOVE_CURSOR: 'moveCursor',
  MOVE_DOWN_ONE_LINE_OF_TEXT: 'moveDownOneLineOfText',
  MOVE_FORWARD_ONE_CHAR_OF_TEXT: 'moveForwardOneCharOfText',
  MOVE_FORWARD_ONE_WORD_OF_TEXT: 'moveForwardOneWordOfText',
  MOVE_UP_ONE_LINE_OF_TEXT: 'moveUpOneLineOfText',
  PASTE: 'paste',
  POINT_SCAN: 'pointScan',
  RIGHT_CLICK: 'rightClick',
  SCROLL_DOWN: 'scrollDown',
  SCROLL_LEFT: 'scrollLeft',
  SCROLL_RIGHT: 'scrollRight',
  SCROLL_UP: 'scrollUp',
  SELECT: 'select',
  SETTINGS: 'settings',
  START_TEXT_SELECTION: 'startTextSelection',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SyntheticKeyboardEventType = {
  KEYUP: 'keyup',
  KEYDOWN: 'keydown',
};

/**
 * @typedef {{
 *   ctrl: (boolean|undefined),
 *   alt: (boolean|undefined),
 *   search: (boolean|undefined),
 *   shift: (boolean|undefined)
 * }}
 */
chrome.accessibilityPrivate.SyntheticKeyboardModifiers;

/**
 * @typedef {{
 *   type: !chrome.accessibilityPrivate.SyntheticKeyboardEventType,
 *   keyCode: number,
 *   modifiers: (!chrome.accessibilityPrivate.SyntheticKeyboardModifiers|undefined)
 * }}
 */
chrome.accessibilityPrivate.SyntheticKeyboardEvent;

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SyntheticMouseEventType = {
  PRESS: 'press',
  RELEASE: 'release',
  DRAG: 'drag',
  MOVE: 'move',
  ENTER: 'enter',
  EXIT: 'exit',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SyntheticMouseEventButton = {
  LEFT: 'left',
  MIDDLE: 'middle',
  RIGHT: 'right',
  BACK: 'back',
  FOWARD: 'foward',
};

/**
 * @typedef {{
 *   type: !chrome.accessibilityPrivate.SyntheticMouseEventType,
 *   x: number,
 *   y: number,
 *   touchAccessibility: (boolean|undefined),
 *   mouseButton: (!chrome.accessibilityPrivate.SyntheticMouseEventButton|undefined)
 * }}
 */
chrome.accessibilityPrivate.SyntheticMouseEvent;

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SelectToSpeakState = {
  SELECTING: 'selecting',
  SPEAKING: 'speaking',
  INACTIVE: 'inactive',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.FocusType = {
  GLOW: 'glow',
  SOLID: 'solid',
  DASHED: 'dashed',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.FocusRingStackingOrder = {
  ABOVE_ACCESSIBILITY_BUBBLES: 'aboveAccessibilityBubbles',
  BELOW_ACCESSIBILITY_BUBBLES: 'belowAccessibilityBubbles',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.AssistiveTechnologyType = {
  CHROME_VOX: 'chromeVox',
  SELECT_TO_SPEAK: 'selectToSpeak',
  SWITCH_ACCESS: 'switchAccess',
  AUTO_CLICK: 'autoClick',
  MAGNIFIER: 'magnifier',
  DICTATION: 'dictation',
};

/**
 * @typedef {{
 *   rects: !Array<!chrome.accessibilityPrivate.ScreenRect>,
 *   type: !chrome.accessibilityPrivate.FocusType,
 *   color: string,
 *   secondaryColor: (string|undefined),
 *   backgroundColor: (string|undefined),
 *   stackingOrder: (!chrome.accessibilityPrivate.FocusRingStackingOrder|undefined),
 *   id: (string|undefined)
 * }}
 */
chrome.accessibilityPrivate.FocusRingInfo;

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.AcceleratorAction = {
  FOCUS_PREVIOUS_PANE: 'focusPreviousPane',
  FOCUS_NEXT_PANE: 'focusNextPane',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.AccessibilityFeature = {
  DICTATION_CONTEXT_CHECKING: 'dictationContextChecking',
  FACE_GAZE: 'faceGaze',
  GOOGLE_TTS_HIGH_QUALITY_VOICES: 'googleTtsHighQualityVoices',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SelectToSpeakPanelAction = {
  PREVIOUS_PARAGRAPH: 'previousParagraph',
  PREVIOUS_SENTENCE: 'previousSentence',
  PAUSE: 'pause',
  RESUME: 'resume',
  NEXT_SENTENCE: 'nextSentence',
  NEXT_PARAGRAPH: 'nextParagraph',
  EXIT: 'exit',
  CHANGE_SPEED: 'changeSpeed',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.SetNativeChromeVoxResponse = {
  SUCCESS: 'success',
  TALKBACK_NOT_INSTALLED: 'talkbackNotInstalled',
  WINDOW_NOT_FOUND: 'windowNotFound',
  FAILURE: 'failure',
  NEED_DEPRECATION_CONFIRMATION: 'needDeprecationConfirmation',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.DictationBubbleIconType = {
  HIDDEN: 'hidden',
  STANDBY: 'standby',
  MACRO_SUCCESS: 'macroSuccess',
  MACRO_FAIL: 'macroFail',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.DictationBubbleHintType = {
  TRY_SAYING: 'trySaying',
  TYPE: 'type',
  DELETE: 'delete',
  SELECT_ALL: 'selectAll',
  UNDO: 'undo',
  HELP: 'help',
  UNSELECT: 'unselect',
  COPY: 'copy',
};

/**
 * @typedef {{
 *   visible: boolean,
 *   icon: !chrome.accessibilityPrivate.DictationBubbleIconType,
 *   text: (string|undefined),
 *   hints: (!Array<!chrome.accessibilityPrivate.DictationBubbleHintType>|undefined)
 * }}
 */
chrome.accessibilityPrivate.DictationBubbleProperties;

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.ToastType = {
  DICTATION_NO_FOCUSED_TEXT_FIELD: 'dictationNoFocusedTextField',
  DICTATION_MIC_MUTED: 'dictationMicMuted',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.DlcType = {
  TTS_BN_BD: 'ttsBnBd',
  TTS_CS_CZ: 'ttsCsCz',
  TTS_DA_DK: 'ttsDaDk',
  TTS_DE_DE: 'ttsDeDe',
  TTS_EL_GR: 'ttsElGr',
  TTS_EN_AU: 'ttsEnAu',
  TTS_EN_GB: 'ttsEnGb',
  TTS_EN_US: 'ttsEnUs',
  TTS_ES_ES: 'ttsEsEs',
  TTS_ES_US: 'ttsEsUs',
  TTS_FI_FI: 'ttsFiFi',
  TTS_FIL_PH: 'ttsFilPh',
  TTS_FR_FR: 'ttsFrFr',
  TTS_HI_IN: 'ttsHiIn',
  TTS_HU_HU: 'ttsHuHu',
  TTS_ID_ID: 'ttsIdId',
  TTS_IT_IT: 'ttsItIt',
  TTS_JA_JP: 'ttsJaJp',
  TTS_KM_KH: 'ttsKmKh',
  TTS_KO_KR: 'ttsKoKr',
  TTS_NB_NO: 'ttsNbNo',
  TTS_NE_NP: 'ttsNeNp',
  TTS_NL_NL: 'ttsNlNl',
  TTS_PL_PL: 'ttsPlPl',
  TTS_PT_BR: 'ttsPtBr',
  TTS_PT_PT: 'ttsPtPt',
  TTS_SI_LK: 'ttsSiLk',
  TTS_SK_SK: 'ttsSkSk',
  TTS_SV_SE: 'ttsSvSe',
  TTS_TH_TH: 'ttsThTh',
  TTS_TR_TR: 'ttsTrTr',
  TTS_UK_UA: 'ttsUkUa',
  TTS_VI_VN: 'ttsViVn',
  TTS_YUE_HK: 'ttsYueHk',
};

/**
 * @enum {string}
 */
chrome.accessibilityPrivate.TtsVariant = {
  LITE: 'lite',
  STANDARD: 'standard',
};

/**
 * @typedef {{
 *   js_pumpkin_tagger_bin_js: ArrayBuffer,
 *   tagger_wasm_main_js: ArrayBuffer,
 *   tagger_wasm_main_wasm: ArrayBuffer,
 *   en_us_action_config_binarypb: ArrayBuffer,
 *   en_us_pumpkin_config_binarypb: ArrayBuffer,
 *   fr_fr_action_config_binarypb: ArrayBuffer,
 *   fr_fr_pumpkin_config_binarypb: ArrayBuffer,
 *   it_it_action_config_binarypb: ArrayBuffer,
 *   it_it_pumpkin_config_binarypb: ArrayBuffer,
 *   de_de_action_config_binarypb: ArrayBuffer,
 *   de_de_pumpkin_config_binarypb: ArrayBuffer,
 *   es_es_action_config_binarypb: ArrayBuffer,
 *   es_es_pumpkin_config_binarypb: ArrayBuffer
 * }}
 */
chrome.accessibilityPrivate.PumpkinData;

/**
 * @typedef {{
 *   model: ArrayBuffer,
 *   wasm: ArrayBuffer
 * }}
 */
chrome.accessibilityPrivate.FaceGazeAssets;

/**
 * Property to indicate whether event source should default to touch.
 * @type {number}
 */
chrome.accessibilityPrivate.IS_DEFAULT_EVENT_SOURCE_TOUCH;

/**
 * Called to translate localeCodeToTranslate into human-readable string in the
 * locale specified by displayLocaleCode
 * @param {string} localeCodeToTranslate
 * @param {string} displayLocaleCode
 * @return {string} The human-readable locale string in the provided locale.
 */
chrome.accessibilityPrivate.getDisplayNameForLocale = function(localeCodeToTranslate, displayLocaleCode) {};

/**
 * Called to request battery status from Chrome OS system.
 * @param {function(string): void} callback Returns battery description as a
 *     string.
 */
chrome.accessibilityPrivate.getBatteryDescription = function(callback) {};

/**
 * Called to request an install of the Pumpkin semantic parser for Dictation.
 * @param {function(!chrome.accessibilityPrivate.PumpkinData): void} callback
 *     Runs when Pumpkin download finishes.
 */
chrome.accessibilityPrivate.installPumpkinForDictation = function(callback) {};

/**
 * Called to request an install of the FaceGaze assets DLC, which contains files
 * (e.g. the FaceLandmarker model) required for FaceGaze to work.
 * @param {function(!chrome.accessibilityPrivate.FaceGazeAssets): void} callback
 *     Runs when the DLC download finishes.
 */
chrome.accessibilityPrivate.installFaceGazeAssets = function(callback) {};

/**
 * Enables or disables native accessibility support. Once disabled, it is up to
 * the calling extension to provide accessibility for web contents.
 * @param {boolean} enabled True if native accessibility support should be
 *     enabled.
 */
chrome.accessibilityPrivate.setNativeAccessibilityEnabled = function(enabled) {};

/**
 * Sets the given accessibility focus rings for this extension.
 * @param {!Array<!chrome.accessibilityPrivate.FocusRingInfo>} focusRings Array
 *     of focus rings to draw.
 * @param {!chrome.accessibilityPrivate.AssistiveTechnologyType} atType
 *     Associates these focus rings with this feature type.
 */
chrome.accessibilityPrivate.setFocusRings = function(focusRings, atType) {};

/**
 * Sets the bounds of the accessibility highlight.
 * @param {!Array<!chrome.accessibilityPrivate.ScreenRect>} rects Array of
 *     rectangles to draw the highlight around.
 * @param {string} color CSS-style hex color string beginning with # like
 *     #FF9982 or #EEE.
 */
chrome.accessibilityPrivate.setHighlights = function(rects, color) {};

/**
 * Informs the system where Select to Speak's reading focus is in screen
 * coordinates. Causes chrome.accessibilityPrivate.onSelectToSpeakFocusChanged
 * to be fired within the AccessibilityCommon component extension.
 * @param {!chrome.accessibilityPrivate.ScreenRect} bounds Bounds of currently
 *     spoken word (if available) or node (if the spoken node is not a text
 *     node).
 */
chrome.accessibilityPrivate.setSelectToSpeakFocus = function(bounds) {};

/**
 * Sets the calling extension as a listener of all keyboard events optionally
 * allowing the calling extension to capture/swallow the key event via DOM apis.
 * Returns false via callback when unable to set the listener.
 * @param {boolean} enabled True if the caller wants to listen to key events;
 *     false to stop listening to events. Note that there is only ever one
 *     extension listening to key events.
 * @param {boolean} capture True if key events should be swallowed natively and
 *     not propagated if preventDefault() gets called by the extension's
 *     background page.
 */
chrome.accessibilityPrivate.setKeyboardListener = function(enabled, capture) {};

/**
 * Darkens or undarkens the screen.
 * @param {boolean} darken True to darken screen; false to undarken screen.
 */
chrome.accessibilityPrivate.darkenScreen = function(darken) {};

/**
 * When enabled, forwards key events to the Switch Access extension
 * @param {boolean} shouldForward
 */
chrome.accessibilityPrivate.forwardKeyEventsToSwitchAccess = function(shouldForward) {};

/**
 * Shows the Switch Access menu next to the specified rectangle and with the
 * given actions
 * @param {!chrome.accessibilityPrivate.SwitchAccessBubble} bubble Which bubble
 *     to show/hide
 * @param {boolean} show True if the bubble should be shown, false otherwise
 * @param {!chrome.accessibilityPrivate.ScreenRect=} anchor A rectangle
 *     indicating the bounds of the object the menu should be displayed next to.
 * @param {!Array<!chrome.accessibilityPrivate.SwitchAccessMenuAction>=} actions
 *     The actions to be shown in the menu.
 */
chrome.accessibilityPrivate.updateSwitchAccessBubble = function(bubble, show, anchor, actions) {};

/**
 * Sets point scanning state Switch Access.
 * @param {!chrome.accessibilityPrivate.PointScanState} state The point scanning
 *     state to set.
 */
chrome.accessibilityPrivate.setPointScanState = function(state) {};

/**
 * Sets current ARC app to use native ARC support.
 * @param {boolean} enabled True for ChromeVox (native), false for TalkBack.
 * @param {function(!chrome.accessibilityPrivate.SetNativeChromeVoxResponse): void}
 *     callback
 */
chrome.accessibilityPrivate.setNativeChromeVoxArcSupportForCurrentApp = function(enabled, callback) {};

/**
 * Sends a fabricated key event.
 * @param {!chrome.accessibilityPrivate.SyntheticKeyboardEvent} keyEvent The
 *     event to send.
 * @param {boolean=} useRewriters If true, uses rewriters for the key event;
 *     only allowed if used from Dictation. Otherwise indicates that rewriters
 *     should be skipped.
 */
chrome.accessibilityPrivate.sendSyntheticKeyEvent = function(keyEvent, useRewriters) {};

/**
 * Enables or disables mouse events in accessibility extensions
 * @param {boolean} enabled True if accessibility component extensions should
 *     receive mouse events.
 */
chrome.accessibilityPrivate.enableMouseEvents = function(enabled) {};

/**
 * Sets the cursor position on the screen in absolute screen coordinates.
 * @param {!chrome.accessibilityPrivate.ScreenPoint} point The screen point at
 *     which to put the cursor.
 */
chrome.accessibilityPrivate.setCursorPosition = function(point) {};

/**
 * Sends a fabricated mouse event.
 * @param {!chrome.accessibilityPrivate.SyntheticMouseEvent} mouseEvent The
 *     event to send.
 */
chrome.accessibilityPrivate.sendSyntheticMouseEvent = function(mouseEvent) {};

/**
 * Called by the Select-to-Speak extension when Select-to-Speak has changed
 * states, between selecting with the mouse, speaking, and inactive.
 * @param {!chrome.accessibilityPrivate.SelectToSpeakState} state
 */
chrome.accessibilityPrivate.setSelectToSpeakState = function(state) {};

/**
 * Called by the Select-to-Speak extension to request a clipboard copy in the
 * active Lacros Google Docs tab for the copy-paste fallback.
 * @param {string} url URL of the Google Docs tab.
 */
chrome.accessibilityPrivate.clipboardCopyInActiveLacrosGoogleDoc = function(url) {};

/**
 * Called by the Accessibility Common extension when
 * onScrollableBoundsForPointRequested has found a scrolling container. |rect|
 * will be the bounds of the nearest scrollable ancestor of the node at the
 * point requested using onScrollableBoundsForPointRequested.
 * @param {!chrome.accessibilityPrivate.ScreenRect} rect
 */
chrome.accessibilityPrivate.handleScrollableBoundsForPointFound = function(rect) {};

/**
 * Called by the Accessibility Common extension to move |rect| within the
 * magnifier viewport (e.g. when focus has changed). If |rect| is already
 * completely within the viewport, magnifier doesn't move. If any edge of |rect|
 * is outside the viewport (e.g. if rect is larger than or extends partially
 * beyond the viewport), magnifier will center the overflowing dimensions of the
 * viewport on center of |rect| (e.g. center viewport vertically if |rect|
 * extends beyond bottom of screen).
 * @param {!chrome.accessibilityPrivate.ScreenRect} rect Rect to ensure visible
 *     in the magnified viewport.
 */
chrome.accessibilityPrivate.moveMagnifierToRect = function(rect) {};

/**
 * Called by the Accessibility Common extension to center magnifier at |point|.
 * @param {!chrome.accessibilityPrivate.ScreenPoint} point
 */
chrome.accessibilityPrivate.magnifierCenterOnPoint = function(point) {};

/**
 * Toggles dictation between active and inactive states.
 */
chrome.accessibilityPrivate.toggleDictation = function() {};

/**
 * Shows or hides the virtual keyboard.
 * @param {boolean} isVisible
 */
chrome.accessibilityPrivate.setVirtualKeyboardVisible = function(isVisible) {};

/**
 * Opens a specified settings subpage. To open a page with url
 * chrome://settings/manageAccessibility/tts, pass in the substring
 * 'manageAccessibility/tts'.
 * @param {string} subpage
 */
chrome.accessibilityPrivate.openSettingsSubpage = function(subpage) {};

/**
 * Performs an accelerator action.
 * @param {!chrome.accessibilityPrivate.AcceleratorAction} acceleratorAction
 */
chrome.accessibilityPrivate.performAcceleratorAction = function(acceleratorAction) {};

/**
 * Checks to see if an accessibility feature is enabled.
 * @param {!chrome.accessibilityPrivate.AccessibilityFeature} feature
 * @param {function(boolean): void} callback Returns whether feature is enabled.
 */
chrome.accessibilityPrivate.isFeatureEnabled = function(feature, callback) {};

/**
 * Updates properties of the Select-to-speak panel.
 * @param {boolean} show True to show panel, false to hide it
 * @param {!chrome.accessibilityPrivate.ScreenRect=} anchor A rectangle
 *     indicating the bounds of the object the panel should be displayed next
 *     to.
 * @param {boolean=} isPaused True if Select-to-speak playback is paused.
 * @param {number=} speed Current reading speed (TTS speech rate).
 */
chrome.accessibilityPrivate.updateSelectToSpeakPanel = function(show, anchor, isPaused, speed) {};

/**
 * Shows a confirmation dialog.
 * @param {string} title The title of the confirmation dialog.
 * @param {string} description The description to show within the confirmation
 *     dialog.
 * @param {?string|undefined} cancelName The human-readable name of the cancel
 *     button.
 * @param {function(boolean): void} callback Called when the dialog is confirmed
 *     or cancelled.
 */
chrome.accessibilityPrivate.showConfirmationDialog = function(title, description, cancelName, callback) {};

/**
 * Gets the DOM key string for the given key code, taking into account the
 * current input method locale, and assuming the key code is for U.S. input. For
 * example, the key code for '/' would return the string '!' if the current
 * input method is French.
 * @param {number} keyCode
 * @param {function(string): void} callback Called with the resulting Dom key
 *     string.
 */
chrome.accessibilityPrivate.getLocalizedDomKeyStringForKeyCode = function(keyCode, callback) {};

/**
 * Updates Dictation's bubble UI.
 * @param {!chrome.accessibilityPrivate.DictationBubbleProperties} properties
 *     Properties for the updated Dictation bubble UI.
 */
chrome.accessibilityPrivate.updateDictationBubble = function(properties) {};

/**
 * Cancels the current and queued speech from ChromeVox.
 */
chrome.accessibilityPrivate.silenceSpokenFeedback = function() {};

/**
 * Returns the contents of a DLC.
 * @param {!chrome.accessibilityPrivate.DlcType} dlc The DLC of interest.
 * @param {function(ArrayBuffer): void} callback A callback that is run when the
 *     contents are returned.
 */
chrome.accessibilityPrivate.getDlcContents = function(dlc, callback) {};

/**
 * Returns the contents of a TTS DLC.
 * @param {!chrome.accessibilityPrivate.DlcType} dlc The DLC of interest.
 * @param {!chrome.accessibilityPrivate.TtsVariant} variant The TTS voice
 *     variant.
 * @param {function(ArrayBuffer): void} callback A callback that is run when the
 *     contents are returned.
 */
chrome.accessibilityPrivate.getTtsDlcContents = function(dlc, variant, callback) {};

/**
 * Returns the bounds of the displays in density-independent pixels in screen
 * coordinates.
 * @param {function(!Array<!chrome.accessibilityPrivate.ScreenRect>): void}
 *     callback A callback that is run when the result is returned.
 */
chrome.accessibilityPrivate.getDisplayBounds = function(callback) {};

/**
 * Gets whether new browser windows and tabs should be in Lacros browser.
 * @param {function(boolean): void} callback A callback that is run when the
 *     result is returned.
 */
chrome.accessibilityPrivate.isLacrosPrimary = function(callback) {};

/**
 * Displays an accessibility-related toast.
 * @param {!chrome.accessibilityPrivate.ToastType} type The type of toast to
 *     show.
 */
chrome.accessibilityPrivate.showToast = function(type) {};

/**
 * Fired whenever ChromeVox should output introduction.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onIntroduceChromeVox;

/**
 * Fired when an accessibility gesture is detected by the touch exploration
 * controller.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onAccessibilityGesture;

/**
 * Fired when the Select to Speak context menu is clicked from outside the
 * context of the Select to Speak extension.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSelectToSpeakContextMenuClicked;

/**
 * Fired when the Select to Speak reading focus changes.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSelectToSpeakFocusChanged;

/**
 * Fired when Chrome OS wants to change the Select-to-Speak state, between
 * selecting with the mouse, speaking, and inactive.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSelectToSpeakStateChangeRequested;

/**
 * Fired when Chrome OS wants to send an updated list of keys currently pressed
 * to Select to Speak.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSelectToSpeakKeysPressedChanged;

/**
 * Fired when Chrome OS wants to send a mouse event Select to Speak.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSelectToSpeakMouseChanged;

/**
 * Fired when an action is performed in the Select-to-speak panel.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSelectToSpeakPanelAction;

/**
 * Fired when Chrome OS has received a key event corresponding to a Switch
 * Access command.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onSwitchAccessCommand;

/**
 * Fired when Chrome OS has received the final point of point scanning.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onPointScanSet;

/**
 * Fired when Chrome OS has received a key event corresponding to a Magnifier
 * command.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onMagnifierCommand;

/**
 * Fired when an internal component within accessibility wants to force speech
 * output for an accessibility extension. Do not use without approval from
 * accessibility owners.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onAnnounceForAccessibility;

/**
 * Fired when an internal component within accessibility wants to find the
 * nearest scrolling container at a given screen coordinate. Used in Automatic
 * Clicks.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onScrollableBoundsForPointRequested;

/**
 * Fired when Chrome OS magnifier bounds are updated.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onMagnifierBoundsChanged;

/**
 * Fired when a custom spoken feedback on the active window gets enabled or
 * disabled. Called from ARC++ accessibility.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onCustomSpokenFeedbackToggled;

/**
 * Fired when ChromeVox should show its tutorial.
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onShowChromeVoxTutorial;

/**
 * Fired when Dictation is activated or deactivated using a keyboard shortcut,
 * the button in the tray, or after a call from
 * accessibilityPrivate.toggleDictation
 * @type {!ChromeEvent}
 */
chrome.accessibilityPrivate.onToggleDictation;