// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.RequiresApi;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.base.Callback;

/**
 * Utility class to use new APIs that were added in OMR1 (API level 27). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@RequiresApi(Build.VERSION_CODES.O_MR1)
public final class GlueApiHelperForOMR1 {
    private GlueApiHelperForOMR1() {}
}
