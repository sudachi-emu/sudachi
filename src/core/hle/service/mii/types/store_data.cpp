// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_result.h"
#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/raw_data.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {

void StoreData::BuildDefault(u32 mii_index) {
    const auto& default_mii = RawData::DefaultMii[mii_index];
    core_data.SetDefault();

    core_data.SetFacelineType(static_cast<FacelineType>(default_mii.face_type));
    core_data.SetFacelineColor(
        RawData::GetFacelineColorFromVer3(static_cast<u8>(default_mii.face_color)));
    core_data.SetFacelineWrinkle(static_cast<FacelineWrinkle>(default_mii.face_wrinkle));
    core_data.SetFacelineMake(static_cast<FacelineMake>(default_mii.face_makeup));

    core_data.SetHairType(static_cast<HairType>(default_mii.hair_type));
    core_data.SetHairColor(RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.hair_color)));
    core_data.SetHairFlip(static_cast<HairFlip>(default_mii.hair_flip));
    core_data.SetEyeType(static_cast<EyeType>(default_mii.eye_type));
    core_data.SetEyeColor(RawData::GetEyeColorFromVer3(static_cast<u8>(default_mii.eye_color)));
    core_data.SetEyeScale(static_cast<u8>(default_mii.eye_scale));
    core_data.SetEyeAspect(static_cast<u8>(default_mii.eye_aspect));
    core_data.SetEyeRotate(static_cast<u8>(default_mii.eye_rotate));
    core_data.SetEyeX(static_cast<u8>(default_mii.eye_x));
    core_data.SetEyeY(static_cast<u8>(default_mii.eye_y));

    core_data.SetEyebrowType(static_cast<EyebrowType>(default_mii.eyebrow_type));
    core_data.SetEyebrowColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.eyebrow_color)));
    core_data.SetEyebrowScale(static_cast<u8>(default_mii.eyebrow_scale));
    core_data.SetEyebrowAspect(static_cast<u8>(default_mii.eyebrow_aspect));
    core_data.SetEyebrowRotate(static_cast<u8>(default_mii.eyebrow_rotate));
    core_data.SetEyebrowX(static_cast<u8>(default_mii.eyebrow_x));
    core_data.SetEyebrowY(static_cast<u8>(default_mii.eyebrow_y - 3));

    core_data.SetNoseType(static_cast<NoseType>(default_mii.nose_type));
    core_data.SetNoseScale(static_cast<u8>(default_mii.nose_scale));
    core_data.SetNoseY(static_cast<u8>(default_mii.nose_y));

    core_data.SetMouthType(static_cast<MouthType>(default_mii.mouth_type));
    core_data.SetMouthColor(
        RawData::GetMouthColorFromVer3(static_cast<u8>(default_mii.mouth_color)));
    core_data.SetMouthScale(static_cast<u8>(default_mii.mouth_scale));
    core_data.SetMouthAspect(static_cast<u8>(default_mii.mouth_aspect));
    core_data.SetMouthY(static_cast<u8>(default_mii.mouth_y));

    core_data.SetMustacheType(static_cast<MustacheType>(default_mii.mustache_type));
    core_data.SetBeardType(static_cast<BeardType>(default_mii.beard_type));
    core_data.SetBeardColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.beard_color)));
    core_data.SetMustacheScale(static_cast<u8>(default_mii.mustache_scale));
    core_data.SetMustacheY(static_cast<u8>(default_mii.mustache_y));

    core_data.SetGlassType(static_cast<GlassType>(default_mii.glasses_type));
    core_data.SetGlassColor(
        RawData::GetGlassColorFromVer3(static_cast<u8>(default_mii.glasses_color)));
    core_data.SetGlassScale(static_cast<u8>(default_mii.glasses_scale));
    core_data.SetGlassY(static_cast<u8>(default_mii.glasses_y));

    core_data.SetMoleType(static_cast<MoleType>(default_mii.mole_type));
    core_data.SetMoleScale(static_cast<u8>(default_mii.mole_scale));
    core_data.SetMoleX(static_cast<u8>(default_mii.mole_x));
    core_data.SetMoleY(static_cast<u8>(default_mii.mole_y));

    core_data.SetHeight(static_cast<u8>(default_mii.height));
    core_data.SetBuild(static_cast<u8>(default_mii.weight));
    core_data.SetGender(static_cast<Gender>(default_mii.gender));
    core_data.SetFavoriteColor(static_cast<FavoriteColor>(default_mii.favorite_color));
    core_data.SetRegionMove(static_cast<u8>(default_mii.region_move));
    core_data.SetFontRegion(static_cast<FontRegion>(default_mii.font_region));
    core_data.SetType(static_cast<u8>(default_mii.type));
    core_data.SetNickname(default_mii.nickname);

    create_id = MiiUtil::MakeCreateId();
    SetChecksum();
}

void StoreData::BuildBase(Gender gender) {
    const auto& default_mii = RawData::BaseMii[gender == Gender::Female ? 1 : 0];
    core_data.SetDefault();

    core_data.SetFacelineType(static_cast<FacelineType>(default_mii.face_type));
    core_data.SetFacelineColor(
        RawData::GetFacelineColorFromVer3(static_cast<u8>(default_mii.face_color)));
    core_data.SetFacelineWrinkle(static_cast<FacelineWrinkle>(default_mii.face_wrinkle));
    core_data.SetFacelineMake(static_cast<FacelineMake>(default_mii.face_makeup));

    core_data.SetHairType(static_cast<HairType>(default_mii.hair_type));
    core_data.SetHairColor(RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.hair_color)));
    core_data.SetHairFlip(static_cast<HairFlip>(default_mii.hair_flip));
    core_data.SetEyeType(static_cast<EyeType>(default_mii.eye_type));
    core_data.SetEyeColor(RawData::GetEyeColorFromVer3(static_cast<u8>(default_mii.eye_color)));
    core_data.SetEyeScale(static_cast<u8>(default_mii.eye_scale));
    core_data.SetEyeAspect(static_cast<u8>(default_mii.eye_aspect));
    core_data.SetEyeRotate(static_cast<u8>(default_mii.eye_rotate));
    core_data.SetEyeX(static_cast<u8>(default_mii.eye_x));
    core_data.SetEyeY(static_cast<u8>(default_mii.eye_y));

    core_data.SetEyebrowType(static_cast<EyebrowType>(default_mii.eyebrow_type));
    core_data.SetEyebrowColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.eyebrow_color)));
    core_data.SetEyebrowScale(static_cast<u8>(default_mii.eyebrow_scale));
    core_data.SetEyebrowAspect(static_cast<u8>(default_mii.eyebrow_aspect));
    core_data.SetEyebrowRotate(static_cast<u8>(default_mii.eyebrow_rotate));
    core_data.SetEyebrowX(static_cast<u8>(default_mii.eyebrow_x));
    core_data.SetEyebrowY(static_cast<u8>(default_mii.eyebrow_y - 3));

    core_data.SetNoseType(static_cast<NoseType>(default_mii.nose_type));
    core_data.SetNoseScale(static_cast<u8>(default_mii.nose_scale));
    core_data.SetNoseY(static_cast<u8>(default_mii.nose_y));

    core_data.SetMouthType(static_cast<MouthType>(default_mii.mouth_type));
    core_data.SetMouthColor(
        RawData::GetMouthColorFromVer3(static_cast<u8>(default_mii.mouth_color)));
    core_data.SetMouthScale(static_cast<u8>(default_mii.mouth_scale));
    core_data.SetMouthAspect(static_cast<u8>(default_mii.mouth_aspect));
    core_data.SetMouthY(static_cast<u8>(default_mii.mouth_y));

    core_data.SetMustacheType(static_cast<MustacheType>(default_mii.mustache_type));
    core_data.SetBeardType(static_cast<BeardType>(default_mii.beard_type));
    core_data.SetBeardColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.beard_color)));
    core_data.SetMustacheScale(static_cast<u8>(default_mii.mustache_scale));
    core_data.SetMustacheY(static_cast<u8>(default_mii.mustache_y));

    core_data.SetGlassType(static_cast<GlassType>(default_mii.glasses_type));
    core_data.SetGlassColor(
        RawData::GetGlassColorFromVer3(static_cast<u8>(default_mii.glasses_color)));
    core_data.SetGlassScale(static_cast<u8>(default_mii.glasses_scale));
    core_data.SetGlassY(static_cast<u8>(default_mii.glasses_y));

    core_data.SetMoleType(static_cast<MoleType>(default_mii.mole_type));
    core_data.SetMoleScale(static_cast<u8>(default_mii.mole_scale));
    core_data.SetMoleX(static_cast<u8>(default_mii.mole_x));
    core_data.SetMoleY(static_cast<u8>(default_mii.mole_y));

    core_data.SetHeight(static_cast<u8>(default_mii.height));
    core_data.SetBuild(static_cast<u8>(default_mii.weight));
    core_data.SetGender(static_cast<Gender>(default_mii.gender));
    core_data.SetFavoriteColor(static_cast<FavoriteColor>(default_mii.favorite_color));
    core_data.SetRegionMove(static_cast<u8>(default_mii.region_move));
    core_data.SetFontRegion(static_cast<FontRegion>(default_mii.font_region));
    core_data.SetType(static_cast<u8>(default_mii.type));
    core_data.SetNickname(default_mii.nickname);

    create_id = MiiUtil::MakeCreateId();
    SetChecksum();
}

void StoreData::BuildRandom(Age age, Gender gender, Race race) {
    core_data.BuildRandom(age, gender, race);
    create_id = MiiUtil::MakeCreateId();
    SetChecksum();
}

void StoreData::BuildWithCharInfo(const CharInfo& char_info) {
    core_data.BuildFromCharInfo(char_info);
    create_id = MiiUtil::MakeCreateId();
    SetChecksum();
}

void StoreData::BuildWithCoreData(const CoreData& in_core_data) {
    core_data = in_core_data;
    create_id = MiiUtil::MakeCreateId();
    SetChecksum();
}

Result StoreData::Restore() {
    // TODO: Implement this
    return ResultNotUpdated;
}

ValidationResult StoreData::IsValid() const {
    if (core_data.IsValid() != ValidationResult::NoErrors) {
        return core_data.IsValid();
    }
    if (data_crc != MiiUtil::CalculateCrc16(&core_data, sizeof(CoreData) + sizeof(Common::UUID))) {
        return ValidationResult::InvalidChecksum;
    }
    const auto device_id = MiiUtil::GetDeviceId();
    if (device_crc != MiiUtil::CalculateDeviceCrc16(device_id, sizeof(StoreData))) {
        return ValidationResult::InvalidChecksum;
    }
    return ValidationResult::NoErrors;
}

bool StoreData::IsSpecial() const {
    return GetType() == 1;
}

void StoreData::SetFontRegion(FontRegion value) {
    core_data.SetFontRegion(value);
}

void StoreData::SetFavoriteColor(FavoriteColor value) {
    core_data.SetFavoriteColor(value);
}

void StoreData::SetGender(Gender value) {
    core_data.SetGender(value);
}

void StoreData::SetHeight(u8 value) {
    core_data.SetHeight(value);
}

void StoreData::SetBuild(u8 value) {
    core_data.SetBuild(value);
}

void StoreData::SetType(u8 value) {
    core_data.SetType(value);
}

void StoreData::SetRegionMove(u8 value) {
    core_data.SetRegionMove(value);
}

void StoreData::SetFacelineType(FacelineType value) {
    core_data.SetFacelineType(value);
}

void StoreData::SetFacelineColor(FacelineColor value) {
    core_data.SetFacelineColor(value);
}

void StoreData::SetFacelineWrinkle(FacelineWrinkle value) {
    core_data.SetFacelineWrinkle(value);
}

void StoreData::SetFacelineMake(FacelineMake value) {
    core_data.SetFacelineMake(value);
}

void StoreData::SetHairType(HairType value) {
    core_data.SetHairType(value);
}

void StoreData::SetHairColor(CommonColor value) {
    core_data.SetHairColor(value);
}

void StoreData::SetHairFlip(HairFlip value) {
    core_data.SetHairFlip(value);
}

void StoreData::SetEyeType(EyeType value) {
    core_data.SetEyeType(value);
}

void StoreData::SetEyeColor(CommonColor value) {
    core_data.SetEyeColor(value);
}

void StoreData::SetEyeScale(u8 value) {
    core_data.SetEyeScale(value);
}

void StoreData::SetEyeAspect(u8 value) {
    core_data.SetEyeAspect(value);
}

void StoreData::SetEyeRotate(u8 value) {
    core_data.SetEyeRotate(value);
}

void StoreData::SetEyeX(u8 value) {
    core_data.SetEyeX(value);
}

void StoreData::SetEyeY(u8 value) {
    core_data.SetEyeY(value);
}

void StoreData::SetEyebrowType(EyebrowType value) {
    core_data.SetEyebrowType(value);
}

void StoreData::SetEyebrowColor(CommonColor value) {
    core_data.SetEyebrowColor(value);
}

void StoreData::SetEyebrowScale(u8 value) {
    core_data.SetEyebrowScale(value);
}

void StoreData::SetEyebrowAspect(u8 value) {
    core_data.SetEyebrowAspect(value);
}

void StoreData::SetEyebrowRotate(u8 value) {
    core_data.SetEyebrowRotate(value);
}

void StoreData::SetEyebrowX(u8 value) {
    core_data.SetEyebrowX(value);
}

void StoreData::SetEyebrowY(u8 value) {
    core_data.SetEyebrowY(value);
}

void StoreData::SetNoseType(NoseType value) {
    core_data.SetNoseType(value);
}

void StoreData::SetNoseScale(u8 value) {
    core_data.SetNoseScale(value);
}

void StoreData::SetNoseY(u8 value) {
    core_data.SetNoseY(value);
}

void StoreData::SetMouthType(MouthType value) {
    core_data.SetMouthType(value);
}

void StoreData::SetMouthColor(CommonColor value) {
    core_data.SetMouthColor(value);
}

void StoreData::SetMouthScale(u8 value) {
    core_data.SetMouthScale(value);
}

void StoreData::SetMouthAspect(u8 value) {
    core_data.SetMouthAspect(value);
}

void StoreData::SetMouthY(u8 value) {
    core_data.SetMouthY(value);
}

void StoreData::SetBeardColor(CommonColor value) {
    core_data.SetBeardColor(value);
}

void StoreData::SetBeardType(BeardType value) {
    core_data.SetBeardType(value);
}

void StoreData::SetMustacheType(MustacheType value) {
    core_data.SetMustacheType(value);
}

void StoreData::SetMustacheScale(u8 value) {
    core_data.SetMustacheScale(value);
}

void StoreData::SetMustacheY(u8 value) {
    core_data.SetMustacheY(value);
}

void StoreData::SetGlassType(GlassType value) {
    core_data.SetGlassType(value);
}

void StoreData::SetGlassColor(CommonColor value) {
    core_data.SetGlassColor(value);
}

void StoreData::SetGlassScale(u8 value) {
    core_data.SetGlassScale(value);
}

void StoreData::SetGlassY(u8 value) {
    core_data.SetGlassY(value);
}

void StoreData::SetMoleType(MoleType value) {
    core_data.SetMoleType(value);
}

void StoreData::SetMoleScale(u8 value) {
    core_data.SetMoleScale(value);
}

void StoreData::SetMoleX(u8 value) {
    core_data.SetMoleX(value);
}

void StoreData::SetMoleY(u8 value) {
    core_data.SetMoleY(value);
}

void StoreData::SetNickname(Nickname value) {
    core_data.SetNickname(value);
}

void StoreData::SetInvalidName() {
    const auto& invalid_name = core_data.GetInvalidNickname();
    core_data.SetNickname(invalid_name);
    SetChecksum();
}

void StoreData::SetChecksum() {
    SetDataChecksum();
    SetDeviceChecksum();
}

void StoreData::SetDataChecksum() {
    data_crc = MiiUtil::CalculateCrc16(&core_data, sizeof(CoreData) + sizeof(Common::UUID));
}

void StoreData::SetDeviceChecksum() {
    const auto device_id = MiiUtil::GetDeviceId();
    device_crc = MiiUtil::CalculateDeviceCrc16(device_id, sizeof(StoreData));
}

Common::UUID StoreData::GetCreateId() const {
    return create_id;
}

FontRegion StoreData::GetFontRegion() const {
    return static_cast<FontRegion>(core_data.GetFontRegion());
}

FavoriteColor StoreData::GetFavoriteColor() const {
    return core_data.GetFavoriteColor();
}

Gender StoreData::GetGender() const {
    return core_data.GetGender();
}

u8 StoreData::GetHeight() const {
    return core_data.GetHeight();
}

u8 StoreData::GetBuild() const {
    return core_data.GetBuild();
}

u8 StoreData::GetType() const {
    return core_data.GetType();
}

u8 StoreData::GetRegionMove() const {
    return core_data.GetRegionMove();
}

FacelineType StoreData::GetFacelineType() const {
    return core_data.GetFacelineType();
}

FacelineColor StoreData::GetFacelineColor() const {
    return core_data.GetFacelineColor();
}

FacelineWrinkle StoreData::GetFacelineWrinkle() const {
    return core_data.GetFacelineWrinkle();
}

FacelineMake StoreData::GetFacelineMake() const {
    return core_data.GetFacelineMake();
}

HairType StoreData::GetHairType() const {
    return core_data.GetHairType();
}

CommonColor StoreData::GetHairColor() const {
    return core_data.GetHairColor();
}

HairFlip StoreData::GetHairFlip() const {
    return core_data.GetHairFlip();
}

EyeType StoreData::GetEyeType() const {
    return core_data.GetEyeType();
}

CommonColor StoreData::GetEyeColor() const {
    return core_data.GetEyeColor();
}

u8 StoreData::GetEyeScale() const {
    return core_data.GetEyeScale();
}

u8 StoreData::GetEyeAspect() const {
    return core_data.GetEyeAspect();
}

u8 StoreData::GetEyeRotate() const {
    return core_data.GetEyeRotate();
}

u8 StoreData::GetEyeX() const {
    return core_data.GetEyeX();
}

u8 StoreData::GetEyeY() const {
    return core_data.GetEyeY();
}

EyebrowType StoreData::GetEyebrowType() const {
    return core_data.GetEyebrowType();
}

CommonColor StoreData::GetEyebrowColor() const {
    return core_data.GetEyebrowColor();
}

u8 StoreData::GetEyebrowScale() const {
    return core_data.GetEyebrowScale();
}

u8 StoreData::GetEyebrowAspect() const {
    return core_data.GetEyebrowAspect();
}

u8 StoreData::GetEyebrowRotate() const {
    return core_data.GetEyebrowRotate();
}

u8 StoreData::GetEyebrowX() const {
    return core_data.GetEyebrowX();
}

u8 StoreData::GetEyebrowY() const {
    return core_data.GetEyebrowY();
}

NoseType StoreData::GetNoseType() const {
    return core_data.GetNoseType();
}

u8 StoreData::GetNoseScale() const {
    return core_data.GetNoseScale();
}

u8 StoreData::GetNoseY() const {
    return core_data.GetNoseY();
}

MouthType StoreData::GetMouthType() const {
    return core_data.GetMouthType();
}

CommonColor StoreData::GetMouthColor() const {
    return core_data.GetMouthColor();
}

u8 StoreData::GetMouthScale() const {
    return core_data.GetMouthScale();
}

u8 StoreData::GetMouthAspect() const {
    return core_data.GetMouthAspect();
}

u8 StoreData::GetMouthY() const {
    return core_data.GetMouthY();
}

CommonColor StoreData::GetBeardColor() const {
    return core_data.GetBeardColor();
}

BeardType StoreData::GetBeardType() const {
    return core_data.GetBeardType();
}

MustacheType StoreData::GetMustacheType() const {
    return core_data.GetMustacheType();
}

u8 StoreData::GetMustacheScale() const {
    return core_data.GetMustacheScale();
}

u8 StoreData::GetMustacheY() const {
    return core_data.GetMustacheY();
}

GlassType StoreData::GetGlassType() const {
    return core_data.GetGlassType();
}

CommonColor StoreData::GetGlassColor() const {
    return core_data.GetGlassColor();
}

u8 StoreData::GetGlassScale() const {
    return core_data.GetGlassScale();
}

u8 StoreData::GetGlassY() const {
    return core_data.GetGlassY();
}

MoleType StoreData::GetMoleType() const {
    return core_data.GetMoleType();
}

u8 StoreData::GetMoleScale() const {
    return core_data.GetMoleScale();
}

u8 StoreData::GetMoleX() const {
    return core_data.GetMoleX();
}

u8 StoreData::GetMoleY() const {
    return core_data.GetMoleY();
}

Nickname StoreData::GetNickname() const {
    return core_data.GetNickname();
}

bool StoreData::operator==(const StoreData& data) {
    bool is_identical = data.core_data.IsValid() == ValidationResult::NoErrors;
    is_identical &= core_data.GetNickname().data == data.core_data.GetNickname().data;
    is_identical &= GetCreateId() == data.GetCreateId();
    is_identical &= GetFontRegion() == data.GetFontRegion();
    is_identical &= GetFavoriteColor() == data.GetFavoriteColor();
    is_identical &= GetGender() == data.GetGender();
    is_identical &= GetHeight() == data.GetHeight();
    is_identical &= GetBuild() == data.GetBuild();
    is_identical &= GetType() == data.GetType();
    is_identical &= GetRegionMove() == data.GetRegionMove();
    is_identical &= GetFacelineType() == data.GetFacelineType();
    is_identical &= GetFacelineColor() == data.GetFacelineColor();
    is_identical &= GetFacelineWrinkle() == data.GetFacelineWrinkle();
    is_identical &= GetFacelineMake() == data.GetFacelineMake();
    is_identical &= GetHairType() == data.GetHairType();
    is_identical &= GetHairColor() == data.GetHairColor();
    is_identical &= GetHairFlip() == data.GetHairFlip();
    is_identical &= GetEyeType() == data.GetEyeType();
    is_identical &= GetEyeColor() == data.GetEyeColor();
    is_identical &= GetEyeScale() == data.GetEyeScale();
    is_identical &= GetEyeAspect() == data.GetEyeAspect();
    is_identical &= GetEyeRotate() == data.GetEyeRotate();
    is_identical &= GetEyeX() == data.GetEyeX();
    is_identical &= GetEyeY() == data.GetEyeY();
    is_identical &= GetEyebrowType() == data.GetEyebrowType();
    is_identical &= GetEyebrowColor() == data.GetEyebrowColor();
    is_identical &= GetEyebrowScale() == data.GetEyebrowScale();
    is_identical &= GetEyebrowAspect() == data.GetEyebrowAspect();
    is_identical &= GetEyebrowRotate() == data.GetEyebrowRotate();
    is_identical &= GetEyebrowX() == data.GetEyebrowX();
    is_identical &= GetEyebrowY() == data.GetEyebrowY();
    is_identical &= GetNoseType() == data.GetNoseType();
    is_identical &= GetNoseScale() == data.GetNoseScale();
    is_identical &= GetNoseY() == data.GetNoseY();
    is_identical &= GetMouthType() == data.GetMouthType();
    is_identical &= GetMouthColor() == data.GetMouthColor();
    is_identical &= GetMouthScale() == data.GetMouthScale();
    is_identical &= GetMouthAspect() == data.GetMouthAspect();
    is_identical &= GetMouthY() == data.GetMouthY();
    is_identical &= GetBeardColor() == data.GetBeardColor();
    is_identical &= GetBeardType() == data.GetBeardType();
    is_identical &= GetMustacheType() == data.GetMustacheType();
    is_identical &= GetMustacheScale() == data.GetMustacheScale();
    is_identical &= GetMustacheY() == data.GetMustacheY();
    is_identical &= GetGlassType() == data.GetGlassType();
    is_identical &= GetGlassColor() == data.GetGlassColor();
    is_identical &= GetGlassScale() == data.GetGlassScale();
    is_identical &= GetGlassY() == data.GetGlassY();
    is_identical &= GetMoleType() == data.GetMoleType();
    is_identical &= GetMoleScale() == data.GetMoleScale();
    is_identical &= GetMoleX() == data.GetMoleX();
    is_identical &= data.GetMoleY() == data.GetMoleY();
    return is_identical;
}

} // namespace Service::Mii
