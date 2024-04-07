// SPDX-FileCopyrightText: Copyright 2018 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// NOTE TO FUTURE MAINTAINERS:
// When a new version of switch cryptography is released,
// hash the new keyblob source and master key and add the hashes to
// the arrays below.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <mbedtls/sha256.h>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/hex_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/crypto/key_manager.h"
#include "core/crypto/partition_data_manager.h"
#include "core/crypto/xts_encryption_layer.h"
#include "core/file_sys/kernel_executable.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_offset.h"
#include "core/file_sys/vfs/vfs_vector.h"
#include "core/loader/loader.h"

using Common::AsArray;

namespace Core::Crypto {

struct Package2Header {
    std::array<u8, 0x100> signature;
    Key128 header_ctr;
    std::array<Key128, 4> section_ctr;
    u32_le magic;
    u32_le base_offset;
    INSERT_PADDING_BYTES(4);
    u8 version_max;
    u8 version_min;
    INSERT_PADDING_BYTES(2);
    std::array<u32_le, 4> section_size;
    std::array<u32_le, 4> section_offset;
    std::array<SHA256Hash, 4> section_hash;
};
static_assert(sizeof(Package2Header) == 0x200, "Package2Header has incorrect size.");

// clang-format off
constexpr std::array source_hashes{
    AsArray("B24BD293259DBC7AC5D63F88E60C59792498E6FC5443402C7FFE87EE8B61A3F0"), // keyblob_mac_key_source
    AsArray("7944862A3A5C31C6720595EFD302245ABD1B54CCDCF33000557681E65C5664A4"), // master_key_source
    AsArray("21E2DF100FC9E094DB51B47B9B1D6E94ED379DB8B547955BEF8FE08D8DD35603"), // package2_key_source
    AsArray("FC02B9D37B42D7A1452E71444F1F700311D1132E301A83B16062E72A78175085"), // aes_kek_generation_source
    AsArray("FBD10056999EDC7ACDB96098E47E2C3606230270D23281E671F0F389FC5BC585"), // aes_key_generation_source
    AsArray("C48B619827986C7F4E3081D59DB2B460C84312650E9A8E6B458E53E8CBCA4E87"), // titlekek_source
    AsArray("04AD66143C726B2A139FB6B21128B46F56C553B2B3887110304298D8D0092D9E"), // key_area_key_application_source
    AsArray("FD434000C8FF2B26F8E9A9D2D2C12F6BE5773CBB9DC86300E1BD99F8EA33A417"), // key_area_key_ocean_source
    AsArray("1F17B1FD51AD1C2379B58F152CA4912EC2106441E51722F38700D5937A1162F7"), // key_area_key_system_source
    AsArray("6B2ED877C2C52334AC51E59ABFA7EC457F4A7D01E46291E9F2EAA45F011D24B7"), // sd_card_kek_source
    AsArray("D482743563D3EA5DCDC3B74E97C9AC8A342164FA041A1DC80F17F6D31E4BC01C"), // sd_card_save_key_source
    AsArray("2E751CECF7D93A2B957BD5FFCB082FD038CC2853219DD3092C6DAB9838F5A7CC"), // sd_card_nca_key_source
    AsArray("1888CAED5551B3EDE01499E87CE0D86827F80820EFB275921055AA4E2ABDFFC2"), // header_kek_source
    AsArray("8F783E46852DF6BE0BA4E19273C4ADBAEE16380043E1B8C418C4089A8BD64AA6"), // header_key_source
    AsArray("D1757E52F1AE55FA882EC690BC6F954AC46A83DC22F277F8806BD55577C6EED7"), // rsa_kek_seed3
    AsArray("FC02B9D37B42D7A1452E71444F1F700311D1132E301A83B16062E72A78175085"), // rsa_kek_mask0
};
// clang-format on

// clang-format off
constexpr std::array keyblob_source_hashes{
    AsArray("8A06FE274AC491436791FDB388BCDD3AB9943BD4DEF8094418CDAC150FD73786"), // keyblob_key_source_00
    AsArray("2D5CAEB2521FEF70B47E17D6D0F11F8CE2C1E442A979AD8035832C4E9FBCCC4B"), // keyblob_key_source_01
    AsArray("61C5005E713BAE780641683AF43E5F5C0E03671117F702F401282847D2FC6064"), // keyblob_key_source_02
    AsArray("8E9795928E1C4428E1B78F0BE724D7294D6934689C11B190943923B9D5B85903"), // keyblob_key_source_03
    AsArray("95FA33AF95AFF9D9B61D164655B32710ED8D615D46C7D6CC3CC70481B686B402"), // keyblob_key_source_04
    AsArray("3F5BE7B3C8B1ABD8C10B4B703D44766BA08730562C172A4FE0D6B866B3E2DB3E"), // keyblob_key_source_05
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_06
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_07

    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_08
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_09
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_0A
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_0B
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_0C
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_0D
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_0E
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_0F

    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_10
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_11
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_12
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_13
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_14
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_15
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_16
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_17

    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_18
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_19
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_1A
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_1B
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_1C
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_1D
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_1E
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // keyblob_key_source_1F
};
// clang-format on

// clang-format off
constexpr std::array master_key_hashes{
    AsArray("0EE359BE3C864BB0782E1D70A718A0342C551EED28C369754F9C4F691BECF7CA"), // master_key_00
    AsArray("4FE707B7E4ABDAF727C894AAF13B1351BFE2AC90D875F73B2E20FA94B9CC661E"), // master_key_01
    AsArray("79277C0237A2252EC3DFAC1F7C359C2B3D121E9DB15BB9AB4C2B4408D2F3AE09"), // master_key_02
    AsArray("4F36C565D13325F65EE134073C6A578FFCB0008E02D69400836844EAB7432754"), // master_key_03
    AsArray("75FF1D95D26113550EE6FCC20ACB58E97EDEB3A2FF52543ED5AEC63BDCC3DA50"), // master_key_04
    AsArray("EBE2BCD6704673EC0F88A187BB2AD9F1CC82B718C389425941BDC194DC46B0DD"), // master_key_05
    AsArray("9497E6779F5D840F2BBA1DE4E95BA1D6F21EFC94717D5AE5CA37D7EC5BD37A19"), // master_key_06
    AsArray("4EC96B8CB01B8DCE382149443430B2B6EBCB2983348AFA04A25E53609DABEDF6"), // master_key_07

    AsArray("2998E2E23609BC2675FF062A2D64AF5B1B78DFF463B24119D64A1B64F01B2D51"), // master_key_08
    AsArray("9D486A98067C44B37CF173D3BF577891EB6081FF6B4A166347D9DBBF7025076B"), // master_key_09
    AsArray("4EC5A237A75A083A9C5F6CF615601522A7F822D06BD4BA32612C9CEBBB29BD45"), // master_key_0A
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_0B
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_0C
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_0D
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_0E
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_0F

    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_10
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_11
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_12
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_13
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_14
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_15
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_16
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_17

    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_18
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_19
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_1A
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_1B
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_1C
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_1D
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_1E
    AsArray("0000000000000000000000000000000000000000000000000000000000000000"), // master_key_1F
};
// clang-format on

static constexpr u8 CalculateMaxKeyblobSourceHash() {
    const auto is_zero = [](const auto& data) {
        return std::all_of(data.begin(), data.end(), [](const auto v) { return v == 0; });
    };

    for (s8 i = 0x1F; i >= 0; --i) {
        if (!is_zero(keyblob_source_hashes[i])) {
            return static_cast<u8>(i + 1);
        }
    }

    return 0;
}

const u8 PartitionDataManager::MAX_KEYBLOB_SOURCE_HASH = CalculateMaxKeyblobSourceHash();

template <size_t key_size = 0x10>
std::array<u8, key_size> FindKeyFromHex(const std::vector<u8>& binary,
                                        const std::array<u8, 0x20>& hash) {
    if (binary.size() < key_size)
        return {};

    std::array<u8, 0x20> temp{};
    for (size_t i = 0; i < binary.size() - key_size; ++i) {
        mbedtls_sha256_ret(binary.data() + i, key_size, temp.data(), 0);

        if (temp != hash)
            continue;

        std::array<u8, key_size> out{};
        std::memcpy(out.data(), binary.data() + i, key_size);
        return out;
    }

    return {};
}

std::array<u8, 16> FindKeyFromHex16(const std::vector<u8>& binary, std::array<u8, 32> hash) {
    return FindKeyFromHex<0x10>(binary, hash);
}

static std::array<Key128, 0x20> FindEncryptedMasterKeyFromHex(const std::vector<u8>& binary,
                                                              const Key128& key) {
    if (binary.size() < 0x10)
        return {};

    SHA256Hash temp{};
    Key128 dec_temp{};
    std::array<Key128, 0x20> out{};
    AESCipher<Key128> cipher(key, Mode::ECB);
    for (size_t i = 0; i < binary.size() - 0x10; ++i) {
        cipher.Transcode(binary.data() + i, dec_temp.size(), dec_temp.data(), Op::Decrypt);
        mbedtls_sha256_ret(dec_temp.data(), dec_temp.size(), temp.data(), 0);

        for (size_t k = 0; k < out.size(); ++k) {
            if (temp == master_key_hashes[k]) {
                out[k] = dec_temp;
                break;
            }
        }
    }

    return out;
}

static FileSys::VirtualFile FindFileInDirWithNames(const FileSys::VirtualDir& dir,
                                                   const std::string& name) {
    const auto upper = Common::ToUpper(name);

    for (const auto& fname : {name, name + ".bin", upper, upper + ".BIN"}) {
        if (dir->GetFile(fname) != nullptr) {
            return dir->GetFile(fname);
        }
    }

    return nullptr;
}

PartitionDataManager::PartitionDataManager(const FileSys::VirtualDir& sysdata_dir)
    : boot0(FindFileInDirWithNames(sysdata_dir, "BOOT0")),
      fuses(FindFileInDirWithNames(sysdata_dir, "fuses")),
      kfuses(FindFileInDirWithNames(sysdata_dir, "kfuses")),
      package2({
          FindFileInDirWithNames(sysdata_dir, "BCPKG2-1-Normal-Main"),
          FindFileInDirWithNames(sysdata_dir, "BCPKG2-2-Normal-Sub"),
          FindFileInDirWithNames(sysdata_dir, "BCPKG2-3-SafeMode-Main"),
          FindFileInDirWithNames(sysdata_dir, "BCPKG2-4-SafeMode-Sub"),
          FindFileInDirWithNames(sysdata_dir, "BCPKG2-5-Repair-Main"),
          FindFileInDirWithNames(sysdata_dir, "BCPKG2-6-Repair-Sub"),
      }),
      prodinfo(FindFileInDirWithNames(sysdata_dir, "PRODINFO")),
      secure_monitor(FindFileInDirWithNames(sysdata_dir, "secmon")),
      package1_decrypted(FindFileInDirWithNames(sysdata_dir, "pkg1_decr")),
      secure_monitor_bytes(secure_monitor == nullptr ? std::vector<u8>{}
                                                     : secure_monitor->ReadAllBytes()),
      package1_decrypted_bytes(package1_decrypted == nullptr ? std::vector<u8>{}
                                                             : package1_decrypted->ReadAllBytes()) {
}

PartitionDataManager::~PartitionDataManager() = default;

bool PartitionDataManager::HasBoot0() const {
    return boot0 != nullptr;
}

FileSys::VirtualFile PartitionDataManager::GetBoot0Raw() const {
    return boot0;
}

PartitionDataManager::EncryptedKeyBlob PartitionDataManager::GetEncryptedKeyblob(
    std::size_t index) const {
    if (HasBoot0() && index < NUM_ENCRYPTED_KEYBLOBS)
        return GetEncryptedKeyblobs()[index];
    return {};
}

PartitionDataManager::EncryptedKeyBlobs PartitionDataManager::GetEncryptedKeyblobs() const {
    if (!HasBoot0())
        return {};

    EncryptedKeyBlobs out{};
    for (size_t i = 0; i < out.size(); ++i)
        boot0->Read(out[i].data(), out[i].size(), 0x180000 + i * 0x200);
    return out;
}

std::vector<u8> PartitionDataManager::GetSecureMonitor() const {
    return secure_monitor_bytes;
}

std::array<u8, 16> PartitionDataManager::GetPackage2KeySource() const {
    return FindKeyFromHex(secure_monitor_bytes, source_hashes[2]);
}

std::array<u8, 16> PartitionDataManager::GetAESKekGenerationSource() const {
    return FindKeyFromHex(secure_monitor_bytes, source_hashes[3]);
}

std::array<u8, 16> PartitionDataManager::GetTitlekekSource() const {
    return FindKeyFromHex(secure_monitor_bytes, source_hashes[5]);
}

std::array<std::array<u8, 16>, 32> PartitionDataManager::GetTZMasterKeys(
    std::array<u8, 0x10> master_key) const {
    return FindEncryptedMasterKeyFromHex(secure_monitor_bytes, master_key);
}

std::array<u8, 16> PartitionDataManager::GetRSAKekSeed3() const {
    return FindKeyFromHex(secure_monitor_bytes, source_hashes[14]);
}

std::array<u8, 16> PartitionDataManager::GetRSAKekMask0() const {
    return FindKeyFromHex(secure_monitor_bytes, source_hashes[15]);
}

std::vector<u8> PartitionDataManager::GetPackage1Decrypted() const {
    return package1_decrypted_bytes;
}

std::array<u8, 16> PartitionDataManager::GetMasterKeySource() const {
    return FindKeyFromHex(package1_decrypted_bytes, source_hashes[1]);
}

std::array<u8, 16> PartitionDataManager::GetKeyblobMACKeySource() const {
    return FindKeyFromHex(package1_decrypted_bytes, source_hashes[0]);
}

std::array<u8, 16> PartitionDataManager::GetKeyblobKeySource(std::size_t revision) const {
    if (keyblob_source_hashes[revision] == SHA256Hash{}) {
        LOG_WARNING(Crypto,
                    "No keyblob source hash for crypto revision {:02X}! Cannot derive keys...",
                    revision);
    }
    return FindKeyFromHex(package1_decrypted_bytes, keyblob_source_hashes[revision]);
}

bool PartitionDataManager::HasFuses() const {
    return fuses != nullptr;
}

FileSys::VirtualFile PartitionDataManager::GetFusesRaw() const {
    return fuses;
}

std::array<u8, 16> PartitionDataManager::GetSecureBootKey() const {
    if (!HasFuses())
        return {};
    Key128 out{};
    fuses->Read(out.data(), out.size(), 0xA4);
    return out;
}

bool PartitionDataManager::HasKFuses() const {
    return kfuses != nullptr;
}

FileSys::VirtualFile PartitionDataManager::GetKFusesRaw() const {
    return kfuses;
}

bool PartitionDataManager::HasPackage2(Package2Type type) const {
    return package2.at(static_cast<size_t>(type)) != nullptr;
}

FileSys::VirtualFile PartitionDataManager::GetPackage2Raw(Package2Type type) const {
    return package2.at(static_cast<size_t>(type));
}

static bool AttemptDecrypt(const std::array<u8, 16>& key, Package2Header& header) {
    Package2Header temp = header;
    AESCipher<Key128> cipher(key, Mode::CTR);
    cipher.SetIV(header.header_ctr);
    cipher.Transcode(&temp.header_ctr, sizeof(Package2Header) - sizeof(Package2Header::signature),
                     &temp.header_ctr, Op::Decrypt);
    if (temp.magic == Common::MakeMagic('P', 'K', '2', '1')) {
        header = temp;
        return true;
    }

    return false;
}

void PartitionDataManager::DecryptPackage2(const std::array<Key128, 0x20>& package2_keys,
                                           Package2Type type) {
    FileSys::VirtualFile file = std::make_shared<FileSys::OffsetVfsFile>(
        package2[static_cast<size_t>(type)],
        package2[static_cast<size_t>(type)]->GetSize() - 0x4000, 0x4000);

    Package2Header header{};
    if (file->ReadObject(&header) != sizeof(Package2Header))
        return;

    std::size_t revision = 0xFF;
    if (header.magic != Common::MakeMagic('P', 'K', '2', '1')) {
        for (std::size_t i = 0; i < package2_keys.size(); ++i) {
            if (AttemptDecrypt(package2_keys[i], header)) {
                revision = i;
            }
        }
    }

    if (header.magic != Common::MakeMagic('P', 'K', '2', '1'))
        return;

    const auto a = std::make_shared<FileSys::OffsetVfsFile>(
        file, header.section_size[1], header.section_size[0] + sizeof(Package2Header));

    auto c = a->ReadAllBytes();

    AESCipher<Key128> cipher(package2_keys[revision], Mode::CTR);
    cipher.SetIV(header.section_ctr[1]);
    cipher.Transcode(c.data(), c.size(), c.data(), Op::Decrypt);

    const auto ini_file = std::make_shared<FileSys::VectorVfsFile>(c);
    const FileSys::INI ini{ini_file};
    if (ini.GetStatus() != Loader::ResultStatus::Success)
        return;

    for (const auto& kip : ini.GetKIPs()) {
        if (kip.GetStatus() != Loader::ResultStatus::Success)
            return;

        if (kip.GetName() != "FS" && kip.GetName() != "spl") {
            continue;
        }

        const auto& text = kip.GetTextSection();
        const auto& rodata = kip.GetRODataSection();
        const auto& data = kip.GetDataSection();

        std::vector<u8> out;
        out.reserve(text.size() + rodata.size() + data.size());
        out.insert(out.end(), text.begin(), text.end());
        out.insert(out.end(), rodata.begin(), rodata.end());
        out.insert(out.end(), data.begin(), data.end());

        if (kip.GetName() == "FS")
            package2_fs[static_cast<size_t>(type)] = std::move(out);
        else if (kip.GetName() == "spl")
            package2_spl[static_cast<size_t>(type)] = std::move(out);
    }
}

const std::vector<u8>& PartitionDataManager::GetPackage2FSDecompressed(Package2Type type) const {
    return package2_fs.at(static_cast<size_t>(type));
}

std::array<u8, 16> PartitionDataManager::GetKeyAreaKeyApplicationSource(Package2Type type) const {
    return FindKeyFromHex(package2_fs.at(static_cast<size_t>(type)), source_hashes[6]);
}

std::array<u8, 16> PartitionDataManager::GetKeyAreaKeyOceanSource(Package2Type type) const {
    return FindKeyFromHex(package2_fs.at(static_cast<size_t>(type)), source_hashes[7]);
}

std::array<u8, 16> PartitionDataManager::GetKeyAreaKeySystemSource(Package2Type type) const {
    return FindKeyFromHex(package2_fs.at(static_cast<size_t>(type)), source_hashes[8]);
}

std::array<u8, 16> PartitionDataManager::GetSDKekSource(Package2Type type) const {
    return FindKeyFromHex(package2_fs.at(static_cast<size_t>(type)), source_hashes[9]);
}

std::array<u8, 32> PartitionDataManager::GetSDSaveKeySource(Package2Type type) const {
    return FindKeyFromHex<0x20>(package2_fs.at(static_cast<size_t>(type)), source_hashes[10]);
}

std::array<u8, 32> PartitionDataManager::GetSDNCAKeySource(Package2Type type) const {
    return FindKeyFromHex<0x20>(package2_fs.at(static_cast<size_t>(type)), source_hashes[11]);
}

std::array<u8, 16> PartitionDataManager::GetHeaderKekSource(Package2Type type) const {
    return FindKeyFromHex(package2_fs.at(static_cast<size_t>(type)), source_hashes[12]);
}

std::array<u8, 32> PartitionDataManager::GetHeaderKeySource(Package2Type type) const {
    return FindKeyFromHex<0x20>(package2_fs.at(static_cast<size_t>(type)), source_hashes[13]);
}

const std::vector<u8>& PartitionDataManager::GetPackage2SPLDecompressed(Package2Type type) const {
    return package2_spl.at(static_cast<size_t>(type));
}

std::array<u8, 16> PartitionDataManager::GetAESKeyGenerationSource(Package2Type type) const {
    return FindKeyFromHex(package2_spl.at(static_cast<size_t>(type)), source_hashes[4]);
}

bool PartitionDataManager::HasProdInfo() const {
    return prodinfo != nullptr;
}

FileSys::VirtualFile PartitionDataManager::GetProdInfoRaw() const {
    return prodinfo;
}

void PartitionDataManager::DecryptProdInfo(std::array<u8, 0x20> bis_key) {
    if (prodinfo == nullptr)
        return;

    prodinfo_decrypted = std::make_shared<XTSEncryptionLayer>(prodinfo, bis_key);
}

FileSys::VirtualFile PartitionDataManager::GetDecryptedProdInfo() const {
    return prodinfo_decrypted;
}

std::array<u8, 576> PartitionDataManager::GetETicketExtendedKek() const {
    std::array<u8, 0x240> out{};
    if (prodinfo_decrypted != nullptr)
        prodinfo_decrypted->Read(out.data(), out.size(), 0x3890);
    return out;
}
} // namespace Core::Crypto
