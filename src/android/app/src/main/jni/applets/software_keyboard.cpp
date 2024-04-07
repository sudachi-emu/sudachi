// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <thread>

#include <jni.h>

#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "jni/android_common/android_common.h"
#include "jni/applets/software_keyboard.h"
#include "jni/id_cache.h"

static jclass s_software_keyboard_class;
static jclass s_keyboard_config_class;
static jclass s_keyboard_data_class;
static jmethodID s_swkbd_execute_normal;
static jmethodID s_swkbd_execute_inline;

namespace SoftwareKeyboard {

static jobject ToJKeyboardParams(const Core::Frontend::KeyboardInitializeParameters& config) {
    JNIEnv* env = IDCache::GetEnvForThread();
    jobject object = env->AllocObject(s_keyboard_config_class);

    env->SetObjectField(object,
                        env->GetFieldID(s_keyboard_config_class, "ok_text", "Ljava/lang/String;"),
                        ToJString(env, config.ok_text));
    env->SetObjectField(
        object, env->GetFieldID(s_keyboard_config_class, "header_text", "Ljava/lang/String;"),
        ToJString(env, config.header_text));
    env->SetObjectField(object,
                        env->GetFieldID(s_keyboard_config_class, "sub_text", "Ljava/lang/String;"),
                        ToJString(env, config.sub_text));
    env->SetObjectField(
        object, env->GetFieldID(s_keyboard_config_class, "guide_text", "Ljava/lang/String;"),
        ToJString(env, config.guide_text));
    env->SetObjectField(
        object, env->GetFieldID(s_keyboard_config_class, "initial_text", "Ljava/lang/String;"),
        ToJString(env, config.initial_text));
    env->SetShortField(object,
                       env->GetFieldID(s_keyboard_config_class, "left_optional_symbol_key", "S"),
                       static_cast<jshort>(config.left_optional_symbol_key));
    env->SetShortField(object,
                       env->GetFieldID(s_keyboard_config_class, "right_optional_symbol_key", "S"),
                       static_cast<jshort>(config.right_optional_symbol_key));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "max_text_length", "I"),
                     static_cast<jint>(config.max_text_length));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "min_text_length", "I"),
                     static_cast<jint>(config.min_text_length));
    env->SetIntField(object,
                     env->GetFieldID(s_keyboard_config_class, "initial_cursor_position", "I"),
                     static_cast<jint>(config.initial_cursor_position));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "type", "I"),
                     static_cast<jint>(config.type));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "password_mode", "I"),
                     static_cast<jint>(config.password_mode));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "text_draw_type", "I"),
                     static_cast<jint>(config.text_draw_type));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "key_disable_flags", "I"),
                     static_cast<jint>(config.key_disable_flags.raw));
    env->SetBooleanField(object,
                         env->GetFieldID(s_keyboard_config_class, "use_blur_background", "Z"),
                         static_cast<jboolean>(config.use_blur_background));
    env->SetBooleanField(object,
                         env->GetFieldID(s_keyboard_config_class, "enable_backspace_button", "Z"),
                         static_cast<jboolean>(config.enable_backspace_button));
    env->SetBooleanField(object,
                         env->GetFieldID(s_keyboard_config_class, "enable_return_button", "Z"),
                         static_cast<jboolean>(config.enable_return_button));
    env->SetBooleanField(object,
                         env->GetFieldID(s_keyboard_config_class, "disable_cancel_button", "Z"),
                         static_cast<jboolean>(config.disable_cancel_button));

    return object;
}

AndroidKeyboard::ResultData AndroidKeyboard::ResultData::CreateFromFrontend(jobject object) {
    JNIEnv* env = IDCache::GetEnvForThread();
    const jstring string = reinterpret_cast<jstring>(env->GetObjectField(
        object, env->GetFieldID(s_keyboard_data_class, "text", "Ljava/lang/String;")));
    return ResultData{GetJString(env, string),
                      static_cast<Service::AM::Frontend::SwkbdResult>(env->GetIntField(
                          object, env->GetFieldID(s_keyboard_data_class, "result", "I")))};
}

AndroidKeyboard::~AndroidKeyboard() = default;

void AndroidKeyboard::InitializeKeyboard(
    bool is_inline, Core::Frontend::KeyboardInitializeParameters initialize_parameters,
    SubmitNormalCallback submit_normal_callback_, SubmitInlineCallback submit_inline_callback_) {
    if (is_inline) {
        LOG_WARNING(
            Frontend,
            "(STUBBED) called, backend requested to initialize the inline software keyboard.");

        submit_inline_callback = std::move(submit_inline_callback_);
    } else {
        LOG_WARNING(
            Frontend,
            "(STUBBED) called, backend requested to initialize the normal software keyboard.");

        submit_normal_callback = std::move(submit_normal_callback_);
    }

    parameters = std::move(initialize_parameters);

    LOG_INFO(Frontend,
             "\nKeyboardInitializeParameters:"
             "\nok_text={}"
             "\nheader_text={}"
             "\nsub_text={}"
             "\nguide_text={}"
             "\ninitial_text={}"
             "\nmax_text_length={}"
             "\nmin_text_length={}"
             "\ninitial_cursor_position={}"
             "\ntype={}"
             "\npassword_mode={}"
             "\ntext_draw_type={}"
             "\nkey_disable_flags={}"
             "\nuse_blur_background={}"
             "\nenable_backspace_button={}"
             "\nenable_return_button={}"
             "\ndisable_cancel_button={}",
             Common::UTF16ToUTF8(parameters.ok_text), Common::UTF16ToUTF8(parameters.header_text),
             Common::UTF16ToUTF8(parameters.sub_text), Common::UTF16ToUTF8(parameters.guide_text),
             Common::UTF16ToUTF8(parameters.initial_text), parameters.max_text_length,
             parameters.min_text_length, parameters.initial_cursor_position, parameters.type,
             parameters.password_mode, parameters.text_draw_type, parameters.key_disable_flags.raw,
             parameters.use_blur_background, parameters.enable_backspace_button,
             parameters.enable_return_button, parameters.disable_cancel_button);
}

void AndroidKeyboard::ShowNormalKeyboard() const {
    LOG_DEBUG(Frontend, "called, backend requested to show the normal software keyboard.");

    ResultData data{};

    // Pivot to a new thread, as we cannot call GetEnvForThread() from a Fiber.
    std::thread([&] {
        data = ResultData::CreateFromFrontend(IDCache::GetEnvForThread()->CallStaticObjectMethod(
            s_software_keyboard_class, s_swkbd_execute_normal, ToJKeyboardParams(parameters)));
    }).join();

    SubmitNormalText(data);
}

void AndroidKeyboard::ShowTextCheckDialog(
    Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
    std::u16string text_check_message) const {
    LOG_WARNING(Frontend, "(STUBBED) called, backend requested to show the text check dialog.");
}

void AndroidKeyboard::ShowInlineKeyboard(
    Core::Frontend::InlineAppearParameters appear_parameters) const {
    LOG_WARNING(Frontend,
                "(STUBBED) called, backend requested to show the inline software keyboard.");

    LOG_INFO(Frontend,
             "\nInlineAppearParameters:"
             "\nmax_text_length={}"
             "\nmin_text_length={}"
             "\nkey_top_scale_x={}"
             "\nkey_top_scale_y={}"
             "\nkey_top_translate_x={}"
             "\nkey_top_translate_y={}"
             "\ntype={}"
             "\nkey_disable_flags={}"
             "\nkey_top_as_floating={}"
             "\nenable_backspace_button={}"
             "\nenable_return_button={}"
             "\ndisable_cancel_button={}",
             appear_parameters.max_text_length, appear_parameters.min_text_length,
             appear_parameters.key_top_scale_x, appear_parameters.key_top_scale_y,
             appear_parameters.key_top_translate_x, appear_parameters.key_top_translate_y,
             appear_parameters.type, appear_parameters.key_disable_flags.raw,
             appear_parameters.key_top_as_floating, appear_parameters.enable_backspace_button,
             appear_parameters.enable_return_button, appear_parameters.disable_cancel_button);

    // Pivot to a new thread, as we cannot call GetEnvForThread() from a Fiber.
    m_is_inline_active = true;
    std::thread([&] {
        IDCache::GetEnvForThread()->CallStaticVoidMethod(
            s_software_keyboard_class, s_swkbd_execute_inline, ToJKeyboardParams(parameters));
    }).join();
}

void AndroidKeyboard::HideInlineKeyboard() const {
    LOG_WARNING(Frontend,
                "(STUBBED) called, backend requested to hide the inline software keyboard.");
}

void AndroidKeyboard::InlineTextChanged(
    Core::Frontend::InlineTextParameters text_parameters) const {
    LOG_WARNING(Frontend,
                "(STUBBED) called, backend requested to change the inline keyboard text.");

    LOG_INFO(Frontend,
             "\nInlineTextParameters:"
             "\ninput_text={}"
             "\ncursor_position={}",
             Common::UTF16ToUTF8(text_parameters.input_text), text_parameters.cursor_position);

    submit_inline_callback(Service::AM::Frontend::SwkbdReplyType::ChangedString,
                           text_parameters.input_text, text_parameters.cursor_position);
}

void AndroidKeyboard::ExitKeyboard() const {
    LOG_WARNING(Frontend, "(STUBBED) called, backend requested to exit the software keyboard.");
}

void AndroidKeyboard::SubmitInlineKeyboardText(std::u16string submitted_text) {
    if (!m_is_inline_active) {
        return;
    }

    m_current_text += submitted_text;

    submit_inline_callback(Service::AM::Frontend::SwkbdReplyType::ChangedString, m_current_text,
                           m_current_text.size());
}

void AndroidKeyboard::SubmitInlineKeyboardInput(int key_code) {
    static constexpr int KEYCODE_BACK = 4;
    static constexpr int KEYCODE_ENTER = 66;
    static constexpr int KEYCODE_DEL = 67;

    if (!m_is_inline_active) {
        return;
    }

    switch (key_code) {
    case KEYCODE_BACK:
    case KEYCODE_ENTER:
        m_is_inline_active = false;
        submit_inline_callback(Service::AM::Frontend::SwkbdReplyType::DecidedEnter, m_current_text,
                               static_cast<s32>(m_current_text.size()));
        break;
    case KEYCODE_DEL:
        m_current_text.pop_back();
        submit_inline_callback(Service::AM::Frontend::SwkbdReplyType::ChangedString, m_current_text,
                               m_current_text.size());
        break;
    }
}

void AndroidKeyboard::SubmitNormalText(const ResultData& data) const {
    submit_normal_callback(data.result, Common::UTF8ToUTF16(data.text), true);
}

void InitJNI(JNIEnv* env) {
    s_software_keyboard_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("org/sudachi/sudachi_emu/applets/keyboard/SoftwareKeyboard")));
    s_keyboard_config_class = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass(
        "org/sudachi/sudachi_emu/applets/keyboard/SoftwareKeyboard$KeyboardConfig")));
    s_keyboard_data_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("org/sudachi/sudachi_emu/applets/keyboard/SoftwareKeyboard$KeyboardData")));

    s_swkbd_execute_normal =
        env->GetStaticMethodID(s_software_keyboard_class, "executeNormal",
                               "(Lorg/sudachi/sudachi_emu/applets/keyboard/"
                               "SoftwareKeyboard$KeyboardConfig;)Lorg/sudachi/sudachi_emu/"
                               "applets/keyboard/SoftwareKeyboard$KeyboardData;");
    s_swkbd_execute_inline = env->GetStaticMethodID(
        s_software_keyboard_class, "executeInline",
        "(Lorg/sudachi/sudachi_emu/applets/keyboard/SoftwareKeyboard$KeyboardConfig;)V");
}

void CleanupJNI(JNIEnv* env) {
    env->DeleteGlobalRef(s_software_keyboard_class);
    env->DeleteGlobalRef(s_keyboard_config_class);
    env->DeleteGlobalRef(s_keyboard_data_class);
}

} // namespace SoftwareKeyboard
