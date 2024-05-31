// SPDX-FileCopyrightText: Copyright 2022 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/glue/time/static.h"
#include "core/hle/service/psc/time/steady_clock.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4701) // Potentially uninitialized local variable 'result' used
#endif

#include <boost/crc.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <fmt/format.h>

#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/input.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/tiny_mt.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mii/mii_manager.h"
#include "core/hle/service/nfc/common/amiibo_crypto.h"
#include "core/hle/service/nfc/common/device.h"
#include "core/hle/service/nfc/mifare_result.h"
#include "core/hle/service/nfc/nfc_result.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "hid_core/hid_types.h"

namespace Service::NFC {
NfcDevice::NfcDevice(Core::HID::NpadIdType npad_id_, Core::System& system_,
                     KernelHelpers::ServiceContext& service_context_,
                     Kernel::KEvent* availability_change_event_)
    : npad_id{npad_id_}, system{system_}, service_context{service_context_},
      availability_change_event{availability_change_event_} {
    activate_event = service_context.CreateEvent("NFC:ActivateEvent");
    deactivate_event = service_context.CreateEvent("NFC:DeactivateEvent");
    npad_device = system.HIDCore().GetEmulatedController(npad_id);

    Core::HID::ControllerUpdateCallback engine_callback{
        .on_change = [this](Core::HID::ControllerTriggerType type) { NpadUpdate(type); },
        .is_npad_service = false,
    };
    is_controller_set = true;
    callback_key = npad_device->SetCallback(engine_callback);
}

NfcDevice::~NfcDevice() {
    service_context.CloseEvent(activate_event);
    service_context.CloseEvent(deactivate_event);
    if (!is_controller_set) {
        return;
    }
    npad_device->DeleteCallback(callback_key);
    is_controller_set = false;
};

void NfcDevice::NpadUpdate(Core::HID::ControllerTriggerType type) {
    if (type == Core::HID::ControllerTriggerType::Connected) {
        Initialize();
        availability_change_event->Signal();
        return;
    }

    if (type == Core::HID::ControllerTriggerType::Disconnected) {
        Finalize();
        availability_change_event->Signal();
        return;
    }

    if (!is_initialized) {
        return;
    }

    if (!npad_device->IsConnected()) {
        return;
    }

    // Ensure nfc mode is always active
    if (npad_device->GetPollingMode(Core::HID::EmulatedDeviceIndex::RightIndex) ==
        Common::Input::PollingMode::Active) {
        npad_device->SetPollingMode(Core::HID::EmulatedDeviceIndex::RightIndex,
                                    Common::Input::PollingMode::NFC);
    }

    if (type != Core::HID::ControllerTriggerType::Nfc) {
        return;
    }

    const auto nfc_status = npad_device->GetNfc();
    switch (nfc_status.state) {
    case Common::Input::NfcState::NewAmiibo:
        LoadNfcTag(nfc_status.protocol, nfc_status.tag_type, nfc_status.uuid_length,
                   nfc_status.uuid);
        break;
    case Common::Input::NfcState::AmiiboRemoved:
        if (device_state == DeviceState::Initialized || device_state == DeviceState::TagRemoved) {
            break;
        }
        if (device_state != DeviceState::SearchingForTag) {
            CloseNfcTag();
        }
        break;
    default:
        break;
    }
}

bool NfcDevice::LoadNfcTag(u8 protocol, u8 tag_type, u8 uuid_length, UniqueSerialNumber uuid) {
    if (device_state != DeviceState::SearchingForTag) {
        LOG_ERROR(Service_NFC, "Game is not looking for nfc tag, current state {}", device_state);
        return false;
    }

    if ((protocol & static_cast<u8>(allowed_protocols)) == 0) {
        LOG_ERROR(Service_NFC, "Protocol not supported {}", protocol);
        return false;
    }

    real_tag_info = {
        .uuid = uuid,
        .uuid_length = uuid_length,
        .protocol = static_cast<NfcProtocol>(protocol),
        .tag_type = static_cast<TagType>(tag_type),
    };

    device_state = DeviceState::TagFound;
    deactivate_event->GetReadableEvent().Clear();
    activate_event->Signal();
    return true;
}

bool NfcDevice::LoadAmiiboData() {
    std::vector<u8> data{};

    if (!npad_device->ReadAmiiboData(data)) {
        return false;
    }

    if (data.size() < sizeof(NFP::EncryptedNTAG215File)) {
        LOG_ERROR(Service_NFC, "Not an amiibo, size={}", data.size());
        return false;
    }

    memcpy(&tag_data, data.data(), sizeof(NFP::EncryptedNTAG215File));
    is_plain_amiibo = NFP::AmiiboCrypto::IsAmiiboValid(tag_data);
    is_write_protected = false;

    // Fallback for plain amiibos
    if (is_plain_amiibo) {
        LOG_INFO(Service_NFP, "Using plain amiibo");
        encrypted_tag_data = NFP::AmiiboCrypto::EncodedDataToNfcData(tag_data);
        return true;
    }

    // Fallback for encrypted amiibos without keys
    if (!NFP::AmiiboCrypto::IsKeyAvailable()) {
        LOG_INFO(Service_NFC, "Loading amiibo without keys");
        memcpy(&encrypted_tag_data, data.data(), sizeof(NFP::EncryptedNTAG215File));
        BuildAmiiboWithoutKeys(tag_data, encrypted_tag_data);
        is_plain_amiibo = true;
        is_write_protected = true;
        return true;
    }

    LOG_INFO(Service_NFP, "Using encrypted amiibo");
    tag_data = {};
    memcpy(&encrypted_tag_data, data.data(), sizeof(NFP::EncryptedNTAG215File));
    return true;
}

void NfcDevice::CloseNfcTag() {
    LOG_INFO(Service_NFC, "Remove nfc tag");

    if (device_state == DeviceState::TagMounted) {
        Unmount();
    }

    device_state = DeviceState::TagRemoved;
    encrypted_tag_data = {};
    tag_data = {};
    activate_event->GetReadableEvent().Clear();
    deactivate_event->Signal();
}

Kernel::KReadableEvent& NfcDevice::GetActivateEvent() const {
    return activate_event->GetReadableEvent();
}

Kernel::KReadableEvent& NfcDevice::GetDeactivateEvent() const {
    return deactivate_event->GetReadableEvent();
}

void NfcDevice::Initialize() {
    device_state = npad_device->HasNfc() ? DeviceState::Initialized : DeviceState::Unavailable;
    encrypted_tag_data = {};
    tag_data = {};

    if (device_state != DeviceState::Initialized) {
        return;
    }

    is_initialized = npad_device->AddNfcHandle();
}

void NfcDevice::Finalize() {
    if (npad_device->IsConnected()) {
        if (device_state == DeviceState::TagMounted) {
            Unmount();
        }
        if (device_state == DeviceState::SearchingForTag ||
            device_state == DeviceState::TagRemoved) {
            StopDetection();
        }
    }

    if (device_state != DeviceState::Unavailable) {
        npad_device->RemoveNfcHandle();
    }

    device_state = DeviceState::Unavailable;
    is_initialized = false;
}

Result NfcDevice::StartDetection(NfcProtocol allowed_protocol) {
    if (device_state != DeviceState::Initialized && device_state != DeviceState::TagRemoved) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        return ResultWrongDeviceState;
    }

    if (!npad_device->StartNfcPolling()) {
        LOG_ERROR(Service_NFC, "Nfc polling not supported");
        return ResultNfcDisabled;
    }

    device_state = DeviceState::SearchingForTag;
    allowed_protocols = allowed_protocol;
    return ResultSuccess;
}

Result NfcDevice::StopDetection() {
    if (device_state == DeviceState::Initialized) {
        return ResultSuccess;
    }

    if (device_state == DeviceState::TagFound || device_state == DeviceState::TagMounted) {
        CloseNfcTag();
    }

    if (device_state == DeviceState::SearchingForTag || device_state == DeviceState::TagRemoved) {
        npad_device->StopNfcPolling();
        device_state = DeviceState::Initialized;
        return ResultSuccess;
    }

    LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
    return ResultWrongDeviceState;
}

Result NfcDevice::GetTagInfo(NFP::TagInfo& tag_info) const {
    if (device_state != DeviceState::TagFound && device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    tag_info = real_tag_info;

    // Generate random UUID to bypass amiibo load limits
    if (real_tag_info.tag_type == TagType::Type2 && Settings::values.random_amiibo_id) {
        Common::TinyMT rng{};
        rng.Initialize(static_cast<u32>(GetCurrentPosixTime()));
        rng.GenerateRandomBytes(tag_info.uuid.data(), tag_info.uuid_length);
    }

    return ResultSuccess;
}

Result NfcDevice::ReadMifare(std::span<const MifareReadBlockParameter> parameters,
                             std::span<MifareReadBlockData> read_block_data) const {
    if (device_state != DeviceState::TagFound && device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    Result result = ResultSuccess;

    TagInfo tag_info{};
    result = GetTagInfo(tag_info);

    if (result.IsError()) {
        return result;
    }

    if (tag_info.protocol != NfcProtocol::TypeA || tag_info.tag_type != TagType::Mifare) {
        return ResultInvalidTagType;
    }

    if (parameters.size() == 0) {
        return ResultInvalidArgument;
    }

    Common::Input::MifareRequest request{};
    Common::Input::MifareRequest out_data{};
    const auto unknown = parameters[0].sector_key.unknown;
    for (std::size_t i = 0; i < parameters.size(); i++) {
        if (unknown != parameters[i].sector_key.unknown) {
            return ResultInvalidArgument;
        }
    }

    for (std::size_t i = 0; i < parameters.size(); i++) {
        if (parameters[i].sector_key.command == MifareCmd::None) {
            continue;
        }
        request.data[i].command = static_cast<u8>(parameters[i].sector_key.command);
        request.data[i].sector = parameters[i].sector_number;
        memcpy(request.data[i].key.data(), parameters[i].sector_key.sector_key.data(),
               sizeof(KeyData));
    }

    if (!npad_device->ReadMifareData(request, out_data)) {
        return ResultMifareError288;
    }

    for (std::size_t i = 0; i < read_block_data.size(); i++) {
        if (static_cast<MifareCmd>(out_data.data[i].command) == MifareCmd::None) {
            continue;
        }

        read_block_data[i] = {
            .data = out_data.data[i].data,
            .sector_number = out_data.data[i].sector,
        };
    }

    return ResultSuccess;
}

Result NfcDevice::WriteMifare(std::span<const MifareWriteBlockParameter> parameters) {
    Result result = ResultSuccess;

    TagInfo tag_info{};
    result = GetTagInfo(tag_info);

    if (result.IsError()) {
        return result;
    }

    if (tag_info.protocol != NfcProtocol::TypeA || tag_info.tag_type != TagType::Mifare) {
        return ResultInvalidTagType;
    }

    if (parameters.size() == 0) {
        return ResultInvalidArgument;
    }

    const auto unknown = parameters[0].sector_key.unknown;
    for (std::size_t i = 0; i < parameters.size(); i++) {
        if (unknown != parameters[i].sector_key.unknown) {
            return ResultInvalidArgument;
        }
    }

    Common::Input::MifareRequest request{};
    for (std::size_t i = 0; i < parameters.size(); i++) {
        if (parameters[i].sector_key.command == MifareCmd::None) {
            continue;
        }
        request.data[i].command = static_cast<u8>(parameters[i].sector_key.command);
        request.data[i].sector = parameters[i].sector_number;
        memcpy(request.data[i].key.data(), parameters[i].sector_key.sector_key.data(),
               sizeof(KeyData));
        memcpy(request.data[i].data.data(), parameters[i].data.data(), sizeof(KeyData));
    }

    if (!npad_device->WriteMifareData(request)) {
        return ResultMifareError288;
    }

    return result;
}

Result NfcDevice::SendCommandByPassThrough(const s64& timeout, std::span<const u8> command_data,
                                           std::span<u8> out_data) {
    // Not implemented
    return ResultSuccess;
}

Result NfcDevice::Mount(NFP::ModelType model_type, NFP::MountTarget mount_target_) {
    bool is_corrupted = false;

    if (model_type != NFP::ModelType::Amiibo) {
        return ResultInvalidArgument;
    }

    if (device_state != DeviceState::TagFound) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        return ResultWrongDeviceState;
    }

    if (!LoadAmiiboData()) {
        LOG_ERROR(Service_NFP, "Not an amiibo");
        return ResultInvalidTagType;
    }

    if (!NFP::AmiiboCrypto::IsAmiiboValid(encrypted_tag_data)) {
        LOG_ERROR(Service_NFP, "Not an amiibo");
        return ResultInvalidTagType;
    }

    // The loaded amiibo is not encrypted
    if (is_plain_amiibo) {
        std::vector<u8> data(sizeof(NFP::NTAG215File));
        memcpy(data.data(), &tag_data, sizeof(tag_data));
    }

    if (!is_plain_amiibo && !NFP::AmiiboCrypto::DecodeAmiibo(encrypted_tag_data, tag_data)) {
        LOG_ERROR(Service_NFP, "Can't decode amiibo");
        is_corrupted = true;
    }

    if (tag_data.settings.settings.amiibo_initialized && !tag_data.owner_mii.IsValid()) {
        LOG_ERROR(Service_NFP, "Invalid mii data");
        is_corrupted = true;
    }

    device_state = DeviceState::TagMounted;
    mount_target = mount_target_;

    const bool create_backup =
        mount_target == NFP::MountTarget::All || mount_target == NFP::MountTarget::Ram ||
        (mount_target == NFP::MountTarget::Rom && HasBackup(encrypted_tag_data.uuid).IsError());
    if (!is_corrupted && create_backup) {
        std::vector<u8> data(sizeof(NFP::EncryptedNTAG215File));
        memcpy(data.data(), &encrypted_tag_data, sizeof(encrypted_tag_data));
        WriteBackupData(encrypted_tag_data.uuid, data);
    }

    if (is_corrupted && mount_target != NFP::MountTarget::Rom) {
        bool has_backup = HasBackup(encrypted_tag_data.uuid).IsSuccess();
        return has_backup ? ResultCorruptedDataWithBackup : ResultCorruptedData;
    }

    return ResultSuccess;
}

Result NfcDevice::Unmount() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    // Save data before unloading the amiibo
    if (is_data_moddified) {
        Flush();
    }

    device_state = DeviceState::TagFound;
    mount_target = NFP::MountTarget::None;
    is_app_area_open = false;

    return ResultSuccess;
}

Result NfcDevice::Flush() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    auto& settings = tag_data.settings;

    const auto& current_date = GetAmiiboDate(GetCurrentPosixTime());
    if (settings.write_date.raw_date != current_date.raw_date) {
        settings.write_date = current_date;
        UpdateSettingsCrc();
    }

    tag_data.write_counter++;

    const auto result = FlushWithBreak(NFP::BreakType::Normal);

    is_data_moddified = false;

    return result;
}

Result NfcDevice::FlushDebug() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    tag_data.write_counter++;

    const auto result = FlushWithBreak(NFP::BreakType::Normal);

    is_data_moddified = false;

    return result;
}

Result NfcDevice::FlushWithBreak(NFP::BreakType break_type) {
    if (break_type != NFP::BreakType::Normal) {
        LOG_ERROR(Service_NFC, "Break type not implemented {}", break_type);
        return ResultWrongDeviceState;
    }

    if (is_write_protected) {
        LOG_ERROR(Service_NFP, "No keys available skipping write request");
        return ResultSuccess;
    }

    std::vector<u8> data(sizeof(NFP::EncryptedNTAG215File));
    if (is_plain_amiibo) {
        memcpy(data.data(), &tag_data, sizeof(tag_data));
        WriteBackupData(tag_data.uid, data);
    } else {
        if (!NFP::AmiiboCrypto::EncodeAmiibo(tag_data, encrypted_tag_data)) {
            LOG_ERROR(Service_NFP, "Failed to encode data");
            return ResultWriteAmiiboFailed;
        }

        memcpy(data.data(), &encrypted_tag_data, sizeof(encrypted_tag_data));
        WriteBackupData(encrypted_tag_data.uuid, data);
    }

    if (!npad_device->WriteNfc(data)) {
        LOG_ERROR(Service_NFP, "Error writing to file");
        return ResultWriteAmiiboFailed;
    }

    return ResultSuccess;
}

Result NfcDevice::Restore() {
    if (device_state != DeviceState::TagFound) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    NFC::TagInfo tag_info{};
    std::array<u8, sizeof(NFP::EncryptedNTAG215File)> data{};
    Result result = GetTagInfo(tag_info);

    if (result.IsError()) {
        return result;
    }

    result = ReadBackupData(tag_info.uuid, tag_info.uuid_length, data);

    if (result.IsError()) {
        return result;
    }

    NFP::NTAG215File temporary_tag_data{};
    NFP::EncryptedNTAG215File temporary_encrypted_tag_data{};

    // Fallback for encrypted amiibos without keys
    if (is_write_protected) {
        return ResultWriteAmiiboFailed;
    }

    // Fallback for plain amiibos
    if (is_plain_amiibo) {
        LOG_INFO(Service_NFP, "Restoring backup of plain amiibo");
        memcpy(&temporary_tag_data, data.data(), sizeof(NFP::EncryptedNTAG215File));
        temporary_encrypted_tag_data = NFP::AmiiboCrypto::EncodedDataToNfcData(temporary_tag_data);
    }

    if (!is_plain_amiibo) {
        LOG_INFO(Service_NFP, "Restoring backup of encrypted amiibo");
        temporary_tag_data = {};
        memcpy(&temporary_encrypted_tag_data, data.data(), sizeof(NFP::EncryptedNTAG215File));
    }

    if (!NFP::AmiiboCrypto::IsAmiiboValid(temporary_encrypted_tag_data)) {
        return ResultInvalidTagType;
    }

    if (!is_plain_amiibo) {
        if (!NFP::AmiiboCrypto::DecodeAmiibo(temporary_encrypted_tag_data, temporary_tag_data)) {
            LOG_ERROR(Service_NFP, "Can't decode amiibo");
            return ResultCorruptedData;
        }
    }

    // Restore mii data in case is corrupted by previous instances of sudachi
    if (tag_data.settings.settings.amiibo_initialized && !tag_data.owner_mii.IsValid()) {
        LOG_ERROR(Service_NFP, "Regenerating mii data");
        Mii::StoreData new_mii{};
        new_mii.BuildRandom(Mii::Age::All, Mii::Gender::All, Mii::Race::All);
        new_mii.SetNickname({u's', u'u', u'd', u'a', u'\0'});

        tag_data.owner_mii.BuildFromStoreData(new_mii);
        tag_data.mii_extension.SetFromStoreData(new_mii);
    }

    // Overwrite tag contents with backup and mount the tag
    tag_data = temporary_tag_data;
    encrypted_tag_data = temporary_encrypted_tag_data;
    device_state = DeviceState::TagMounted;
    mount_target = NFP::MountTarget::All;
    is_data_moddified = true;

    return ResultSuccess;
}

Result NfcDevice::GetCommonInfo(NFP::CommonInfo& common_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    const auto& settings = tag_data.settings;

    // TODO: Validate this data
    common_info = {
        .last_write_date = settings.write_date.GetWriteDate(),
        .write_counter = tag_data.application_write_counter,
        .version = tag_data.amiibo_version,
        .application_area_size = sizeof(NFP::ApplicationArea),
    };
    return ResultSuccess;
}

Result NfcDevice::GetModelInfo(NFP::ModelInfo& model_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    const auto& model_info_data = encrypted_tag_data.user_memory.model_info;

    model_info = {
        .character_id = model_info_data.character_id,
        .character_variant = model_info_data.character_variant,
        .amiibo_type = model_info_data.amiibo_type,
        .model_number = model_info_data.model_number,
        .series = model_info_data.series,
    };
    return ResultSuccess;
}

Result NfcDevice::GetRegisterInfo(NFP::RegisterInfo& register_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.amiibo_initialized == 0) {
        return ResultRegistrationIsNotInitialized;
    }

    Mii::CharInfo char_info{};
    Mii::StoreData store_data{};
    tag_data.owner_mii.BuildToStoreData(store_data);
    char_info.SetFromStoreData(store_data);

    const auto& settings = tag_data.settings;

    // TODO: Validate this data
    register_info = {
        .mii_char_info = char_info,
        .creation_date = settings.init_date.GetWriteDate(),
        .amiibo_name = GetAmiiboName(settings),
        .font_region = settings.settings.font_region,
    };

    return ResultSuccess;
}

Result NfcDevice::GetRegisterInfoPrivate(NFP::RegisterInfoPrivate& register_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.amiibo_initialized == 0) {
        return ResultRegistrationIsNotInitialized;
    }

    Mii::StoreData store_data{};
    const auto& settings = tag_data.settings;
    tag_data.owner_mii.BuildToStoreData(store_data);

    // TODO: Validate and complete this data
    register_info = {
        .mii_store_data = store_data,
        .creation_date = settings.init_date.GetWriteDate(),
        .amiibo_name = GetAmiiboName(settings),
        .font_region = settings.settings.font_region,
    };

    return ResultSuccess;
}

Result NfcDevice::GetAdminInfo(NFP::AdminInfo& admin_info) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    u8 flags = static_cast<u8>(tag_data.settings.settings.raw >> 0x4);
    if (tag_data.settings.settings.amiibo_initialized == 0) {
        flags = flags & 0xfe;
    }

    u64 application_id = 0;
    u32 application_area_id = 0;
    NFP::AppAreaVersion app_area_version = NFP::AppAreaVersion::NotSet;
    if (tag_data.settings.settings.appdata_initialized != 0) {
        application_id = tag_data.application_id;
        app_area_version = static_cast<NFP::AppAreaVersion>(
            application_id >> NFP::application_id_version_offset & 0xf);

        // Restore application id to original value
        if (application_id >> 0x38 != 0) {
            const u8 application_byte = tag_data.application_id_byte & 0xf;
            application_id =
                RemoveVersionByte(application_id) |
                (static_cast<u64>(application_byte) << NFP::application_id_version_offset);
        }

        application_area_id = tag_data.application_area_id;
    }

    // TODO: Validate this data
    admin_info = {
        .application_id = application_id,
        .application_area_id = application_area_id,
        .crc_change_counter = tag_data.settings.crc_counter,
        .flags = flags,
        .tag_type = PackedTagType::Type2,
        .app_area_version = app_area_version,
    };

    return ResultSuccess;
}

Result NfcDevice::DeleteRegisterInfo() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.amiibo_initialized == 0) {
        return ResultRegistrationIsNotInitialized;
    }

    Common::TinyMT rng{};
    rng.Initialize(static_cast<u32>(GetCurrentPosixTime()));
    rng.GenerateRandomBytes(&tag_data.owner_mii, sizeof(tag_data.owner_mii));
    rng.GenerateRandomBytes(&tag_data.settings.amiibo_name, sizeof(tag_data.settings.amiibo_name));
    rng.GenerateRandomBytes(&tag_data.unknown, sizeof(u8));
    rng.GenerateRandomBytes(&tag_data.unknown2[0], sizeof(u32));
    rng.GenerateRandomBytes(&tag_data.unknown2[1], sizeof(u32));
    rng.GenerateRandomBytes(&tag_data.register_info_crc, sizeof(u32));
    rng.GenerateRandomBytes(&tag_data.settings.init_date, sizeof(u32));
    tag_data.settings.settings.font_region.Assign(0);
    tag_data.settings.settings.amiibo_initialized.Assign(0);

    return Flush();
}

Result NfcDevice::SetRegisterInfoPrivate(const NFP::RegisterInfoPrivate& register_info) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    auto& settings = tag_data.settings;

    if (tag_data.settings.settings.amiibo_initialized == 0) {
        settings.init_date = GetAmiiboDate(GetCurrentPosixTime());
        settings.write_date.raw_date = 0;
    }

    SetAmiiboName(settings, register_info.amiibo_name);
    tag_data.owner_mii.BuildFromStoreData(register_info.mii_store_data);
    tag_data.mii_extension.SetFromStoreData(register_info.mii_store_data);
    tag_data.unknown = 0;
    tag_data.unknown2 = {};
    settings.country_code_id = 0;
    settings.settings.font_region.Assign(0);
    settings.settings.amiibo_initialized.Assign(1);

    UpdateRegisterInfoCrc();

    return Flush();
}

Result NfcDevice::Format() {
    Result result = ResultSuccess;

    if (device_state == DeviceState::TagFound) {
        result = Mount(NFP::ModelType::Amiibo, NFP::MountTarget::All);
    }

    // We are formatting all data. Corruption is not an issue.
    if (result.IsError() &&
        (result != ResultCorruptedData && result != ResultCorruptedDataWithBackup)) {
        return result;
    }

    DeleteApplicationArea();
    DeleteRegisterInfo();

    return Flush();
}

Result NfcDevice::OpenApplicationArea(u32 access_id) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.appdata_initialized.Value() == 0) {
        LOG_WARNING(Service_NFP, "Application area is not initialized");
        return ResultApplicationAreaIsNotInitialized;
    }

    if (tag_data.application_area_id != access_id) {
        LOG_WARNING(Service_NFP, "Wrong application area id");
        return ResultWrongApplicationAreaId;
    }

    is_app_area_open = true;

    return ResultSuccess;
}

Result NfcDevice::GetApplicationAreaId(u32& application_area_id) const {
    application_area_id = {};

    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.appdata_initialized.Value() == 0) {
        LOG_WARNING(Service_NFP, "Application area is not initialized");
        return ResultApplicationAreaIsNotInitialized;
    }

    application_area_id = tag_data.application_area_id;

    return ResultSuccess;
}

Result NfcDevice::GetApplicationArea(std::span<u8> data) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (!is_app_area_open) {
        LOG_ERROR(Service_NFP, "Application area is not open");
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.appdata_initialized.Value() == 0) {
        LOG_ERROR(Service_NFP, "Application area is not initialized");
        return ResultApplicationAreaIsNotInitialized;
    }

    memcpy(data.data(), tag_data.application_area.data(),
           std::min(data.size(), sizeof(NFP::ApplicationArea)));

    return ResultSuccess;
}

Result NfcDevice::SetApplicationArea(std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (!is_app_area_open) {
        LOG_ERROR(Service_NFP, "Application area is not open");
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.appdata_initialized.Value() == 0) {
        LOG_ERROR(Service_NFP, "Application area is not initialized");
        return ResultApplicationAreaIsNotInitialized;
    }

    if (data.size() > sizeof(NFP::ApplicationArea)) {
        LOG_ERROR(Service_NFP, "Wrong data size {}", data.size());
        return ResultUnknown;
    }

    Common::TinyMT rng{};
    rng.Initialize(static_cast<u32>(GetCurrentPosixTime()));
    std::memcpy(tag_data.application_area.data(), data.data(), data.size());
    // Fill remaining data with random numbers
    rng.GenerateRandomBytes(tag_data.application_area.data() + data.size(),
                            sizeof(NFP::ApplicationArea) - data.size());

    if (tag_data.application_write_counter != NFP::counter_limit) {
        tag_data.application_write_counter++;
    }

    is_data_moddified = true;

    return ResultSuccess;
}

Result NfcDevice::CreateApplicationArea(u32 access_id, std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.appdata_initialized.Value() != 0) {
        LOG_ERROR(Service_NFP, "Application area already exist");
        return ResultApplicationAreaExist;
    }

    return RecreateApplicationArea(access_id, data);
}

Result NfcDevice::RecreateApplicationArea(u32 access_id, std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (is_app_area_open) {
        LOG_ERROR(Service_NFP, "Application area is open");
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (data.size() > sizeof(NFP::ApplicationArea)) {
        LOG_ERROR(Service_NFP, "Wrong data size {}", data.size());
        return ResultWrongApplicationAreaSize;
    }

    Common::TinyMT rng{};
    rng.Initialize(static_cast<u32>(GetCurrentPosixTime()));
    std::memcpy(tag_data.application_area.data(), data.data(), data.size());
    // Fill remaining data with random numbers
    rng.GenerateRandomBytes(tag_data.application_area.data() + data.size(),
                            sizeof(NFP::ApplicationArea) - data.size());

    if (tag_data.application_write_counter != NFP::counter_limit) {
        tag_data.application_write_counter++;
    }

    const u64 application_id = system.GetApplicationProcessProgramID();

    tag_data.application_id_byte =
        static_cast<u8>(application_id >> NFP::application_id_version_offset & 0xf);
    tag_data.application_id =
        RemoveVersionByte(application_id) | (static_cast<u64>(NFP::AppAreaVersion::NintendoSwitch)
                                             << NFP::application_id_version_offset);
    tag_data.settings.settings.appdata_initialized.Assign(1);
    tag_data.application_area_id = access_id;
    tag_data.unknown = {};
    tag_data.unknown2 = {};

    UpdateRegisterInfoCrc();

    return Flush();
}

Result NfcDevice::DeleteApplicationArea() {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFP, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFP, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    if (tag_data.settings.settings.appdata_initialized == 0) {
        return ResultApplicationAreaIsNotInitialized;
    }

    if (tag_data.application_write_counter != NFP::counter_limit) {
        tag_data.application_write_counter++;
    }

    Common::TinyMT rng{};
    rng.Initialize(static_cast<u32>(GetCurrentPosixTime()));
    rng.GenerateRandomBytes(tag_data.application_area.data(), sizeof(NFP::ApplicationArea));
    rng.GenerateRandomBytes(&tag_data.application_id, sizeof(u64));
    rng.GenerateRandomBytes(&tag_data.application_area_id, sizeof(u32));
    rng.GenerateRandomBytes(&tag_data.application_id_byte, sizeof(u8));
    tag_data.settings.settings.appdata_initialized.Assign(0);
    tag_data.unknown = {};
    tag_data.unknown2 = {};
    is_app_area_open = false;

    UpdateRegisterInfoCrc();

    return Flush();
}

Result NfcDevice::ExistsApplicationArea(bool& has_application_area) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    has_application_area = tag_data.settings.settings.appdata_initialized.Value() != 0;

    return ResultSuccess;
}

Result NfcDevice::GetAll(NFP::NfpData& data) const {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    NFP::CommonInfo common_info{};
    const u64 application_id = tag_data.application_id;

    GetCommonInfo(common_info);

    data = {
        .magic = tag_data.constant_value,
        .write_counter = tag_data.write_counter,
        .settings_crc = tag_data.settings.crc,
        .common_info = common_info,
        .mii_char_info = tag_data.owner_mii,
        .mii_store_data_extension = tag_data.mii_extension,
        .creation_date = tag_data.settings.init_date.GetWriteDate(),
        .amiibo_name = tag_data.settings.amiibo_name,
        .amiibo_name_null_terminated = 0,
        .settings = tag_data.settings.settings,
        .unknown1 = tag_data.unknown,
        .register_info_crc = tag_data.register_info_crc,
        .unknown2 = tag_data.unknown2,
        .application_id = application_id,
        .access_id = tag_data.application_area_id,
        .settings_crc_counter = tag_data.settings.crc_counter,
        .font_region = tag_data.settings.settings.font_region,
        .tag_type = PackedTagType::Type2,
        .console_type = static_cast<NFP::AppAreaVersion>(
            application_id >> NFP::application_id_version_offset & 0xf),
        .application_id_byte = tag_data.application_id_byte,
        .application_area = tag_data.application_area,
    };

    return ResultSuccess;
}

Result NfcDevice::SetAll(const NFP::NfpData& data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    tag_data.constant_value = data.magic;
    tag_data.write_counter = data.write_counter;
    tag_data.settings.crc = data.settings_crc;
    tag_data.settings.write_date.SetWriteDate(data.common_info.last_write_date);
    tag_data.write_counter = data.common_info.write_counter;
    tag_data.amiibo_version = data.common_info.version;
    tag_data.owner_mii = data.mii_char_info;
    tag_data.mii_extension = data.mii_store_data_extension;
    tag_data.settings.init_date.SetWriteDate(data.creation_date);
    tag_data.settings.amiibo_name = data.amiibo_name;
    tag_data.settings.settings = data.settings;
    tag_data.unknown = data.unknown1;
    tag_data.register_info_crc = data.register_info_crc;
    tag_data.unknown2 = data.unknown2;
    tag_data.application_id = data.application_id;
    tag_data.application_area_id = data.access_id;
    tag_data.settings.crc_counter = data.settings_crc_counter;
    tag_data.settings.settings.font_region.Assign(data.font_region);
    tag_data.application_id_byte = data.application_id_byte;
    tag_data.application_area = data.application_area;

    return ResultSuccess;
}

Result NfcDevice::BreakTag(NFP::BreakType break_type) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    // TODO: Complete this implementation

    return FlushWithBreak(break_type);
}

Result NfcDevice::HasBackup(const UniqueSerialNumber& uid, std::size_t uuid_size) const {
    ASSERT_MSG(uuid_size < sizeof(UniqueSerialNumber), "Invalid UUID size");
    constexpr auto backup_dir = "backup";
    const auto sudachi_amiibo_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::AmiiboDir);
    const auto file_name =
        fmt::format("{0:02x}.bin", fmt::join(uid.begin(), uid.begin() + uuid_size, ""));

    if (!Common::FS::Exists(sudachi_amiibo_dir / backup_dir / file_name)) {
        return ResultUnableToAccessBackupFile;
    }

    return ResultSuccess;
}

Result NfcDevice::HasBackup(const NFP::TagUuid& tag_uid) const {
    UniqueSerialNumber uuid{};
    memcpy(uuid.data(), &tag_uid, sizeof(NFP::TagUuid));
    return HasBackup(uuid, sizeof(NFP::TagUuid));
}

Result NfcDevice::ReadBackupData(const UniqueSerialNumber& uid, std::size_t uuid_size,
                                 std::span<u8> data) const {
    ASSERT_MSG(uuid_size < sizeof(UniqueSerialNumber), "Invalid UUID size");
    constexpr auto backup_dir = "backup";
    const auto sudachi_amiibo_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::AmiiboDir);
    const auto file_name =
        fmt::format("{0:02x}.bin", fmt::join(uid.begin(), uid.begin() + uuid_size, ""));

    const Common::FS::IOFile keys_file{sudachi_amiibo_dir / backup_dir / file_name,
                                       Common::FS::FileAccessMode::Read,
                                       Common::FS::FileType::BinaryFile};

    if (!keys_file.IsOpen()) {
        LOG_ERROR(Service_NFP, "Failed to open amiibo backup");
        return ResultUnableToAccessBackupFile;
    }

    if (keys_file.Read(data) != data.size()) {
        LOG_ERROR(Service_NFP, "Failed to read amiibo backup");
        return ResultUnableToAccessBackupFile;
    }

    return ResultSuccess;
}

Result NfcDevice::ReadBackupData(const NFP::TagUuid& tag_uid, std::span<u8> data) const {
    UniqueSerialNumber uuid{};
    memcpy(uuid.data(), &tag_uid, sizeof(NFP::TagUuid));
    return ReadBackupData(uuid, sizeof(NFP::TagUuid), data);
}

Result NfcDevice::WriteBackupData(const UniqueSerialNumber& uid, std::size_t uuid_size,
                                  std::span<const u8> data) {
    ASSERT_MSG(uuid_size < sizeof(UniqueSerialNumber), "Invalid UUID size");
    constexpr auto backup_dir = "backup";
    const auto sudachi_amiibo_dir = Common::FS::GetSudachiPath(Common::FS::SudachiPath::AmiiboDir);
    const auto file_name =
        fmt::format("{0:02x}.bin", fmt::join(uid.begin(), uid.begin() + uuid_size, ""));

    if (HasBackup(uid, uuid_size).IsError()) {
        if (!Common::FS::CreateDir(sudachi_amiibo_dir / backup_dir)) {
            return ResultBackupPathAlreadyExist;
        }

        if (!Common::FS::NewFile(sudachi_amiibo_dir / backup_dir / file_name)) {
            return ResultBackupPathAlreadyExist;
        }
    }

    const Common::FS::IOFile keys_file{sudachi_amiibo_dir / backup_dir / file_name,
                                       Common::FS::FileAccessMode::ReadWrite,
                                       Common::FS::FileType::BinaryFile};

    if (!keys_file.IsOpen()) {
        LOG_ERROR(Service_NFP, "Failed to open amiibo backup");
        return ResultUnableToAccessBackupFile;
    }

    if (keys_file.Write(data) != data.size()) {
        LOG_ERROR(Service_NFP, "Failed to write amiibo backup");
        return ResultUnableToAccessBackupFile;
    }

    return ResultSuccess;
}

Result NfcDevice::WriteBackupData(const NFP::TagUuid& tag_uid, std::span<const u8> data) {
    UniqueSerialNumber uuid{};
    memcpy(uuid.data(), &tag_uid, sizeof(NFP::TagUuid));
    return WriteBackupData(uuid, sizeof(NFP::TagUuid), data);
}

Result NfcDevice::WriteNtf(std::span<const u8> data) {
    if (device_state != DeviceState::TagMounted) {
        LOG_ERROR(Service_NFC, "Wrong device state {}", device_state);
        if (device_state == DeviceState::TagRemoved) {
            return ResultTagRemoved;
        }
        return ResultWrongDeviceState;
    }

    if (mount_target == NFP::MountTarget::None || mount_target == NFP::MountTarget::Rom) {
        LOG_ERROR(Service_NFC, "Amiibo is read only", device_state);
        return ResultWrongDeviceState;
    }

    // Not implemented

    return ResultSuccess;
}

NFP::AmiiboName NfcDevice::GetAmiiboName(const NFP::AmiiboSettings& settings) const {
    std::array<char16_t, NFP::amiibo_name_length> settings_amiibo_name{};
    NFP::AmiiboName amiibo_name{};

    // Convert from big endian to little endian
    for (std::size_t i = 0; i < NFP::amiibo_name_length; i++) {
        settings_amiibo_name[i] = static_cast<u16>(settings.amiibo_name[i]);
    }

    // Convert from utf16 to utf8
    const auto amiibo_name_utf8 = Common::UTF16ToUTF8(settings_amiibo_name.data());
    memcpy(amiibo_name.data(), amiibo_name_utf8.data(), amiibo_name_utf8.size());

    return amiibo_name;
}

void NfcDevice::SetAmiiboName(NFP::AmiiboSettings& settings,
                              const NFP::AmiiboName& amiibo_name) const {
    std::array<char16_t, NFP::amiibo_name_length> settings_amiibo_name{};

    // Convert from utf8 to utf16
    const auto amiibo_name_utf16 = Common::UTF8ToUTF16(amiibo_name.data());
    memcpy(settings_amiibo_name.data(), amiibo_name_utf16.data(),
           amiibo_name_utf16.size() * sizeof(char16_t));

    // Convert from little endian to big endian
    for (std::size_t i = 0; i < NFP::amiibo_name_length; i++) {
        settings.amiibo_name[i] = static_cast<u16_be>(settings_amiibo_name[i]);
    }
}

NFP::AmiiboDate NfcDevice::GetAmiiboDate(s64 posix_time) const {
    auto static_service =
        system.ServiceManager().GetService<Service::Glue::Time::StaticService>("time:u", true);

    std::shared_ptr<Service::Glue::Time::TimeZoneService> timezone_service{};
    static_service->GetTimeZoneService(&timezone_service);

    Service::PSC::Time::CalendarTime calendar_time{};
    Service::PSC::Time::CalendarAdditionalInfo additional_info{};

    NFP::AmiiboDate amiibo_date{};

    amiibo_date.SetYear(2000);
    amiibo_date.SetMonth(1);
    amiibo_date.SetDay(1);

    if (timezone_service->ToCalendarTimeWithMyRule(&calendar_time, &additional_info, posix_time) ==
        ResultSuccess) {
        amiibo_date.SetYear(calendar_time.year);
        amiibo_date.SetMonth(calendar_time.month);
        amiibo_date.SetDay(calendar_time.day);
    }

    return amiibo_date;
}

s64 NfcDevice::GetCurrentPosixTime() const {
    auto static_service =
        system.ServiceManager().GetService<Service::Glue::Time::StaticService>("time:u", true);

    std::shared_ptr<Service::PSC::Time::SteadyClock> steady_clock{};
    static_service->GetStandardSteadyClock(&steady_clock);

    Service::PSC::Time::SteadyClockTimePoint time_point{};
    R_ASSERT(steady_clock->GetCurrentTimePoint(&time_point));
    return time_point.time_point;
}

u64 NfcDevice::RemoveVersionByte(u64 application_id) const {
    return application_id & ~(0xfULL << NFP::application_id_version_offset);
}

void NfcDevice::UpdateSettingsCrc() {
    auto& settings = tag_data.settings;

    if (settings.crc_counter != NFP::counter_limit) {
        settings.crc_counter++;
    }

    // TODO: this reads data from a global, find what it is
    std::array<u8, 8> unknown_input{};
    boost::crc_32_type crc;
    crc.process_bytes(&unknown_input, sizeof(unknown_input));
    settings.crc = crc.checksum();
}

void NfcDevice::UpdateRegisterInfoCrc() {
#pragma pack(push, 1)
    struct CrcData {
        Mii::Ver3StoreData mii;
        u8 application_id_byte;
        u8 unknown;
        Mii::NfpStoreDataExtension mii_extension;
        std::array<u32, 0x5> unknown2;
    };
    static_assert(sizeof(CrcData) == 0x7e, "CrcData is an invalid size");
#pragma pack(pop)

    const CrcData crc_data{
        .mii = tag_data.owner_mii,
        .application_id_byte = tag_data.application_id_byte,
        .unknown = tag_data.unknown,
        .mii_extension = tag_data.mii_extension,
        .unknown2 = tag_data.unknown2,
    };

    boost::crc_32_type crc;
    crc.process_bytes(&crc_data, sizeof(CrcData));
    tag_data.register_info_crc = crc.checksum();
}

void NfcDevice::BuildAmiiboWithoutKeys(NFP::NTAG215File& stubbed_tag_data,
                                       const NFP::EncryptedNTAG215File& encrypted_file) const {
    Service::Mii::StoreData store_data{};
    auto& settings = stubbed_tag_data.settings;

    stubbed_tag_data = NFP::AmiiboCrypto::NfcDataToEncodedData(encrypted_file);

    // Common info
    stubbed_tag_data.write_counter = 0;
    stubbed_tag_data.amiibo_version = 0;
    settings.write_date = GetAmiiboDate(GetCurrentPosixTime());

    // Register info
    SetAmiiboName(settings, {'s', 'u', 'd', 'a', 'A', 'm', 'i', 'i', 'b', 'o'});
    settings.settings.font_region.Assign(0);
    settings.init_date = GetAmiiboDate(GetCurrentPosixTime());
    store_data.BuildBase(Mii::Gender::Male);
    stubbed_tag_data.owner_mii.BuildFromStoreData(store_data);

    // Admin info
    settings.settings.amiibo_initialized.Assign(1);
    settings.settings.appdata_initialized.Assign(0);
}

u64 NfcDevice::GetHandle() const {
    // Generate a handle based of the npad id
    return static_cast<u64>(npad_id);
}

DeviceState NfcDevice::GetCurrentState() const {
    return device_state;
}

Result NfcDevice::GetNpadId(Core::HID::NpadIdType& out_npad_id) const {
    // TODO: This should get the npad id from nn::hid::system::GetXcdHandleForNpadWithNfc
    out_npad_id = npad_id;
    return ResultSuccess;
}

} // namespace Service::NFC
