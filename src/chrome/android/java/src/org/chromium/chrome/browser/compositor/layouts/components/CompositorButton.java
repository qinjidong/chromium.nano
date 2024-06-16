// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;
import android.util.FloatProperty;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

/**
 * {@link CompositorButton} keeps track of state for buttons that are rendered in the compositor.
 */
public class CompositorButton extends StripLayoutView {
    /**
     * A property that can be used with a {@link
     * org.chromium.chrome.browser.layouts.animation.CompositorAnimator}.
     */
    public static final FloatProperty<CompositorButton> OPACITY =
            new FloatProperty<CompositorButton>("opacity") {
                @Override
                public void setValue(CompositorButton object, float value) {
                    object.setOpacity(value);
                }

                @Override
                public Float get(CompositorButton object) {
                    return object.getOpacity();
                }
            };

    /** Handler for click actions on VirtualViews. */
    public interface CompositorOnClickHandler {
        /**
         * Handles the click action.
         * @param time The time of the click action.
         */
        void onClick(long time);
    }

    // Pre-allocated to avoid in-frame allocations.
    private final RectF mBounds = new RectF();
    private final RectF mCacheBounds = new RectF();

    private final CompositorOnClickHandler mClickHandler;

    protected int mResource;
    protected int mBackgroundResource;

    private int mPressedResource;
    private int mIncognitoResource;
    private int mIncognitoPressedResource;

    private float mOpacity;
    private float mClickSlop;
    private boolean mIsPressed;
    private boolean mIsPressedFromMouse;
    private boolean mIsHovered;
    private boolean mIsIncognito;
    private boolean mIsEnabled;
    private String mAccessibilityDescription = "";
    private String mAccessibilityDescriptionIncognito = "";

    /**
     * Default constructor for {@link CompositorButton}
     * @param context      An Android context for fetching dimens.
     * @param width        The button width.
     * @param height       The button height.
     * @param clickHandler The action to be performed on click.
     */
    public CompositorButton(
            Context context, float width, float height, CompositorOnClickHandler clickHandler) {
        mBounds.set(0, 0, width, height);

        mOpacity = 1.f;
        mIsPressed = false;
        mIsIncognito = false;
        mIsEnabled = true;
        setVisible(true);

        Resources res = context.getResources();
        float sPxToDp = 1.0f / res.getDisplayMetrics().density;
        mClickSlop = res.getDimension(R.dimen.compositor_button_slop) * sPxToDp;

        mClickHandler = clickHandler;
    }

    /**
     * A set of Android resources to supply to the compositor.
     * @param resource                  The default Android resource.
     * @param pressedResource           The pressed Android resource.
     * @param incognitoResource         The incognito Android resource.
     * @param incognitoPressedResource  The incognito pressed resource.
     */
    public void setResources(
            int resource,
            int pressedResource,
            int incognitoResource,
            int incognitoPressedResource) {
        mResource = resource;
        mPressedResource = pressedResource;
        mIncognitoResource = incognitoResource;
        mIncognitoPressedResource = incognitoPressedResource;
    }

    /**
     * @param description A string describing the resource.
     */
    public void setAccessibilityDescription(String description, String incognitoDescription) {
        mAccessibilityDescription = description;
        mAccessibilityDescriptionIncognito = incognitoDescription;
    }

    /** {@link org.chromium.chrome.browser.layouts.components.VirtualView} Implementation */
    @Override
    public String getAccessibilityDescription() {
        return mIsIncognito ? mAccessibilityDescriptionIncognito : mAccessibilityDescription;
    }

    @Override
    public void getTouchTarget(RectF outTarget) {
        outTarget.set(mBounds);
        // Get the whole touchable region.
        outTarget.inset((int) -mClickSlop, (int) -mClickSlop);
    }

    /**
     * @param x The x offset of the click.
     * @param y The y offset of the click.
     * @return Whether or not that click occurred inside of the button + slop area.
     */
    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        if (mOpacity < 1.f || !isVisible() || !mIsEnabled) return false;

        mCacheBounds.set(mBounds);
        mCacheBounds.inset(-mClickSlop, -mClickSlop);
        return mCacheBounds.contains(x, y);
    }

    @Override
    public void handleClick(long time) {
        mClickHandler.onClick(time);
    }

    /** {@link StripLayoutView} Implementation */
    @Override
    public float getDrawX() {
        return mBounds.left;
    }

    @Override
    public void setDrawX(float x) {
        mBounds.right = x + mBounds.width();
        mBounds.left = x;
    }

    @Override
    public float getDrawY() {
        return mBounds.top;
    }

    @Override
    public void setDrawY(float y) {
        mBounds.bottom = y + mBounds.height();
        mBounds.top = y;
    }

    @Override
    public float getWidth() {
        return mBounds.width();
    }

    @Override
    public void setWidth(float width) {
        mBounds.right = mBounds.left + width;
    }

    @Override
    public float getHeight() {
        return mBounds.height();
    }

    @Override
    public void setHeight(float height) {
        mBounds.bottom = mBounds.top + height;
    }

    /**
     * @param bounds A {@link RectF} representing the location of the button.
     */
    public void setBounds(RectF bounds) {
        mBounds.set(bounds);
    }

    /**
     * @return The opacity of the button.
     */
    public float getOpacity() {
        return mOpacity;
    }

    /**
     * @param opacity The opacity of the button.
     */
    public void setOpacity(float opacity) {
        mOpacity = opacity;
    }

    /**
     * @return The pressed state of the button.
     */
    public boolean isPressed() {
        return mIsPressed;
    }

    /**
     * @param state The pressed state of the button.
     */
    public void setPressed(boolean state) {
        mIsPressed = state;

        // clear isPressedFromMouse state.
        if (!state) {
            setPressedFromMouse(false);
        }
    }

    /**
     * @param state The pressed state of the button.
     * @param fromMouse Whether the event originates from a mouse.
     */
    public void setPressed(boolean state, boolean fromMouse) {
        mIsPressed = state;
        mIsPressedFromMouse = fromMouse;
    }

    /**
     * @return The incognito state of the button.
     */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /**
     * @param state The incognito state of the button.
     */
    public void setIncognito(boolean state) {
        mIsIncognito = state;
    }

    /**
     * @return Whether or not the button can be interacted with.
     */
    public boolean isEnabled() {
        return mIsEnabled;
    }

    /**
     * @param enabled Whether or not the button can be interacted with.
     */
    public void setEnabled(boolean enabled) {
        mIsEnabled = enabled;
    }

    /**
     * @param slop  The additional area outside of the button to be considered when
     *              checking click target bounds.
     */
    public void setClickSlop(float slop) {
        mClickSlop = slop;
    }

    /**
     * @return The Android resource id for this button based on it's state.
     */
    public int getResourceId() {
        if (isPressed()) {
            return isIncognito() ? mIncognitoPressedResource : mPressedResource;
        }
        return isIncognito() ? mIncognitoResource : mResource;
    }

    /**
     * Set state for a drag event.
     * @param x     The x offset of the event.
     * @param y     The y offset of the event.
     * @return      Whether or not the button is selected after the event.
     */
    public boolean drag(float x, float y) {
        if (!checkClickedOrHovered(x, y)) {
            setPressed(false);
            return false;
        }
        return isPressed();
    }

    /**
     * Set state for an onDown event.
     *
     * @param x The x offset of the event.
     * @param y The y offset of the event.
     * @param fromMouse Whether the event originates from a mouse.
     * @return Whether or not the close button was selected.
     */
    public boolean onDown(float x, float y, boolean fromMouse) {
        if (checkClickedOrHovered(x, y)) {
            setPressed(true, fromMouse);
            return true;
        }
        return false;
    }

    /**
     * @param x     The x offset of the event.
     * @param y     The y offset of the event.
     * @return      If the button was clicked or not.
     */
    public boolean click(float x, float y) {
        if (checkClickedOrHovered(x, y)) {
            setPressed(false, false);
            return true;
        }
        return false;
    }

    /**
     * Set state for an onUpOrCancel event.
     * @return Whether or not the button was selected.
     */
    public boolean onUpOrCancel() {
        boolean state = isPressed();
        setPressed(false, false);
        return state;
    }

    /**
     * Set whether button is hovered on.
     *
     * @param isHovered Whether the button is hovered on.
     */
    public void setHovered(boolean isHovered) {
        mIsHovered = isHovered;
    }

    /**
     * @return Whether the button is hovered on.
     */
    public boolean isHovered() {
        return mIsHovered;
    }

    /**
     * Set whether the button is pressed from mouse.
     *
     * @param isPressedFromMouse Whether the button is pressed from mouse.
     */
    private void setPressedFromMouse(boolean isPressedFromMouse) {
        mIsPressedFromMouse = isPressedFromMouse;
    }

    /**
     * @return Whether the button is pressed from mouse.
     */
    public boolean isPressedFromMouse() {
        return mIsPressed && mIsPressedFromMouse;
    }

    /**
     * @return Whether hover background should be applied to the button.
     */
    public boolean getShouldApplyHoverBackground() {
        return isHovered() || isPressedFromMouse();
    }
}