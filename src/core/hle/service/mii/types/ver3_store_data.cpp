// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/raw_data.h"
#include "core/hle/service/mii/types/store_data.h"
#include "core/hle/service/mii/types/ver3_store_data.h"

namespace Service::Mii {

void NfpStoreDataExtension::SetFromStoreData(const StoreData& store_data) {
    faceline_color = static_cast<u8>(store_data.GetFacelineColor()) & 0xf;
    hair_color = static_cast<u8>(store_data.GetHairColor()) & 0x7f;
    eye_color = static_cast<u8>(store_data.GetEyeColor()) & 0x7f;
    eyebrow_color = static_cast<u8>(store_data.GetEyebrowColor()) & 0x7f;
    mouth_color = static_cast<u8>(store_data.GetMouthColor()) & 0x7f;
    beard_color = static_cast<u8>(store_data.GetBeardColor()) & 0x7f;
    glass_color = static_cast<u8>(store_data.GetGlassColor()) & 0x7f;
    glass_type = static_cast<u8>(store_data.GetGlassType()) & 0x1f;
}

void Ver3StoreData::BuildToStoreData(StoreData& out_store_data) const {
    out_store_data.BuildBase(Gender::Male);

    out_store_data.SetGender(static_cast<Gender>(mii_information.gender.Value()));
    out_store_data.SetFavoriteColor(
        static_cast<FavoriteColor>(mii_information.favorite_color.Value()));
    out_store_data.SetHeight(height);
    out_store_data.SetBuild(build);

    out_store_data.SetNickname(mii_name);
    out_store_data.SetFontRegion(
        static_cast<FontRegion>(static_cast<u8>(region_information.font_region.Value())));

    out_store_data.SetFacelineType(
        static_cast<FacelineType>(appearance_bits1.faceline_type.Value()));
    out_store_data.SetFacelineColor(
        RawData::GetFacelineColorFromVer3(appearance_bits1.faceline_color.Value()));
    out_store_data.SetFacelineWrinkle(
        static_cast<FacelineWrinkle>(appearance_bits2.faceline_wrinkle.Value()));
    out_store_data.SetFacelineMake(
        static_cast<FacelineMake>(appearance_bits2.faceline_make.Value()));

    out_store_data.SetHairType(static_cast<HairType>(hair_type));
    out_store_data.SetHairColor(RawData::GetHairColorFromVer3(appearance_bits3.hair_color.Value()));
    out_store_data.SetHairFlip(static_cast<HairFlip>(appearance_bits3.hair_flip.Value()));

    out_store_data.SetEyeType(static_cast<EyeType>(appearance_bits4.eye_type.Value()));
    out_store_data.SetEyeColor(RawData::GetEyeColorFromVer3(appearance_bits4.eye_color.Value()));
    out_store_data.SetEyeScale(static_cast<u8>(appearance_bits4.eye_scale.Value()));
    out_store_data.SetEyeAspect(static_cast<u8>(appearance_bits4.eye_aspect.Value()));
    out_store_data.SetEyeRotate(static_cast<u8>(appearance_bits4.eye_rotate.Value()));
    out_store_data.SetEyeX(static_cast<u8>(appearance_bits4.eye_x.Value()));
    out_store_data.SetEyeY(static_cast<u8>(appearance_bits4.eye_y.Value()));

    out_store_data.SetEyebrowType(static_cast<EyebrowType>(appearance_bits5.eyebrow_type.Value()));
    out_store_data.SetEyebrowColor(
        RawData::GetHairColorFromVer3(appearance_bits5.eyebrow_color.Value()));
    out_store_data.SetEyebrowScale(static_cast<u8>(appearance_bits5.eyebrow_scale.Value()));
    out_store_data.SetEyebrowAspect(static_cast<u8>(appearance_bits5.eyebrow_aspect.Value()));
    out_store_data.SetEyebrowRotate(static_cast<u8>(appearance_bits5.eyebrow_rotate.Value()));
    out_store_data.SetEyebrowX(static_cast<u8>(appearance_bits5.eyebrow_x.Value()));
    out_store_data.SetEyebrowY(static_cast<u8>(appearance_bits5.eyebrow_y.Value() - 3));

    out_store_data.SetNoseType(static_cast<NoseType>(appearance_bits6.nose_type.Value()));
    out_store_data.SetNoseScale(static_cast<u8>(appearance_bits6.nose_scale.Value()));
    out_store_data.SetNoseY(static_cast<u8>(appearance_bits6.nose_y.Value()));

    out_store_data.SetMouthType(static_cast<MouthType>(appearance_bits7.mouth_type.Value()));
    out_store_data.SetMouthColor(
        RawData::GetMouthColorFromVer3(appearance_bits7.mouth_color.Value()));
    out_store_data.SetMouthScale(static_cast<u8>(appearance_bits7.mouth_scale.Value()));
    out_store_data.SetMouthAspect(static_cast<u8>(appearance_bits7.mouth_aspect.Value()));
    out_store_data.SetMouthY(static_cast<u8>(appearance_bits8.mouth_y.Value()));

    out_store_data.SetMustacheType(
        static_cast<MustacheType>(appearance_bits8.mustache_type.Value()));
    out_store_data.SetMustacheScale(static_cast<u8>(appearance_bits9.mustache_scale.Value()));
    out_store_data.SetMustacheY(static_cast<u8>(appearance_bits9.mustache_y.Value()));

    out_store_data.SetBeardType(static_cast<BeardType>(appearance_bits9.beard_type.Value()));
    out_store_data.SetBeardColor(
        RawData::GetHairColorFromVer3(appearance_bits9.beard_color.Value()));

    // Glass type is compatible as it is. It doesn't need a table
    out_store_data.SetGlassType(static_cast<GlassType>(appearance_bits10.glass_type.Value()));
    out_store_data.SetGlassColor(
        RawData::GetGlassColorFromVer3(appearance_bits10.glass_color.Value()));
    out_store_data.SetGlassScale(static_cast<u8>(appearance_bits10.glass_scale.Value()));
    out_store_data.SetGlassY(static_cast<u8>(appearance_bits10.glass_y.Value()));

    out_store_data.SetMoleType(static_cast<MoleType>(appearance_bits11.mole_type.Value()));
    out_store_data.SetMoleScale(static_cast<u8>(appearance_bits11.mole_scale.Value()));
    out_store_data.SetMoleX(static_cast<u8>(appearance_bits11.mole_x.Value()));
    out_store_data.SetMoleY(static_cast<u8>(appearance_bits11.mole_y.Value()));

    out_store_data.SetChecksum();
}

void Ver3StoreData::BuildFromStoreData(const StoreData& store_data) {
    version = 3;
    mii_information.gender.Assign(static_cast<u8>(store_data.GetGender()));
    mii_information.favorite_color.Assign(static_cast<u8>(store_data.GetFavoriteColor()));
    height = store_data.GetHeight();
    build = store_data.GetBuild();

    mii_name = store_data.GetNickname();
    region_information.font_region.Assign(static_cast<u8>(store_data.GetFontRegion()));

    appearance_bits1.faceline_type.Assign(static_cast<u8>(store_data.GetFacelineType()));
    appearance_bits2.faceline_wrinkle.Assign(static_cast<u8>(store_data.GetFacelineWrinkle()));
    appearance_bits2.faceline_make.Assign(static_cast<u8>(store_data.GetFacelineMake()));

    hair_type = static_cast<u8>(store_data.GetHairType());
    appearance_bits3.hair_flip.Assign(static_cast<u8>(store_data.GetHairFlip()));

    appearance_bits4.eye_type.Assign(static_cast<u8>(store_data.GetEyeType()));
    appearance_bits4.eye_scale.Assign(store_data.GetEyeScale());
    appearance_bits4.eye_aspect.Assign(store_data.GetEyebrowAspect());
    appearance_bits4.eye_rotate.Assign(store_data.GetEyeRotate());
    appearance_bits4.eye_x.Assign(store_data.GetEyeX());
    appearance_bits4.eye_y.Assign(store_data.GetEyeY());

    appearance_bits5.eyebrow_type.Assign(static_cast<u8>(store_data.GetEyebrowType()));
    appearance_bits5.eyebrow_scale.Assign(store_data.GetEyebrowScale());
    appearance_bits5.eyebrow_aspect.Assign(store_data.GetEyebrowAspect());
    appearance_bits5.eyebrow_rotate.Assign(store_data.GetEyebrowRotate());
    appearance_bits5.eyebrow_x.Assign(store_data.GetEyebrowX());
    appearance_bits5.eyebrow_y.Assign(store_data.GetEyebrowY());

    appearance_bits6.nose_type.Assign(static_cast<u8>(store_data.GetNoseType()));
    appearance_bits6.nose_scale.Assign(store_data.GetNoseScale());
    appearance_bits6.nose_y.Assign(store_data.GetNoseY());

    appearance_bits7.mouth_type.Assign(static_cast<u8>(store_data.GetMouthType()));
    appearance_bits7.mouth_scale.Assign(store_data.GetMouthScale());
    appearance_bits7.mouth_aspect.Assign(store_data.GetMouthAspect());
    appearance_bits8.mouth_y.Assign(store_data.GetMouthY());

    appearance_bits8.mustache_type.Assign(static_cast<u8>(store_data.GetMustacheType()));
    appearance_bits9.mustache_scale.Assign(store_data.GetMustacheScale());
    appearance_bits9.mustache_y.Assign(store_data.GetMustacheY());

    appearance_bits9.beard_type.Assign(static_cast<u8>(store_data.GetBeardType()));

    appearance_bits10.glass_scale.Assign(store_data.GetGlassScale());
    appearance_bits10.glass_y.Assign(store_data.GetGlassY());

    appearance_bits11.mole_type.Assign(static_cast<u8>(store_data.GetMoleType()));
    appearance_bits11.mole_scale.Assign(store_data.GetMoleScale());
    appearance_bits11.mole_x.Assign(store_data.GetMoleX());
    appearance_bits11.mole_y.Assign(store_data.GetMoleY());

    // These types are converted to V3 from a table
    appearance_bits1.faceline_color.Assign(
        RawData::FromVer3GetFacelineColor(static_cast<u8>(store_data.GetFacelineColor())));
    appearance_bits3.hair_color.Assign(
        RawData::FromVer3GetHairColor(static_cast<u8>(store_data.GetHairColor())));
    appearance_bits4.eye_color.Assign(
        RawData::FromVer3GetEyeColor(static_cast<u8>(store_data.GetEyeColor())));
    appearance_bits5.eyebrow_color.Assign(
        RawData::FromVer3GetHairColor(static_cast<u8>(store_data.GetEyebrowColor())));
    appearance_bits7.mouth_color.Assign(
        RawData::FromVer3GetMouthlineColor(static_cast<u8>(store_data.GetMouthColor())));
    appearance_bits9.beard_color.Assign(
        RawData::FromVer3GetHairColor(static_cast<u8>(store_data.GetBeardColor())));
    appearance_bits10.glass_color.Assign(
        RawData::FromVer3GetGlassColor(static_cast<u8>(store_data.GetGlassColor())));
    appearance_bits10.glass_type.Assign(
        RawData::FromVer3GetGlassType(static_cast<u8>(store_data.GetGlassType())));

    crc = MiiUtil::CalculateCrc16(&version, sizeof(Ver3StoreData) - sizeof(u16));
}

u32 Ver3StoreData::IsValid() const {
    bool is_valid = version == 0 || version == 3;

    is_valid = is_valid && (mii_name.data[0] != '\0');

    is_valid = is_valid && (mii_information.birth_month < 13);
    is_valid = is_valid && (mii_information.birth_day < 32);
    is_valid = is_valid && (mii_information.favorite_color <= static_cast<u8>(FavoriteColor::Max));
    is_valid = is_valid && (height <= MaxHeight);
    is_valid = is_valid && (build <= MaxBuild);

    is_valid = is_valid && (appearance_bits1.faceline_type <= static_cast<u8>(FacelineType::Max));
    is_valid = is_valid && (appearance_bits1.faceline_color <= MaxVer3CommonColor - 2);
    is_valid =
        is_valid && (appearance_bits2.faceline_wrinkle <= static_cast<u8>(FacelineWrinkle::Max));
    is_valid = is_valid && (appearance_bits2.faceline_make <= static_cast<u8>(FacelineMake::Max));

    is_valid = is_valid && (hair_type <= static_cast<u8>(HairType::Max));
    is_valid = is_valid && (appearance_bits3.hair_color <= MaxVer3CommonColor);

    is_valid = is_valid && (appearance_bits4.eye_type <= static_cast<u8>(EyeType::Max));
    is_valid = is_valid && (appearance_bits4.eye_color <= MaxVer3CommonColor - 2);
    is_valid = is_valid && (appearance_bits4.eye_scale <= MaxEyeScale);
    is_valid = is_valid && (appearance_bits4.eye_aspect <= MaxEyeAspect);
    is_valid = is_valid && (appearance_bits4.eye_rotate <= MaxEyeRotate);
    is_valid = is_valid && (appearance_bits4.eye_x <= MaxEyeX);
    is_valid = is_valid && (appearance_bits4.eye_y <= MaxEyeY);

    is_valid = is_valid && (appearance_bits5.eyebrow_type <= static_cast<u8>(EyebrowType::Max));
    is_valid = is_valid && (appearance_bits5.eyebrow_color <= MaxVer3CommonColor);
    is_valid = is_valid && (appearance_bits5.eyebrow_scale <= MaxEyebrowScale);
    is_valid = is_valid && (appearance_bits5.eyebrow_aspect <= MaxEyebrowAspect);
    is_valid = is_valid && (appearance_bits5.eyebrow_rotate <= MaxEyebrowRotate);
    is_valid = is_valid && (appearance_bits5.eyebrow_x <= MaxEyebrowX);
    is_valid = is_valid && (appearance_bits5.eyebrow_y <= MaxEyebrowY);

    is_valid = is_valid && (appearance_bits6.nose_type <= static_cast<u8>(NoseType::Max));
    is_valid = is_valid && (appearance_bits6.nose_scale <= MaxNoseScale);
    is_valid = is_valid && (appearance_bits6.nose_y <= MaxNoseY);

    is_valid = is_valid && (appearance_bits7.mouth_type <= static_cast<u8>(MouthType::Max));
    is_valid = is_valid && (appearance_bits7.mouth_color <= MaxVer3CommonColor - 3);
    is_valid = is_valid && (appearance_bits7.mouth_scale <= MaxMouthScale);
    is_valid = is_valid && (appearance_bits7.mouth_aspect <= MaxMoutAspect);
    is_valid = is_valid && (appearance_bits8.mouth_y <= MaxMouthY);

    is_valid = is_valid && (appearance_bits8.mustache_type <= static_cast<u8>(MustacheType::Max));
    is_valid = is_valid && (appearance_bits9.mustache_scale < MaxMustacheScale);
    is_valid = is_valid && (appearance_bits9.mustache_y <= MaxMustacheY);

    is_valid = is_valid && (appearance_bits9.beard_type <= static_cast<u8>(BeardType::Max));
    is_valid = is_valid && (appearance_bits9.beard_color <= MaxVer3CommonColor);

    is_valid = is_valid && (appearance_bits10.glass_type <= MaxVer3GlassType);
    is_valid = is_valid && (appearance_bits10.glass_color <= MaxVer3CommonColor - 2);
    is_valid = is_valid && (appearance_bits10.glass_scale <= MaxGlassScale);
    is_valid = is_valid && (appearance_bits10.glass_y <= MaxGlassY);

    is_valid = is_valid && (appearance_bits11.mole_type <= static_cast<u8>(MoleType::Max));
    is_valid = is_valid && (appearance_bits11.mole_scale <= MaxMoleScale);
    is_valid = is_valid && (appearance_bits11.mole_x <= MaxMoleX);
    is_valid = is_valid && (appearance_bits11.mole_y <= MaxMoleY);

    return is_valid;
}

} // namespace Service::Mii
