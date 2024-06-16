// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

public class TopicsUtils {
    /**
     * Fetches the icon resource id for a given topic.
     *
     * @param context The current context.
     * @param topic The topic to fetch the icon for.
     * @return icon id if it exists; 0 if it doesn't.
     */
    @SuppressWarnings("DiscouragedApi")
    public static int getIconResourceIdForTopic(Context context, Topic topic) {
        // Check all the previous taxonomy versions as well, in case the version is increased.
        int taxonomyVersion = topic.getTaxonomyVersion();
        if (taxonomyVersion < 1) taxonomyVersion = 1;
        for (int version = taxonomyVersion; version > 0; version--) {
            String iconName = String.format("topic_taxonomy_%s_id_%s", version, topic.getTopicId());
            int iconId =
                    context.getResources()
                            .getIdentifier(iconName, "drawable", context.getPackageName());
            if (iconId != 0) return iconId;
        }
        return 0;
    }

    /**
     * Proactive Topics Blocking should not be shown if feature is not enabled. If include-mode-b
     * param is false and user is part of Mode B, this should return false.
     *
     * @return boolean of whether all conditions are met.
     */
    public static boolean shouldShowProactiveTopicsBlocking() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_PROACTIVE_TOPICS_BLOCKING)) {
            return false;
        }
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.PRIVACY_SANDBOX_PROACTIVE_TOPICS_BLOCKING,
                        "include-mode-b",
                        false)
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING);
    }
}