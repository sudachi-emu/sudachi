// SPDX-FileCopyrightText: Copyright 2022 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/frontend/applets/mii_edit.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applets/applet_mii_edit.h"
#include "core/hle/service/mii/mii.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/sm/sm.h"

namespace Service::AM::Applets {

MiiEdit::MiiEdit(Core::System& system_, LibraryAppletMode applet_mode_,
                 const Core::Frontend::MiiEditApplet& frontend_)
    : Applet{system_, applet_mode_}, frontend{frontend_}, system{system_} {}

MiiEdit::~MiiEdit() = default;

void MiiEdit::Initialize() {
    // Note: MiiEdit is not initialized with common arguments.
    //       Instead, it is initialized by an AppletInput storage with size 0x100 bytes.
    //       Do NOT call Applet::Initialize() here.

    const auto storage = broker.PopNormalDataToApplet();
    ASSERT(storage != nullptr);

    const auto applet_input_data = storage->GetData();
    ASSERT(applet_input_data.size() >= sizeof(MiiEditAppletInputCommon));

    std::memcpy(&applet_input_common, applet_input_data.data(), sizeof(MiiEditAppletInputCommon));

    LOG_INFO(Service_AM,
             "Initializing MiiEdit Applet with MiiEditAppletVersion={} and MiiEditAppletMode={}",
             applet_input_common.version, applet_input_common.applet_mode);

    switch (applet_input_common.version) {
    case MiiEditAppletVersion::Version3:
        ASSERT(applet_input_data.size() ==
               sizeof(MiiEditAppletInputCommon) + sizeof(MiiEditAppletInputV3));
        std::memcpy(&applet_input_v3, applet_input_data.data() + sizeof(MiiEditAppletInputCommon),
                    sizeof(MiiEditAppletInputV3));
        break;
    case MiiEditAppletVersion::Version4:
        ASSERT(applet_input_data.size() ==
               sizeof(MiiEditAppletInputCommon) + sizeof(MiiEditAppletInputV4));
        std::memcpy(&applet_input_v4, applet_input_data.data() + sizeof(MiiEditAppletInputCommon),
                    sizeof(MiiEditAppletInputV4));
        break;
    default:
        UNIMPLEMENTED_MSG("Unknown MiiEditAppletVersion={} with size={}",
                          applet_input_common.version, applet_input_data.size());
        ASSERT(applet_input_data.size() >=
               sizeof(MiiEditAppletInputCommon) + sizeof(MiiEditAppletInputV4));
        std::memcpy(&applet_input_v4, applet_input_data.data() + sizeof(MiiEditAppletInputCommon),
                    sizeof(MiiEditAppletInputV4));
        break;
    }

    manager = system.ServiceManager().GetService<Mii::MiiDBModule>("mii:e")->GetMiiManager();
    if (manager == nullptr) {
        manager = std::make_shared<Mii::MiiManager>();
    }
    manager->Initialize(metadata);
}

bool MiiEdit::TransactionComplete() const {
    return is_complete;
}

Result MiiEdit::GetStatus() const {
    return ResultSuccess;
}

void MiiEdit::ExecuteInteractive() {
    ASSERT_MSG(false, "Attempted to call interactive execution on non-interactive applet.");
}

void MiiEdit::Execute() {
    if (is_complete) {
        return;
    }

    // This is a default stub for each of the MiiEdit applet modes.
    switch (applet_input_common.applet_mode) {
    case MiiEditAppletMode::ShowMiiEdit:
    case MiiEditAppletMode::AppendMiiImage:
    case MiiEditAppletMode::UpdateMiiImage:
        MiiEditOutput(MiiEditResult::Success, 0);
        break;
    case MiiEditAppletMode::AppendMii: {
        Mii::StoreData store_data{};
        store_data.BuildRandom(Mii::Age::All, Mii::Gender::All, Mii::Race::All);
        store_data.SetNickname({u's', u'u', u'd', u'a'});
        store_data.SetChecksum();
        const auto result = manager->AddOrReplace(metadata, store_data);

        if (result.IsError()) {
            MiiEditOutput(MiiEditResult::Cancel, 0);
            break;
        }

        s32 index = manager->FindIndex(store_data.GetCreateId(), false);

        if (index == -1) {
            MiiEditOutput(MiiEditResult::Cancel, 0);
            break;
        }

        MiiEditOutput(MiiEditResult::Success, index);
        break;
    }
    case MiiEditAppletMode::CreateMii: {
        Mii::CharInfo char_info{};
        manager->BuildRandom(char_info, Mii::Age::All, Mii::Gender::All, Mii::Race::All);

        const MiiEditCharInfo edit_char_info{
            .mii_info{char_info},
        };

        MiiEditOutputForCharInfoEditing(MiiEditResult::Success, edit_char_info);
        break;
    }
    case MiiEditAppletMode::EditMii: {
        const MiiEditCharInfo edit_char_info{
            .mii_info{applet_input_v4.char_info.mii_info},
        };

        MiiEditOutputForCharInfoEditing(MiiEditResult::Success, edit_char_info);
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unknown MiiEditAppletMode={}", applet_input_common.applet_mode);

        MiiEditOutput(MiiEditResult::Success, 0);
        break;
    }
}

void MiiEdit::MiiEditOutput(MiiEditResult result, s32 index) {
    const MiiEditAppletOutput applet_output{
        .result{result},
        .index{index},
    };

    LOG_INFO(Input, "called, result={}, index={}", result, index);

    std::vector<u8> out_data(sizeof(MiiEditAppletOutput));
    std::memcpy(out_data.data(), &applet_output, sizeof(MiiEditAppletOutput));

    is_complete = true;

    broker.PushNormalDataFromApplet(std::make_shared<IStorage>(system, std::move(out_data)));
    broker.SignalStateChanged();
}

void MiiEdit::MiiEditOutputForCharInfoEditing(MiiEditResult result,
                                              const MiiEditCharInfo& char_info) {
    const MiiEditAppletOutputForCharInfoEditing applet_output{
        .result{result},
        .char_info{char_info},
    };

    std::vector<u8> out_data(sizeof(MiiEditAppletOutputForCharInfoEditing));
    std::memcpy(out_data.data(), &applet_output, sizeof(MiiEditAppletOutputForCharInfoEditing));

    is_complete = true;

    broker.PushNormalDataFromApplet(std::make_shared<IStorage>(system, std::move(out_data)));
    broker.SignalStateChanged();
}

Result MiiEdit::RequestExit() {
    frontend.Close();
    R_SUCCEED();
}

} // namespace Service::AM::Applets
