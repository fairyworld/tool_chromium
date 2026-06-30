// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicNudgeControllerAndroid : public GlicNudgeController {
 public:
  explicit GlicNudgeControllerAndroid(tabs::TabInterface& tab);
  GlicNudgeControllerAndroid(const GlicNudgeControllerAndroid&) = delete;
  GlicNudgeControllerAndroid& operator=(const GlicNudgeControllerAndroid&) =
      delete;
  ~GlicNudgeControllerAndroid() override;

  // GlicNudgeController:
  void SetTabStripDelegate(GlicSplitButtonDelegate* delegate) override;
  void SetToolbarDelegate(GlicSplitButtonDelegate* delegate) override;

  void UpdateNudgeLabel(content::WebContents* web_contents,
                        const std::string& nudge_label,
                        std::optional<std::string> prompt_suggestion,
                        const std::string& anchored_message_text,
                        std::optional<GlicNudgeActivity> activity,
                        GlicNudgeActivityCallback callback) override;
  void OnNudgeActivity(GlicNudgeActivity activity) override;

  std::optional<std::string> GetPromptSuggestion() override;
  void ClearPromptSuggestion() override;

 private:
  void OnTabWillDeactivate(tabs::TabInterface* tab);

  raw_ref<tabs::TabInterface> tab_;
  raw_ptr<GlicSplitButtonDelegate> tab_strip_delegate_ = nullptr;
  std::optional<std::string> prompt_suggestion_;
  GlicNudgeActivityCallback nudge_activity_callback_;
  std::unique_ptr<GlicSplitButtonDelegate> delegate_;
  base::CallbackListSubscription tab_deactivate_subscription_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_NUDGE_CONTROLLER_ANDROID_H_
