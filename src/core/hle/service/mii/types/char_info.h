// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/mii/mii_types.h"

namespace Service::Mii {
class StoreData;

// This is nn::mii::detail::CharInfoRaw
class CharInfo {
public:
    void SetFromStoreData(const StoreData& store_data_raw);

    ValidationResult Verify() const;

    Common::UUID GetCreateId() const;
    Nickname GetNickname() const;
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

    bool operator==(const CharInfo& info);

private:
    Common::UUID create_id{};
    Nickname name{};
    u16 null_terminator{};
    FontRegion font_region{};
    FavoriteColor favorite_color{};
    Gender gender{};
    u8 height{};
    u8 build{};
    u8 type{};
    u8 region_move{};
    FacelineType faceline_type{};
    FacelineColor faceline_color{};
    FacelineWrinkle faceline_wrinkle{};
    FacelineMake faceline_make{};
    HairType hair_type{};
    CommonColor hair_color{};
    HairFlip hair_flip{};
    EyeType eye_type{};
    CommonColor eye_color{};
    u8 eye_scale{};
    u8 eye_aspect{};
    u8 eye_rotate{};
    u8 eye_x{};
    u8 eye_y{};
    EyebrowType eyebrow_type{};
    CommonColor eyebrow_color{};
    u8 eyebrow_scale{};
    u8 eyebrow_aspect{};
    u8 eyebrow_rotate{};
    u8 eyebrow_x{};
    u8 eyebrow_y{};
    NoseType nose_type{};
    u8 nose_scale{};
    u8 nose_y{};
    MouthType mouth_type{};
    CommonColor mouth_color{};
    u8 mouth_scale{};
    u8 mouth_aspect{};
    u8 mouth_y{};
    CommonColor beard_color{};
    BeardType beard_type{};
    MustacheType mustache_type{};
    u8 mustache_scale{};
    u8 mustache_y{};
    GlassType glass_type{};
    CommonColor glass_color{};
    u8 glass_scale{};
    u8 glass_y{};
    MoleType mole_type{};
    u8 mole_scale{};
    u8 mole_x{};
    u8 mole_y{};
    u8 padding{};
};
static_assert(sizeof(CharInfo) == 0x58, "CharInfo has incorrect size.");
static_assert(std::has_unique_object_representations_v<CharInfo>,
              "All bits of CharInfo must contribute to its value.");

struct CharInfoElement {
    CharInfo char_info{};
    Source source{};
};
static_assert(sizeof(CharInfoElement) == 0x5c, "CharInfoElement has incorrect size.");

}; // namespace Service::Mii
