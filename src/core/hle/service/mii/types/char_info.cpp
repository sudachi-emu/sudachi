// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {

void CharInfo::SetFromStoreData(const StoreData& store_data) {
    name = store_data.GetNickname();
    null_terminator = '\0';
    create_id = store_data.GetCreateId();
    font_region = store_data.GetFontRegion();
    favorite_color = store_data.GetFavoriteColor();
    gender = store_data.GetGender();
    height = store_data.GetHeight();
    build = store_data.GetBuild();
    type = store_data.GetType();
    region_move = store_data.GetRegionMove();
    faceline_type = store_data.GetFacelineType();
    faceline_color = store_data.GetFacelineColor();
    faceline_wrinkle = store_data.GetFacelineWrinkle();
    faceline_make = store_data.GetFacelineMake();
    hair_type = store_data.GetHairType();
    hair_color = store_data.GetHairColor();
    hair_flip = store_data.GetHairFlip();
    eye_type = store_data.GetEyeType();
    eye_color = store_data.GetEyeColor();
    eye_scale = store_data.GetEyeScale();
    eye_aspect = store_data.GetEyeAspect();
    eye_rotate = store_data.GetEyeRotate();
    eye_x = store_data.GetEyeX();
    eye_y = store_data.GetEyeY();
    eyebrow_type = store_data.GetEyebrowType();
    eyebrow_color = store_data.GetEyebrowColor();
    eyebrow_scale = store_data.GetEyebrowScale();
    eyebrow_aspect = store_data.GetEyebrowAspect();
    eyebrow_rotate = store_data.GetEyebrowRotate();
    eyebrow_x = store_data.GetEyebrowX();
    eyebrow_y = store_data.GetEyebrowY() + 3;
    nose_type = store_data.GetNoseType();
    nose_scale = store_data.GetNoseScale();
    nose_y = store_data.GetNoseY();
    mouth_type = store_data.GetMouthType();
    mouth_color = store_data.GetMouthColor();
    mouth_scale = store_data.GetMouthScale();
    mouth_aspect = store_data.GetMouthAspect();
    mouth_y = store_data.GetMouthY();
    beard_color = store_data.GetBeardColor();
    beard_type = store_data.GetBeardType();
    mustache_type = store_data.GetMustacheType();
    mustache_scale = store_data.GetMustacheScale();
    mustache_y = store_data.GetMustacheY();
    glass_type = store_data.GetGlassType();
    glass_color = store_data.GetGlassColor();
    glass_scale = store_data.GetGlassScale();
    glass_y = store_data.GetGlassY();
    mole_type = store_data.GetMoleType();
    mole_scale = store_data.GetMoleScale();
    mole_x = store_data.GetMoleX();
    mole_y = store_data.GetMoleY();
    padding = '\0';
}

ValidationResult CharInfo::Verify() const {
    if (!create_id.IsValid()) {
        return ValidationResult::InvalidCreateId;
    }
    if (!name.IsValid()) {
        return ValidationResult::InvalidName;
    }
    if (font_region > FontRegion::Max) {
        return ValidationResult::InvalidFont;
    }
    if (favorite_color > FavoriteColor::Max) {
        return ValidationResult::InvalidColor;
    }
    if (gender > Gender::Max) {
        return ValidationResult::InvalidGender;
    }
    if (height > MaxHeight) {
        return ValidationResult::InvalidHeight;
    }
    if (build > MaxBuild) {
        return ValidationResult::InvalidBuild;
    }
    if (type > MaxType) {
        return ValidationResult::InvalidType;
    }
    if (region_move > MaxRegionMove) {
        return ValidationResult::InvalidRegionMove;
    }
    if (faceline_type > FacelineType::Max) {
        return ValidationResult::InvalidFacelineType;
    }
    if (faceline_color > FacelineColor::Max) {
        return ValidationResult::InvalidFacelineColor;
    }
    if (faceline_wrinkle > FacelineWrinkle::Max) {
        return ValidationResult::InvalidFacelineWrinkle;
    }
    if (faceline_make > FacelineMake::Max) {
        return ValidationResult::InvalidFacelineMake;
    }
    if (hair_type > HairType::Max) {
        return ValidationResult::InvalidHairType;
    }
    if (hair_color > CommonColor::Max) {
        return ValidationResult::InvalidHairColor;
    }
    if (hair_flip > HairFlip::Max) {
        return ValidationResult::InvalidHairFlip;
    }
    if (eye_type > EyeType::Max) {
        return ValidationResult::InvalidEyeType;
    }
    if (eye_color > CommonColor::Max) {
        return ValidationResult::InvalidEyeColor;
    }
    if (eye_scale > MaxEyeScale) {
        return ValidationResult::InvalidEyeScale;
    }
    if (eye_aspect > MaxEyeAspect) {
        return ValidationResult::InvalidEyeAspect;
    }
    if (eye_rotate > MaxEyeX) {
        return ValidationResult::InvalidEyeRotate;
    }
    if (eye_x > MaxEyeX) {
        return ValidationResult::InvalidEyeX;
    }
    if (eye_y > MaxEyeY) {
        return ValidationResult::InvalidEyeY;
    }
    if (eyebrow_type > EyebrowType::Max) {
        return ValidationResult::InvalidEyebrowType;
    }
    if (eyebrow_color > CommonColor::Max) {
        return ValidationResult::InvalidEyebrowColor;
    }
    if (eyebrow_scale > MaxEyebrowScale) {
        return ValidationResult::InvalidEyebrowScale;
    }
    if (eyebrow_aspect > MaxEyebrowAspect) {
        return ValidationResult::InvalidEyebrowAspect;
    }
    if (eyebrow_rotate > MaxEyebrowRotate) {
        return ValidationResult::InvalidEyebrowRotate;
    }
    if (eyebrow_x > MaxEyebrowX) {
        return ValidationResult::InvalidEyebrowX;
    }
    if (eyebrow_y - 3 > MaxEyebrowY) {
        return ValidationResult::InvalidEyebrowY;
    }
    if (nose_type > NoseType::Max) {
        return ValidationResult::InvalidNoseType;
    }
    if (nose_scale > MaxNoseScale) {
        return ValidationResult::InvalidNoseScale;
    }
    if (nose_y > MaxNoseY) {
        return ValidationResult::InvalidNoseY;
    }
    if (mouth_type > MouthType::Max) {
        return ValidationResult::InvalidMouthType;
    }
    if (mouth_color > CommonColor::Max) {
        return ValidationResult::InvalidMouthColor;
    }
    if (mouth_scale > MaxMouthScale) {
        return ValidationResult::InvalidMouthScale;
    }
    if (mouth_aspect > MaxMoutAspect) {
        return ValidationResult::InvalidMouthAspect;
    }
    if (mouth_y > MaxMouthY) {
        return ValidationResult::InvalidMoleY;
    }
    if (beard_color > CommonColor::Max) {
        return ValidationResult::InvalidBeardColor;
    }
    if (beard_type > BeardType::Max) {
        return ValidationResult::InvalidBeardType;
    }
    if (mustache_type > MustacheType::Max) {
        return ValidationResult::InvalidMustacheType;
    }
    if (mustache_scale > MaxMustacheScale) {
        return ValidationResult::InvalidMustacheScale;
    }
    if (mustache_y > MaxMustacheY) {
        return ValidationResult::InvalidMustacheY;
    }
    if (glass_type > GlassType::Max) {
        return ValidationResult::InvalidGlassType;
    }
    if (glass_color > CommonColor::Max) {
        return ValidationResult::InvalidGlassColor;
    }
    if (glass_scale > MaxGlassScale) {
        return ValidationResult::InvalidGlassScale;
    }
    if (glass_y > MaxGlassY) {
        return ValidationResult::InvalidGlassY;
    }
    if (mole_type > MoleType::Max) {
        return ValidationResult::InvalidMoleType;
    }
    if (mole_scale > MaxMoleScale) {
        return ValidationResult::InvalidMoleScale;
    }
    if (mole_x > MaxMoleX) {
        return ValidationResult::InvalidMoleX;
    }
    if (mole_y > MaxMoleY) {
        return ValidationResult::InvalidMoleY;
    }
    return ValidationResult::NoErrors;
}

Common::UUID CharInfo::GetCreateId() const {
    return create_id;
}

Nickname CharInfo::GetNickname() const {
    return name;
}

FontRegion CharInfo::GetFontRegion() const {
    return font_region;
}

FavoriteColor CharInfo::GetFavoriteColor() const {
    return favorite_color;
}

Gender CharInfo::GetGender() const {
    return gender;
}

u8 CharInfo::GetHeight() const {
    return height;
}

u8 CharInfo::GetBuild() const {
    return build;
}

u8 CharInfo::GetType() const {
    return type;
}

u8 CharInfo::GetRegionMove() const {
    return region_move;
}

FacelineType CharInfo::GetFacelineType() const {
    return faceline_type;
}

FacelineColor CharInfo::GetFacelineColor() const {
    return faceline_color;
}

FacelineWrinkle CharInfo::GetFacelineWrinkle() const {
    return faceline_wrinkle;
}

FacelineMake CharInfo::GetFacelineMake() const {
    return faceline_make;
}

HairType CharInfo::GetHairType() const {
    return hair_type;
}

CommonColor CharInfo::GetHairColor() const {
    return hair_color;
}

HairFlip CharInfo::GetHairFlip() const {
    return hair_flip;
}

EyeType CharInfo::GetEyeType() const {
    return eye_type;
}

CommonColor CharInfo::GetEyeColor() const {
    return eye_color;
}

u8 CharInfo::GetEyeScale() const {
    return eye_scale;
}

u8 CharInfo::GetEyeAspect() const {
    return eye_aspect;
}

u8 CharInfo::GetEyeRotate() const {
    return eye_rotate;
}

u8 CharInfo::GetEyeX() const {
    return eye_x;
}

u8 CharInfo::GetEyeY() const {
    return eye_y;
}

EyebrowType CharInfo::GetEyebrowType() const {
    return eyebrow_type;
}

CommonColor CharInfo::GetEyebrowColor() const {
    return eyebrow_color;
}

u8 CharInfo::GetEyebrowScale() const {
    return eyebrow_scale;
}

u8 CharInfo::GetEyebrowAspect() const {
    return eyebrow_aspect;
}

u8 CharInfo::GetEyebrowRotate() const {
    return eyebrow_rotate;
}

u8 CharInfo::GetEyebrowX() const {
    return eyebrow_x;
}

u8 CharInfo::GetEyebrowY() const {
    return eyebrow_y;
}

NoseType CharInfo::GetNoseType() const {
    return nose_type;
}

u8 CharInfo::GetNoseScale() const {
    return nose_scale;
}

u8 CharInfo::GetNoseY() const {
    return nose_y;
}

MouthType CharInfo::GetMouthType() const {
    return mouth_type;
}

CommonColor CharInfo::GetMouthColor() const {
    return mouth_color;
}

u8 CharInfo::GetMouthScale() const {
    return mouth_scale;
}

u8 CharInfo::GetMouthAspect() const {
    return mouth_aspect;
}

u8 CharInfo::GetMouthY() const {
    return mouth_y;
}

CommonColor CharInfo::GetBeardColor() const {
    return beard_color;
}

BeardType CharInfo::GetBeardType() const {
    return beard_type;
}

MustacheType CharInfo::GetMustacheType() const {
    return mustache_type;
}

u8 CharInfo::GetMustacheScale() const {
    return mustache_scale;
}

u8 CharInfo::GetMustacheY() const {
    return mustache_y;
}

GlassType CharInfo::GetGlassType() const {
    return glass_type;
}

CommonColor CharInfo::GetGlassColor() const {
    return glass_color;
}

u8 CharInfo::GetGlassScale() const {
    return glass_scale;
}

u8 CharInfo::GetGlassY() const {
    return glass_y;
}

MoleType CharInfo::GetMoleType() const {
    return mole_type;
}

u8 CharInfo::GetMoleScale() const {
    return mole_scale;
}

u8 CharInfo::GetMoleX() const {
    return mole_x;
}

u8 CharInfo::GetMoleY() const {
    return mole_y;
}

bool CharInfo::operator==(const CharInfo& info) {
    bool is_identical = info.Verify() == ValidationResult::NoErrors;
    is_identical &= name.data == info.GetNickname().data;
    is_identical &= create_id == info.GetCreateId();
    is_identical &= font_region == info.GetFontRegion();
    is_identical &= favorite_color == info.GetFavoriteColor();
    is_identical &= gender == info.GetGender();
    is_identical &= height == info.GetHeight();
    is_identical &= build == info.GetBuild();
    is_identical &= type == info.GetType();
    is_identical &= region_move == info.GetRegionMove();
    is_identical &= faceline_type == info.GetFacelineType();
    is_identical &= faceline_color == info.GetFacelineColor();
    is_identical &= faceline_wrinkle == info.GetFacelineWrinkle();
    is_identical &= faceline_make == info.GetFacelineMake();
    is_identical &= hair_type == info.GetHairType();
    is_identical &= hair_color == info.GetHairColor();
    is_identical &= hair_flip == info.GetHairFlip();
    is_identical &= eye_type == info.GetEyeType();
    is_identical &= eye_color == info.GetEyeColor();
    is_identical &= eye_scale == info.GetEyeScale();
    is_identical &= eye_aspect == info.GetEyeAspect();
    is_identical &= eye_rotate == info.GetEyeRotate();
    is_identical &= eye_x == info.GetEyeX();
    is_identical &= eye_y == info.GetEyeY();
    is_identical &= eyebrow_type == info.GetEyebrowType();
    is_identical &= eyebrow_color == info.GetEyebrowColor();
    is_identical &= eyebrow_scale == info.GetEyebrowScale();
    is_identical &= eyebrow_aspect == info.GetEyebrowAspect();
    is_identical &= eyebrow_rotate == info.GetEyebrowRotate();
    is_identical &= eyebrow_x == info.GetEyebrowX();
    is_identical &= eyebrow_y == info.GetEyebrowY();
    is_identical &= nose_type == info.GetNoseType();
    is_identical &= nose_scale == info.GetNoseScale();
    is_identical &= nose_y == info.GetNoseY();
    is_identical &= mouth_type == info.GetMouthType();
    is_identical &= mouth_color == info.GetMouthColor();
    is_identical &= mouth_scale == info.GetMouthScale();
    is_identical &= mouth_aspect == info.GetMouthAspect();
    is_identical &= mouth_y == info.GetMouthY();
    is_identical &= beard_color == info.GetBeardColor();
    is_identical &= beard_type == info.GetBeardType();
    is_identical &= mustache_type == info.GetMustacheType();
    is_identical &= mustache_scale == info.GetMustacheScale();
    is_identical &= mustache_y == info.GetMustacheY();
    is_identical &= glass_type == info.GetGlassType();
    is_identical &= glass_color == info.GetGlassColor();
    is_identical &= glass_scale == info.GetGlassScale();
    is_identical &= glass_y == info.GetGlassY();
    is_identical &= mole_type == info.GetMoleType();
    is_identical &= mole_scale == info.GetMoleScale();
    is_identical &= mole_x == info.GetMoleX();
    is_identical &= mole_y == info.GetMoleY();
    return is_identical;
}

} // namespace Service::Mii
