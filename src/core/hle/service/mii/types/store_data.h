// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/mii/mii_types.h"
#include "core/hle/service/mii/types/core_data.h"

namespace Service::Mii {

class StoreData {
public:
    void BuildDefault(u32 mii_index);
    void BuildBase(Gender gender);
    void BuildRandom(Age age, Gender gender, Race race);
    void BuildWithCharInfo(const CharInfo& char_info);
    void BuildWithCoreData(const CoreData& in_core_data);
    Result Restore();

    ValidationResult IsValid() const;

    bool IsSpecial() const;

    void SetFontRegion(FontRegion value);
    void SetFavoriteColor(FavoriteColor value);
    void SetGender(Gender value);
    void SetHeight(u8 value);
    void SetBuild(u8 value);
    void SetType(u8 value);
    void SetRegionMove(u8 value);
    void SetFacelineType(FacelineType value);
    void SetFacelineColor(FacelineColor value);
    void SetFacelineWrinkle(FacelineWrinkle value);
    void SetFacelineMake(FacelineMake value);
    void SetHairType(HairType value);
    void SetHairColor(CommonColor value);
    void SetHairFlip(HairFlip value);
    void SetEyeType(EyeType value);
    void SetEyeColor(CommonColor value);
    void SetEyeScale(u8 value);
    void SetEyeAspect(u8 value);
    void SetEyeRotate(u8 value);
    void SetEyeX(u8 value);
    void SetEyeY(u8 value);
    void SetEyebrowType(EyebrowType value);
    void SetEyebrowColor(CommonColor value);
    void SetEyebrowScale(u8 value);
    void SetEyebrowAspect(u8 value);
    void SetEyebrowRotate(u8 value);
    void SetEyebrowX(u8 value);
    void SetEyebrowY(u8 value);
    void SetNoseType(NoseType value);
    void SetNoseScale(u8 value);
    void SetNoseY(u8 value);
    void SetMouthType(MouthType value);
    void SetMouthColor(CommonColor value);
    void SetMouthScale(u8 value);
    void SetMouthAspect(u8 value);
    void SetMouthY(u8 value);
    void SetBeardColor(CommonColor value);
    void SetBeardType(BeardType value);
    void SetMustacheType(MustacheType value);
    void SetMustacheScale(u8 value);
    void SetMustacheY(u8 value);
    void SetGlassType(GlassType value);
    void SetGlassColor(CommonColor value);
    void SetGlassScale(u8 value);
    void SetGlassY(u8 value);
    void SetMoleType(MoleType value);
    void SetMoleScale(u8 value);
    void SetMoleX(u8 value);
    void SetMoleY(u8 value);
    void SetNickname(Nickname nickname);
    void SetInvalidName();
    void SetChecksum();
    void SetDataChecksum();
    void SetDeviceChecksum();

    Common::UUID GetCreateId() const;
    FontRegion GetFontRegion() const;
    FavoriteColor GetFavoriteColor() const;
    Gender GetGender() const;
    u8 GetHeight() const;
    u8 GetBuild() const;
    u8 GetType() const;
    u8 GetRegionMove() const;
    FacelineType GetFacelineType() const;
    FacelineColor GetFacelineColor() const;
    FacelineWrinkle GetFacelineWrinkle() const;
    FacelineMake GetFacelineMake() const;
    HairType GetHairType() const;
    CommonColor GetHairColor() const;
    HairFlip GetHairFlip() const;
    EyeType GetEyeType() const;
    CommonColor GetEyeColor() const;
    u8 GetEyeScale() const;
    u8 GetEyeAspect() const;
    u8 GetEyeRotate() const;
    u8 GetEyeX() const;
    u8 GetEyeY() const;
    EyebrowType GetEyebrowType() const;
    CommonColor GetEyebrowColor() const;
    u8 GetEyebrowScale() const;
    u8 GetEyebrowAspect() const;
    u8 GetEyebrowRotate() const;
    u8 GetEyebrowX() const;
    u8 GetEyebrowY() const;
    NoseType GetNoseType() const;
    u8 GetNoseScale() const;
    u8 GetNoseY() const;
    MouthType GetMouthType() const;
    CommonColor GetMouthColor() const;
    u8 GetMouthScale() const;
    u8 GetMouthAspect() const;
    u8 GetMouthY() const;
    CommonColor GetBeardColor() const;
    BeardType GetBeardType() const;
    MustacheType GetMustacheType() const;
    u8 GetMustacheScale() const;
    u8 GetMustacheY() const;
    GlassType GetGlassType() const;
    CommonColor GetGlassColor() const;
    u8 GetGlassScale() const;
    u8 GetGlassY() const;
    MoleType GetMoleType() const;
    u8 GetMoleScale() const;
    u8 GetMoleX() const;
    u8 GetMoleY() const;
    Nickname GetNickname() const;

    bool operator==(const StoreData& data);

private:
    CoreData core_data{};
    Common::UUID create_id{};
    u16 data_crc{};
    u16 device_crc{};
};
static_assert(sizeof(StoreData) == 0x44, "StoreData has incorrect size.");
static_assert(std::is_trivially_copyable_v<StoreData>,
              "StoreData type must be trivially copyable.");

struct StoreDataElement {
    StoreData store_data{};
    Source source{};
};
static_assert(sizeof(StoreDataElement) == 0x48, "StoreDataElement has incorrect size.");

}; // namespace Service::Mii
