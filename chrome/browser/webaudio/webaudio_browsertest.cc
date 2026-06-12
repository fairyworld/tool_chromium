// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This test runs on Android as well as desktop platforms.
class WebAudioBrowserTest : public PlatformBrowserTest {
 public:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

IN_PROC_BROWSER_TEST_F(WebAudioBrowserTest,
                       VerifyDynamicsCompressorFingerprint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::DOMMessageQueue messages(web_contents());
  base::RunLoop run_loop;

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          "/webaudio/calls_dynamics_compressor_fingerprint.html")));

  // The document computes the DynamicsCompressor fingerprint and sends a
  // message back to the test. Receipt of the message indicates that the script
  // successfully completed.
  std::string fingerprint;
  ASSERT_TRUE(messages.WaitForMessage(&fingerprint));

  // NOTE: Changes to Web Audio code that alter the below fingerprints are
  // fine, and are cause for updating these expectations -- the issue is if
  // different devices return different fingerprints.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ("13.130919280310309", fingerprint);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ("13.130933322129067", fingerprint);
#elif BUILDFLAG(IS_MAC)
  EXPECT_EQ("13.130926895706125", fingerprint);
#elif BUILDFLAG(IS_WIN)
  EXPECT_EQ("13.130931982188713", fingerprint);
#else
  EXPECT_EQ("13.130926895706125", fingerprint);
#endif
}

}  // namespace
