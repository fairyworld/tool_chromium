// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_delegate_android.h"

#include <algorithm>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/glic/android/jni_headers/GlicNudgeDelegateBridge_jni.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace glic {

namespace {
std::vector<GlicNudgeDelegateAndroid*>& GetDelegates() {
  static base::NoDestructor<std::vector<GlicNudgeDelegateAndroid*>> delegates;
  return *delegates;
}
}  // namespace

GlicNudgeDelegateAndroid::GlicNudgeDelegateAndroid(
    GlicNudgeController& controller,
    tabs::TabInterface& tab)
    : controller_(controller), tab_(tab) {
  GetDelegates().push_back(this);
}

GlicNudgeDelegateAndroid::~GlicNudgeDelegateAndroid() {
  auto& delegates = GetDelegates();
  auto it = std::find(delegates.begin(), delegates.end(), this);
  if (it != delegates.end()) {
    delegates.erase(it);
  }
}

void GlicNudgeDelegateAndroid::OnTriggerGlicNudgeUI(NudgeParams params) {
  ui::WindowAndroid* window = GetWindowAndroid();
  if (!window) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_label =
      base::android::ConvertUTF8ToJavaString(env, params.label);
  base::android::ScopedJavaLocalRef<jstring> j_msg =
      base::android::ConvertUTF8ToJavaString(env, params.anchored_message_text);
  base::android::ScopedJavaLocalRef<jstring> j_suggestion =
      base::android::ConvertUTF8ToJavaString(
          env, params.prompt_suggestion.value_or(""));

  Java_GlicNudgeDelegateBridge_triggerGlicNudge(env, window->GetJavaObject(),
                                                j_label, j_msg, j_suggestion);
}

void GlicNudgeDelegateAndroid::OnHideGlicNudgeUI() {
  ui::WindowAndroid* window = GetWindowAndroid();
  if (!window) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GlicNudgeDelegateBridge_hideGlicNudge(env, window->GetJavaObject());
}

bool GlicNudgeDelegateAndroid::GetIsShowingGlicNudge() {
  ui::WindowAndroid* window = GetWindowAndroid();
  if (!window) {
    return false;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_GlicNudgeDelegateBridge_getIsShowingGlicNudge(
      env, window->GetJavaObject());
}

void GlicNudgeDelegateAndroid::OnNudgeActivity(GlicNudgeActivity activity) {
  controller_->OnNudgeActivity(activity);
}

ui::WindowAndroid* GlicNudgeDelegateAndroid::GetWindowAndroid() {
  if (content::WebContents* contents = tab_->GetContents()) {
    return contents->GetTopLevelNativeWindow();
  }
  return nullptr;
}

bool GlicNudgeDelegateAndroid::IsActiveTab() {
  return tab_->IsActivated();
}

static void JNI_GlicNudgeDelegateBridge_OnNudgeActivity(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_window,
    int32_t event) {
  ui::WindowAndroid* target_window =
      ui::WindowAndroid::FromJavaWindowAndroid(j_window);
  if (!target_window) {
    return;
  }
  for (GlicNudgeDelegateAndroid* delegate : GetDelegates()) {
    if (delegate->GetWindowAndroid() == target_window &&
        delegate->IsActiveTab()) {
      delegate->OnNudgeActivity(static_cast<GlicNudgeActivity>(event));
      break;
    }
  }
}

DEFINE_JNI(GlicNudgeDelegateBridge)

}  // namespace glic
