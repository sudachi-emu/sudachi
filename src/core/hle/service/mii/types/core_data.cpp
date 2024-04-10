// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/core_data.h"
#include "core/hle/service/mii/types/raw_data.h"

namespace Service::Mii {

void CoreData::SetDefault() {
    data = {};
    name = GetDefaultNickname();
}

void CoreData::BuildRandom(Age age, Gender gender, Race race) {
    if (gender == Gender::All) {
        gender = MiiUtil::GetRandomValue(Gender::Max);
    }

    if (age == Age::All) {
        const auto random{MiiUtil::GetRandomValue<int>(10)};
        if (random >= 8) {
            age = Age::Old;
        } else if (random >= 4) {
            age = Age::Normal;
        } else {
            age = Age::Young;
        }
    }

    if (race == Race::All) {
        const auto random{MiiUtil::GetRandomValue<int>(10)};
        if (random >= 8) {
            race = Race::Black;
        } else if (random >= 4) {
            race = Race::White;
        } else {
            race = Race::Asian;
        }
    }

    SetDefault();
    SetGender(gender);
    SetFavoriteColor(MiiUtil::GetRandomValue(FavoriteColor::Max));
    SetRegionMove(0);
    SetFontRegion(FontRegion::Standard);
    SetType(0);
    SetHeight(64);
    SetBuild(64);

    u32 axis_y{};
    if (gender == Gender::Female && age == Age::Young) {
        axis_y = MiiUtil::GetRandomValue<u32>(3);
    }

    const std::size_t index{3 * static_cast<std::size_t>(age) +
                            9 * static_cast<std::size_t>(gender) + static_cast<std::size_t>(race)};

    const auto& faceline_type_info{RawData::RandomMiiFaceline.at(index)};
    const auto& faceline_color_info{RawData::RandomMiiFacelineColor.at(
        3 * static_cast<std::size_t>(gender) + static_cast<std::size_t>(race))};
    const auto& faceline_wrinkle_info{RawData::RandomMiiFacelineWrinkle.at(index)};
    const auto& faceline_makeup_info{RawData::RandomMiiFacelineMakeup.at(index)};
    const auto& hair_type_info{RawData::RandomMiiHairType.at(index)};
    const auto& hair_color_info{RawData::RandomMiiHairColor.at(3 * static_cast<std::size_t>(race) +
                                                               static_cast<std::size_t>(age))};
    const auto& eye_type_info{RawData::RandomMiiEyeType.at(index)};
    const auto& eye_color_info{RawData::RandomMiiEyeColor.at(static_cast<std::size_t>(race))};
    const auto& eyebrow_type_info{RawData::RandomMiiEyebrowType.at(index)};
    const auto& nose_type_info{RawData::RandomMiiNoseType.at(index)};
    const auto& mouth_type_info{RawData::RandomMiiMouthType.at(index)};
    const auto& glasses_type_info{RawData::RandomMiiGlassType.at(static_cast<std::size_t>(age))};

    data.faceline_type.Assign(
        faceline_type_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_type_info.values_count)]);
    data.faceline_color.Assign(
        faceline_color_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_color_info.values_count)]);
    data.faceline_wrinkle.Assign(
        faceline_wrinkle_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_wrinkle_info.values_count)]);
    data.faceline_makeup.Assign(
        faceline_makeup_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_makeup_info.values_count)]);

    data.hair_type.Assign(
        hair_type_info.values[MiiUtil::GetRandomValue<std::size_t>(hair_type_info.values_count)]);
    SetHairColor(RawData::GetHairColorFromVer3(
        hair_color_info
            .values[MiiUtil::GetRandomValue<std::size_t>(hair_color_info.values_count)]));
    SetHairFlip(MiiUtil::GetRandomValue(HairFlip::Max));

    data.eye_type.Assign(
        eye_type_info.values[MiiUtil::GetRandomValue<std::size_t>(eye_type_info.values_count)]);

    const auto eye_rotate_1{gender != Gender::Male ? 4 : 2};
    const auto eye_rotate_2{gender != Gender::Male ? 3 : 4};
    const auto eye_rotate_offset{32 - RawData::EyeRotateLookup[eye_rotate_1] + eye_rotate_2};
    const auto eye_rotate{32 - RawData::EyeRotateLookup[data.eye_type]};

    SetEyeColor(RawData::GetEyeColorFromVer3(
        eye_color_info.values[MiiUtil::GetRandomValue<std::size_t>(eye_color_info.values_count)]));
    SetEyeScale(4);
    SetEyeAspect(3);
    SetEyeRotate(static_cast<u8>(eye_rotate_offset - eye_rotate));
    SetEyeX(2);
    SetEyeY(static_cast<u8>(axis_y + 12));

    data.eyebrow_type.Assign(
        eyebrow_type_info
            .values[MiiUtil::GetRandomValue<std::size_t>(eyebrow_type_info.values_count)]);

    const auto eyebrow_rotate_1{race == Race::Asian ? 6 : 0};
    const auto eyebrow_y{race == Race::Asian ? 6 : 7};
    const auto eyebrow_rotate_offset{32 - RawData::EyebrowRotateLookup[eyebrow_rotate_1] + 6};
    const auto eyebrow_rotate{
        32 - RawData::EyebrowRotateLookup[static_cast<std::size_t>(data.eyebrow_type.Value())]};

    SetEyebrowColor(GetHairColor());
    SetEyebrowScale(4);
    SetEyebrowAspect(3);
    SetEyebrowRotate(static_cast<u8>(eyebrow_rotate_offset - eyebrow_rotate));
    SetEyebrowX(2);
    SetEyebrowY(static_cast<u8>(axis_y + eyebrow_y));

    data.nose_type.Assign(
        nose_type_info.values[MiiUtil::GetRandomValue<std::size_t>(nose_type_info.values_count)]);
    SetNoseScale(gender == Gender::Female ? 3 : 4);
    SetNoseY(static_cast<u8>(axis_y + 9));

    const auto mouth_color{gender == Gender::Female ? MiiUtil::GetRandomValue<int>(4) : 0};

    data.mouth_type.Assign(
        mouth_type_info.values[MiiUtil::GetRandomValue<std::size_t>(mouth_type_info.values_count)]);
    SetMouthColor(RawData::GetMouthColorFromVer3(mouth_color));
    SetMouthScale(4);
    SetMouthAspect(3);
    SetMouthY(static_cast<u8>(axis_y + 13));

    SetBeardColor(GetHairColor());
    SetMustacheScale(4);

    if (gender == Gender::Male && age != Age::Young && MiiUtil::GetRandomValue<int>(10) < 2) {
        const auto mustache_and_beard_flag{MiiUtil::GetRandomValue(BeardAndMustacheFlag::All)};

        auto beard_type{BeardType::None};
        auto mustache_type{MustacheType::None};

        if ((mustache_and_beard_flag & BeardAndMustacheFlag::Beard) ==
            BeardAndMustacheFlag::Beard) {
            beard_type = MiiUtil::GetRandomValue(BeardType::Min, BeardType::Max);
        }

        if ((mustache_and_beard_flag & BeardAndMustacheFlag::Mustache) ==
            BeardAndMustacheFlag::Mustache) {
            mustache_type = MiiUtil::GetRandomValue(MustacheType::Min, MustacheType::Max);
        }

        SetMustacheType(mustache_type);
        SetBeardType(beard_type);
        SetMustacheY(10);
    } else {
        SetMustacheType(MustacheType::None);
        SetBeardType(BeardType::None);
        SetMustacheY(static_cast<u8>(axis_y + 10));
    }

    const auto glasses_type_start{MiiUtil::GetRandomValue<std::size_t>(100)};
    u8 glasses_type{};
    while (glasses_type_start < glasses_type_info.values[glasses_type]) {
        if (++glasses_type >= glasses_type_info.values_count) {
            glasses_type = 0;
            break;
        }
    }

    SetGlassType(static_cast<GlassType>(glasses_type));
    SetGlassColor(RawData::GetGlassColorFromVer3(0));
    SetGlassScale(4);
    SetGlassY(static_cast<u8>(axis_y + 10));

    SetMoleType(MoleType::None);
    SetMoleScale(4);
    SetMoleX(2);
    SetMoleY(20);
}

void CoreData::BuildFromCharInfo(const CharInfo& char_info) {
    name = char_info.GetNickname();
    SetFontRegion(char_info.GetFontRegion());
    SetFavoriteColor(char_info.GetFavoriteColor());
    SetGender(char_info.GetGender());
    SetHeight(char_info.GetHeight());
    SetBuild(char_info.GetBuild());
    SetType(char_info.GetType());
    SetRegionMove(char_info.GetRegionMove());
    SetFacelineType(char_info.GetFacelineType());
    SetFacelineColor(char_info.GetFacelineColor());
    SetFacelineWrinkle(char_info.GetFacelineWrinkle());
    SetFacelineMake(char_info.GetFacelineMake());
    SetHairType(char_info.GetHairType());
    SetHairColor(char_info.GetHairColor());
    SetHairFlip(char_info.GetHairFlip());
    SetEyeType(char_info.GetEyeType());
    SetEyeColor(char_info.GetEyeColor());
    SetEyeScale(char_info.GetEyeScale());
    SetEyeAspect(char_info.GetEyeAspect());
    SetEyeRotate(char_info.GetEyeRotate());
    SetEyeX(char_info.GetEyeX());
    SetEyeY(char_info.GetEyeY());
    SetEyebrowType(char_info.GetEyebrowType());
    SetEyebrowColor(char_info.GetEyebrowColor());
    SetEyebrowScale(char_info.GetEyebrowScale());
    SetEyebrowAspect(char_info.GetEyebrowAspect());
    SetEyebrowRotate(char_info.GetEyebrowRotate());
    SetEyebrowX(char_info.GetEyebrowX());
    SetEyebrowY(char_info.GetEyebrowY() - 3);
    SetNoseType(char_info.GetNoseType());
    SetNoseScale(char_info.GetNoseScale());
    SetNoseY(char_info.GetNoseY());
    SetMouthType(char_info.GetMouthType());
    SetMouthColor(char_info.GetMouthColor());
    SetMouthScale(char_info.GetMouthScale());
    SetMouthAspect(char_info.GetMouthAspect());
    SetMouthY(char_info.GetMouthY());
    SetBeardColor(char_info.GetBeardColor());
    SetBeardType(char_info.GetBeardType());
    SetMustacheType(char_info.GetMustacheType());
    SetMustacheScale(char_info.GetMustacheScale());
    SetMustacheY(char_info.GetMustacheY());
    SetGlassType(char_info.GetGlassType());
    SetGlassColor(char_info.GetGlassColor());
    SetGlassScale(char_info.GetGlassScale());
    SetGlassY(char_info.GetGlassY());
    SetMoleType(char_info.GetMoleType());
    SetMoleScale(char_info.GetMoleScale());
    SetMoleX(char_info.GetMoleX());
    SetMoleY(char_info.GetMoleY());
}

ValidationResult CoreData::IsValid() const {
    if (!name.IsValid()) {
        return ValidationResult::InvalidName;
    }
    if (GetFontRegion() > FontRegion::Max) {
        return ValidationResult::InvalidFont;
    }
    if (GetFavoriteColor() > FavoriteColor::Max) {
        return ValidationResult::InvalidColor;
    }
    if (GetGender() > Gender::Max) {
        return ValidationResult::InvalidGender;
    }
    if (GetHeight() > MaxHeight) {
        return ValidationResult::InvalidHeight;
    }
    if (GetBuild() > MaxBuild) {
        return ValidationResult::InvalidBuild;
    }
    if (GetType() > MaxType) {
        return ValidationResult::InvalidType;
    }
    if (GetRegionMove() > MaxRegionMove) {
        return ValidationResult::InvalidRegionMove;
    }
    if (GetFacelineType() > FacelineType::Max) {
        return ValidationResult::InvalidFacelineType;
    }
    if (GetFacelineColor() > FacelineColor::Max) {
        return ValidationResult::InvalidFacelineColor;
    }
    if (GetFacelineWrinkle() > FacelineWrinkle::Max) {
        return ValidationResult::InvalidFacelineWrinkle;
    }
    if (GetFacelineMake() > FacelineMake::Max) {
        return ValidationResult::InvalidFacelineMake;
    }
    if (GetHairType() > HairType::Max) {
        return ValidationResult::InvalidHairType;
    }
    if (GetHairColor() > CommonColor::Max) {
        return ValidationResult::InvalidHairColor;
    }
    if (GetHairFlip() > HairFlip::Max) {
        return ValidationResult::InvalidHairFlip;
    }
    if (GetEyeType() > EyeType::Max) {
        return ValidationResult::InvalidEyeType;
    }
    if (GetEyeColor() > CommonColor::Max) {
        return ValidationResult::InvalidEyeColor;
    }
    if (GetEyeScale() > MaxEyeScale) {
        return ValidationResult::InvalidEyeScale;
    }
    if (GetEyeAspect() > MaxEyeAspect) {
        return ValidationResult::InvalidEyeAspect;
    }
    if (GetEyeRotate() > MaxEyeRotate) {
        return ValidationResult::InvalidEyeRotate;
    }
    if (GetEyeX() > MaxEyeX) {
        return ValidationResult::InvalidEyeX;
    }
    if (GetEyeY() > MaxEyeY) {
        return ValidationResult::InvalidEyeY;
    }
    if (GetEyebrowType() > EyebrowType::Max) {
        return ValidationResult::InvalidEyebrowType;
    }
    if (GetEyebrowColor() > CommonColor::Max) {
        return ValidationResult::InvalidEyebrowColor;
    }
    if (GetEyebrowScale() > MaxEyebrowScale) {
        return ValidationResult::InvalidEyebrowScale;
    }
    if (GetEyebrowAspect() > MaxEyebrowAspect) {
        return ValidationResult::InvalidEyebrowAspect;
    }
    if (GetEyebrowRotate() > MaxEyebrowRotate) {
        return ValidationResult::InvalidEyebrowRotate;
    }
    if (GetEyebrowX() > MaxEyebrowX) {
        return ValidationResult::InvalidEyebrowX;
    }
    if (GetEyebrowY() > MaxEyebrowY) {
        return ValidationResult::InvalidEyebrowY;
    }
    if (GetNoseType() > NoseType::Max) {
        return ValidationResult::InvalidNoseType;
    }
    if (GetNoseScale() > MaxNoseScale) {
        return ValidationResult::InvalidNoseScale;
    }
    if (GetNoseY() > MaxNoseY) {
        return ValidationResult::InvalidNoseY;
    }
    if (GetMouthType() > MouthType::Max) {
        return ValidationResult::InvalidMouthType;
    }
    if (GetMouthColor() > CommonColor::Max) {
        return ValidationResult::InvalidMouthColor;
    }
    if (GetMouthScale() > MaxMouthScale) {
        return ValidationResult::InvalidMouthScale;
    }
    if (GetMouthAspect() > MaxMoutAspect) {
        return ValidationResult::InvalidMouthAspect;
    }
    if (GetMouthY() > MaxMouthY) {
        return ValidationResult::InvalidMouthY;
    }
    if (GetBeardColor() > CommonColor::Max) {
        return ValidationResult::InvalidBeardColor;
    }
    if (GetBeardType() > BeardType::Max) {
        return ValidationResult::InvalidBeardType;
    }
    if (GetMustacheType() > MustacheType::Max) {
        return ValidationResult::InvalidMustacheType;
    }
    if (GetMustacheScale() > MaxMustacheScale) {
        return ValidationResult::InvalidMustacheScale;
    }
    if (GetMustacheY() > MaxMustacheY) {
        return ValidationResult::InvalidMustacheY;
    }
    if (GetGlassType() > GlassType::Max) {
        return ValidationResult::InvalidGlassType;
    }
    if (GetGlassColor() > CommonColor::Max) {
        return ValidationResult::InvalidGlassColor;
    }
    if (GetGlassScale() > MaxGlassScale) {
        return ValidationResult::InvalidGlassScale;
    }
    if (GetGlassY() > MaxGlassY) {
        return ValidationResult::InvalidGlassY;
    }
    if (GetMoleType() > MoleType::Max) {
        return ValidationResult::InvalidMoleType;
    }
    if (GetMoleScale() > MaxMoleScale) {
        return ValidationResult::InvalidMoleScale;
    }
    if (GetMoleX() > MaxMoleX) {
        return ValidationResult::InvalidMoleX;
    }
    if (GetMoleY() > MaxMoleY) {
        return ValidationResult::InvalidMoleY;
    }
    return ValidationResult::NoErrors;
}

void CoreData::SetFontRegion(FontRegion value) {
    data.font_region.Assign(static_cast<u32>(value));
}

void CoreData::SetFavoriteColor(FavoriteColor value) {
    data.favorite_color.Assign(static_cast<u32>(value));
}

void CoreData::SetGender(Gender value) {
    data.gender.Assign(static_cast<u32>(value));
}

void CoreData::SetHeight(u8 value) {
    data.height.Assign(value);
}

void CoreData::SetBuild(u8 value) {
    data.build.Assign(value);
}

void CoreData::SetType(u8 value) {
    data.type.Assign(value);
}

void CoreData::SetRegionMove(u8 value) {
    data.region_move.Assign(value);
}

void CoreData::SetFacelineType(FacelineType value) {
    data.faceline_type.Assign(static_cast<u32>(value));
}

void CoreData::SetFacelineColor(FacelineColor value) {
    data.faceline_color.Assign(static_cast<u32>(value));
}

void CoreData::SetFacelineWrinkle(FacelineWrinkle value) {
    data.faceline_wrinkle.Assign(static_cast<u32>(value));
}

void CoreData::SetFacelineMake(FacelineMake value) {
    data.faceline_makeup.Assign(static_cast<u32>(value));
}

void CoreData::SetHairType(HairType value) {
    data.hair_type.Assign(static_cast<u32>(value));
}

void CoreData::SetHairColor(CommonColor value) {
    data.hair_color.Assign(static_cast<u32>(value));
}

void CoreData::SetHairFlip(HairFlip value) {
    data.hair_flip.Assign(static_cast<u32>(value));
}

void CoreData::SetEyeType(EyeType value) {
    data.eye_type.Assign(static_cast<u32>(value));
}

void CoreData::SetEyeColor(CommonColor value) {
    data.eye_color.Assign(static_cast<u32>(value));
}

void CoreData::SetEyeScale(u8 value) {
    data.eye_scale.Assign(value);
}

void CoreData::SetEyeAspect(u8 value) {
    data.eye_aspect.Assign(value);
}

void CoreData::SetEyeRotate(u8 value) {
    data.eye_rotate.Assign(value);
}

void CoreData::SetEyeX(u8 value) {
    data.eye_x.Assign(value);
}

void CoreData::SetEyeY(u8 value) {
    data.eye_y.Assign(value);
}

void CoreData::SetEyebrowType(EyebrowType value) {
    data.eyebrow_type.Assign(static_cast<u32>(value));
}

void CoreData::SetEyebrowColor(CommonColor value) {
    data.eyebrow_color.Assign(static_cast<u32>(value));
}

void CoreData::SetEyebrowScale(u8 value) {
    data.eyebrow_scale.Assign(value);
}

void CoreData::SetEyebrowAspect(u8 value) {
    data.eyebrow_aspect.Assign(value);
}

void CoreData::SetEyebrowRotate(u8 value) {
    data.eyebrow_rotate.Assign(value);
}

void CoreData::SetEyebrowX(u8 value) {
    data.eyebrow_x.Assign(value);
}

void CoreData::SetEyebrowY(u8 value) {
    data.eyebrow_y.Assign(value);
}

void CoreData::SetNoseType(NoseType value) {
    data.nose_type.Assign(static_cast<u32>(value));
}

void CoreData::SetNoseScale(u8 value) {
    data.nose_scale.Assign(value);
}

void CoreData::SetNoseY(u8 value) {
    data.nose_y.Assign(value);
}

void CoreData::SetMouthType(MouthType value) {
    data.mouth_type.Assign(static_cast<u32>(value));
}

void CoreData::SetMouthColor(CommonColor value) {
    data.mouth_color.Assign(static_cast<u32>(value));
}

void CoreData::SetMouthScale(u8 value) {
    data.mouth_scale.Assign(value);
}

void CoreData::SetMouthAspect(u8 value) {
    data.mouth_aspect.Assign(value);
}

void CoreData::SetMouthY(u8 value) {
    data.mouth_y.Assign(value);
}

void CoreData::SetBeardColor(CommonColor value) {
    data.beard_color.Assign(static_cast<u32>(value));
}

void CoreData::SetBeardType(BeardType value) {
    data.beard_type.Assign(static_cast<u32>(value));
}

void CoreData::SetMustacheType(MustacheType value) {
    data.mustache_type.Assign(static_cast<u32>(value));
}

void CoreData::SetMustacheScale(u8 value) {
    data.mustache_scale.Assign(value);
}

void CoreData::SetMustacheY(u8 value) {
    data.mustache_y.Assign(value);
}

void CoreData::SetGlassType(GlassType value) {
    data.glasses_type.Assign(static_cast<u32>(value));
}

void CoreData::SetGlassColor(CommonColor value) {
    data.glasses_color.Assign(static_cast<u32>(value));
}

void CoreData::SetGlassScale(u8 value) {
    data.glasses_scale.Assign(value);
}

void CoreData::SetGlassY(u8 value) {
    data.glasses_y.Assign(value);
}

void CoreData::SetMoleType(MoleType value) {
    data.mole_type.Assign(static_cast<u32>(value));
}

void CoreData::SetMoleScale(u8 value) {
    data.mole_scale.Assign(value);
}

void CoreData::SetMoleX(u8 value) {
    data.mole_x.Assign(value);
}

void CoreData::SetMoleY(u8 value) {
    data.mole_y.Assign(value);
}

void CoreData::SetNickname(Nickname nickname) {
    name = nickname;
}

FontRegion CoreData::GetFontRegion() const {
    return static_cast<FontRegion>(data.font_region.Value());
}

FavoriteColor CoreData::GetFavoriteColor() const {
    return static_cast<FavoriteColor>(data.favorite_color.Value());
}

Gender CoreData::GetGender() const {
    return static_cast<Gender>(data.gender.Value());
}

u8 CoreData::GetHeight() const {
    return static_cast<u8>(data.height.Value());
}

u8 CoreData::GetBuild() const {
    return static_cast<u8>(data.build.Value());
}

u8 CoreData::GetType() const {
    return static_cast<u8>(data.type.Value());
}

u8 CoreData::GetRegionMove() const {
    return static_cast<u8>(data.region_move.Value());
}

FacelineType CoreData::GetFacelineType() const {
    return static_cast<FacelineType>(data.faceline_type.Value());
}

FacelineColor CoreData::GetFacelineColor() const {
    return static_cast<FacelineColor>(data.faceline_color.Value());
}

FacelineWrinkle CoreData::GetFacelineWrinkle() const {
    return static_cast<FacelineWrinkle>(data.faceline_wrinkle.Value());
}

FacelineMake CoreData::GetFacelineMake() const {
    return static_cast<FacelineMake>(data.faceline_makeup.Value());
}

HairType CoreData::GetHairType() const {
    return static_cast<HairType>(data.hair_type.Value());
}

CommonColor CoreData::GetHairColor() const {
    return static_cast<CommonColor>(data.hair_color.Value());
}

HairFlip CoreData::GetHairFlip() const {
    return static_cast<HairFlip>(data.hair_flip.Value());
}

EyeType CoreData::GetEyeType() const {
    return static_cast<EyeType>(data.eye_type.Value());
}

CommonColor CoreData::GetEyeColor() const {
    return static_cast<CommonColor>(data.eye_color.Value());
}

u8 CoreData::GetEyeScale() const {
    return static_cast<u8>(data.eye_scale.Value());
}

u8 CoreData::GetEyeAspect() const {
    return static_cast<u8>(data.eye_aspect.Value());
}

u8 CoreData::GetEyeRotate() const {
    return static_cast<u8>(data.eye_rotate.Value());
}

u8 CoreData::GetEyeX() const {
    return static_cast<u8>(data.eye_x.Value());
}

u8 CoreData::GetEyeY() const {
    return static_cast<u8>(data.eye_y.Value());
}

EyebrowType CoreData::GetEyebrowType() const {
    return static_cast<EyebrowType>(data.eyebrow_type.Value());
}

CommonColor CoreData::GetEyebrowColor() const {
    return static_cast<CommonColor>(data.eyebrow_color.Value());
}

u8 CoreData::GetEyebrowScale() const {
    return static_cast<u8>(data.eyebrow_scale.Value());
}

u8 CoreData::GetEyebrowAspect() const {
    return static_cast<u8>(data.eyebrow_aspect.Value());
}

u8 CoreData::GetEyebrowRotate() const {
    return static_cast<u8>(data.eyebrow_rotate.Value());
}

u8 CoreData::GetEyebrowX() const {
    return static_cast<u8>(data.eyebrow_x.Value());
}

u8 CoreData::GetEyebrowY() const {
    return static_cast<u8>(data.eyebrow_y.Value());
}

NoseType CoreData::GetNoseType() const {
    return static_cast<NoseType>(data.nose_type.Value());
}

u8 CoreData::GetNoseScale() const {
    return static_cast<u8>(data.nose_scale.Value());
}

u8 CoreData::GetNoseY() const {
    return static_cast<u8>(data.nose_y.Value());
}

MouthType CoreData::GetMouthType() const {
    return static_cast<MouthType>(data.mouth_type.Value());
}

CommonColor CoreData::GetMouthColor() const {
    return static_cast<CommonColor>(data.mouth_color.Value());
}

u8 CoreData::GetMouthScale() const {
    return static_cast<u8>(data.mouth_scale.Value());
}

u8 CoreData::GetMouthAspect() const {
    return static_cast<u8>(data.mouth_aspect.Value());
}

u8 CoreData::GetMouthY() const {
    return static_cast<u8>(data.mouth_y.Value());
}

CommonColor CoreData::GetBeardColor() const {
    return static_cast<CommonColor>(data.beard_color.Value());
}

BeardType CoreData::GetBeardType() const {
    return static_cast<BeardType>(data.beard_type.Value());
}

MustacheType CoreData::GetMustacheType() const {
    return static_cast<MustacheType>(data.mustache_type.Value());
}

u8 CoreData::GetMustacheScale() const {
    return static_cast<u8>(data.mustache_scale.Value());
}

u8 CoreData::GetMustacheY() const {
    return static_cast<u8>(data.mustache_y.Value());
}

GlassType CoreData::GetGlassType() const {
    return static_cast<GlassType>(data.glasses_type.Value());
}

CommonColor CoreData::GetGlassColor() const {
    return static_cast<CommonColor>(data.glasses_color.Value());
}

u8 CoreData::GetGlassScale() const {
    return static_cast<u8>(data.glasses_scale.Value());
}

u8 CoreData::GetGlassY() const {
    return static_cast<u8>(data.glasses_y.Value());
}

MoleType CoreData::GetMoleType() const {
    return static_cast<MoleType>(data.mole_type.Value());
}

u8 CoreData::GetMoleScale() const {
    return static_cast<u8>(data.mole_scale.Value());
}

u8 CoreData::GetMoleX() const {
    return static_cast<u8>(data.mole_x.Value());
}

u8 CoreData::GetMoleY() const {
    return static_cast<u8>(data.mole_y.Value());
}

Nickname CoreData::GetNickname() const {
    return name;
}

Nickname CoreData::GetDefaultNickname() const {
    return {u'n', u'o', u' ', u'n', u'a', u'm', u'e'};
}

Nickname CoreData::GetInvalidNickname() const {
    return {u'?', u'?', u'?'};
}

} // namespace Service::Mii
