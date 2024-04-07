// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <jni.h>

#include "core/frontend/applets/software_keyboard.h"

namespace Common::Android::SoftwareKeyboard {

class AndroidKeyboard final : public Core::Frontend::SoftwareKeyboardApplet {
public:
    ~AndroidKeyboard() override;

    void Close() const override {
        ExitKeyboard();
    }

    void InitializeKeyboard(bool is_inline,
                            Core::Frontend::KeyboardInitializeParameters initialize_parameters,
                            SubmitNormalCallback submit_normal_callback_,
                            SubmitInlineCallback submit_inline_callback_) override;

    void ShowNormalKeyboard() const override;

    void ShowTextCheckDialog(Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
                             std::u16string text_check_message) const override;

    void ShowInlineKeyboard(
        Core::Frontend::InlineAppearParameters appear_parameters) const override;

    void HideInlineKeyboard() const override;

    void InlineTextChanged(Core::Frontend::InlineTextParameters text_parameters) const override;

    void ExitKeyboard() const override;

    void SubmitInlineKeyboardText(std::u16string submitted_text);

    void SubmitInlineKeyboardInput(int key_code);

private:
    struct ResultData {
        static ResultData CreateFromFrontend(jobject object);

        std::string text;
        Service::AM::Frontend::SwkbdResult result{};
    };

    void SubmitNormalText(const ResultData& result) const;

    Core::Frontend::KeyboardInitializeParameters parameters{};

    mutable SubmitNormalCallback submit_normal_callback;
    mutable SubmitInlineCallback submit_inline_callback;

private:
    mutable bool m_is_inline_active{};
    std::u16string m_current_text;
};

// Should be called in JNI_Load
void InitJNI(JNIEnv* env);

// Should be called in JNI_Unload
void CleanupJNI(JNIEnv* env);

} // namespace Common::Android::SoftwareKeyboard

// Native function calls
extern "C" {
JNIEXPORT jobject JNICALL Java_org_citra_citra_1emu_applets_SoftwareKeyboard_ValidateFilters(
    JNIEnv* env, jclass clazz, jstring text);

JNIEXPORT jobject JNICALL Java_org_citra_citra_1emu_applets_SoftwareKeyboard_ValidateInput(
    JNIEnv* env, jclass clazz, jstring text);
}
