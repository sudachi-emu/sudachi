// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/android/android_common.h"
#include "core/core.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/patch_manager.h"
#include "core/loader/loader.h"
#include "core/loader/nro.h"
#include "native.h"

struct RomMetadata {
    std::string title;
    u64 programId;
    std::string developer;
    std::string version;
    std::vector<u8> icon;
    bool isHomebrew;
};

std::unordered_map<std::string, RomMetadata> m_rom_metadata_cache;

RomMetadata CacheRomMetadata(const std::string& path) {
    const auto file =
        Core::GetGameFileFromPath(EmulationSession::GetInstance().System().GetFilesystem(), path);
    auto loader = Loader::GetLoader(EmulationSession::GetInstance().System(), file, 0, 0);

    RomMetadata entry;
    loader->ReadTitle(entry.title);
    loader->ReadProgramId(entry.programId);
    loader->ReadIcon(entry.icon);

    const FileSys::PatchManager pm{
        entry.programId, EmulationSession::GetInstance().System().GetFileSystemController(),
        EmulationSession::GetInstance().System().GetContentProvider()};
    const auto control = pm.GetControlMetadata();

    if (control.first != nullptr) {
        entry.developer = control.first->GetDeveloperName();
        entry.version = control.first->GetVersionString();
    } else {
        FileSys::NACP nacp;
        if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success) {
            entry.developer = nacp.GetDeveloperName();
        } else {
            entry.developer = "";
        }

        entry.version = "1.0.0";
    }

    if (loader->GetFileType() == Loader::FileType::NRO) {
        auto loader_nro = reinterpret_cast<Loader::AppLoader_NRO*>(loader.get());
        entry.isHomebrew = loader_nro->IsHomebrew();
    } else {
        entry.isHomebrew = false;
    }

    m_rom_metadata_cache[path] = entry;

    return entry;
}

RomMetadata GetRomMetadata(const std::string& path, bool reload = false) {
    if (reload) {
        return CacheRomMetadata(path);
    }

    if (auto search = m_rom_metadata_cache.find(path); search != m_rom_metadata_cache.end()) {
        return search->second;
    }

    return CacheRomMetadata(path);
}

extern "C" {

jboolean Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getIsValid(JNIEnv* env, jobject obj,
                                                                     jstring jpath) {
    const auto file = EmulationSession::GetInstance().System().GetFilesystem()->OpenFile(
        Common::Android::GetJString(env, jpath), FileSys::OpenMode::Read);
    if (!file) {
        return false;
    }

    auto loader = Loader::GetLoader(EmulationSession::GetInstance().System(), file);
    if (!loader) {
        return false;
    }

    const auto file_type = loader->GetFileType();
    if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
        return false;
    }

    u64 program_id = 0;
    Loader::ResultStatus res = loader->ReadProgramId(program_id);
    if (res != Loader::ResultStatus::Success) {
        return false;
    }
    return true;
}

jstring Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getTitle(JNIEnv* env, jobject obj,
                                                                  jstring jpath) {
    return Common::Android::ToJString(
        env, GetRomMetadata(Common::Android::GetJString(env, jpath)).title);
}

jstring Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getProgramId(JNIEnv* env, jobject obj,
                                                                      jstring jpath) {
    return Common::Android::ToJString(
        env, std::to_string(GetRomMetadata(Common::Android::GetJString(env, jpath)).programId));
}

jstring Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getDeveloper(JNIEnv* env, jobject obj,
                                                                      jstring jpath) {
    return Common::Android::ToJString(
        env, GetRomMetadata(Common::Android::GetJString(env, jpath)).developer);
}

jstring Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getVersion(JNIEnv* env, jobject obj,
                                                                    jstring jpath,
                                                                    jboolean jreload) {
    return Common::Android::ToJString(
        env, GetRomMetadata(Common::Android::GetJString(env, jpath), jreload).version);
}

jbyteArray Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getIcon(JNIEnv* env, jobject obj,
                                                                    jstring jpath) {
    auto icon_data = GetRomMetadata(Common::Android::GetJString(env, jpath)).icon;
    jbyteArray icon = env->NewByteArray(static_cast<jsize>(icon_data.size()));
    env->SetByteArrayRegion(icon, 0, env->GetArrayLength(icon),
                            reinterpret_cast<jbyte*>(icon_data.data()));
    return icon;
}

jboolean Java_org_sudachi_sudachi_1emu_utils_GameMetadata_getIsHomebrew(JNIEnv* env, jobject obj,
                                                                        jstring jpath) {
    return static_cast<jboolean>(
        GetRomMetadata(Common::Android::GetJString(env, jpath)).isHomebrew);
}

void Java_org_sudachi_sudachi_1emu_utils_GameMetadata_resetMetadata(JNIEnv* env, jobject obj) {
    m_rom_metadata_cache.clear();
}

} // extern "C"
