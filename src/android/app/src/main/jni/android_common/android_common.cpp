// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "jni/android_common/android_common.h"

#include <string>
#include <string_view>

#include <jni.h>

#include "common/string_util.h"
#include "jni/id_cache.h"

std::string GetJString(JNIEnv* env, jstring jstr) {
    if (!jstr) {
        return {};
    }

    const jchar* jchars = env->GetStringChars(jstr, nullptr);
    const jsize length = env->GetStringLength(jstr);
    const std::u16string_view string_view(reinterpret_cast<const char16_t*>(jchars), length);
    const std::string converted_string = Common::UTF16ToUTF8(string_view);
    env->ReleaseStringChars(jstr, jchars);

    return converted_string;
}

jstring ToJString(JNIEnv* env, std::string_view str) {
    const std::u16string converted_string = Common::UTF8ToUTF16(str);
    return env->NewString(reinterpret_cast<const jchar*>(converted_string.data()),
                          static_cast<jint>(converted_string.size()));
}

jstring ToJString(JNIEnv* env, std::u16string_view str) {
    return ToJString(env, Common::UTF16ToUTF8(str));
}

double GetJDouble(JNIEnv* env, jobject jdouble) {
    return env->GetDoubleField(jdouble, IDCache::GetDoubleValueField());
}

jobject ToJDouble(JNIEnv* env, double value) {
    return env->NewObject(IDCache::GetDoubleClass(), IDCache::GetDoubleConstructor(), value);
}

s32 GetJInteger(JNIEnv* env, jobject jinteger) {
    return env->GetIntField(jinteger, IDCache::GetIntegerValueField());
}

jobject ToJInteger(JNIEnv* env, s32 value) {
    return env->NewObject(IDCache::GetIntegerClass(), IDCache::GetIntegerConstructor(), value);
}

bool GetJBoolean(JNIEnv* env, jobject jboolean) {
    return env->GetBooleanField(jboolean, IDCache::GetBooleanValueField());
}

jobject ToJBoolean(JNIEnv* env, bool value) {
    return env->NewObject(IDCache::GetBooleanClass(), IDCache::GetBooleanConstructor(), value);
}
