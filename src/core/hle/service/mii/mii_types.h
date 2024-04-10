// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <type_traits>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/uuid.h"

namespace Service::Mii {

constexpr std::size_t MaxNameSize = 10;
constexpr u8 MaxHeight = 127;
constexpr u8 MaxBuild = 127;
constexpr u8 MaxType = 1;
constexpr u8 MaxRegionMove = 3;
constexpr u8 MaxEyeScale = 7;
constexpr u8 MaxEyeAspect = 6;
constexpr u8 MaxEyeRotate = 7;
constexpr u8 MaxEyeX = 12;
constexpr u8 MaxEyeY = 18;
constexpr u8 MaxEyebrowScale = 8;
constexpr u8 MaxEyebrowAspect = 6;
constexpr u8 MaxEyebrowRotate = 11;
constexpr u8 MaxEyebrowX = 12;
constexpr u8 MaxEyebrowY = 15;
constexpr u8 MaxNoseScale = 8;
constexpr u8 MaxNoseY = 18;
constexpr u8 MaxMouthScale = 8;
constexpr u8 MaxMoutAspect = 6;
constexpr u8 MaxMouthY = 18;
constexpr u8 MaxMustacheScale = 8;
constexpr u8 MaxMustacheY = 16;
constexpr u8 MaxGlassScale = 7;
constexpr u8 MaxGlassY = 20;
constexpr u8 MaxMoleScale = 8;
constexpr u8 MaxMoleX = 16;
constexpr u8 MaxMoleY = 30;
constexpr u8 MaxVer3CommonColor = 7;
constexpr u8 MaxVer3GlassType = 8;

enum class Age : u8 {
    Young,
    Normal,
    Old,
    All, // Default

    Max = All,
};

enum class Gender : u8 {
    Male,
    Female,
    All, // Default

    Max = Female,
};

enum class Race : u8 {
    Black,
    White,
    Asian,
    All, // Default

    Max = All,
};

enum class HairType : u8 {
    NormalLong, // Default
    NormalShort,
    NormalMedium,
    NormalExtraLong,
    NormalLongBottom,
    NormalTwoPeaks,
    PartingLong,
    FrontLock,
    PartingShort,
    PartingExtraLongCurved,
    PartingExtraLong,
    PartingMiddleLong,
    PartingSquared,
    PartingLongBottom,
    PeaksTop,
    PeaksSquared,
    PartingPeaks,
    PeaksLongBottom,
    Peaks,
    PeaksRounded,
    PeaksSide,
    PeaksMedium,
    PeaksLong,
    PeaksRoundedLong,
    PartingFrontPeaks,
    PartingLongFront,
    PartingLongRounded,
    PartingFrontPeaksLong,
    PartingExtraLongRounded,
    LongRounded,
    NormalUnknown1,
    NormalUnknown2,
    NormalUnknown3,
    NormalUnknown4,
    NormalUnknown5,
    NormalUnknown6,
    DreadLocks,
    PlatedMats,
    Caps,
    Afro,
    PlatedMatsLong,
    Beanie,
    Short,
    ShortTopLongSide,
    ShortUnknown1,
    ShortUnknown2,
    MilitaryParting,
    Military,
    ShortUnknown3,
    ShortUnknown4,
    ShortUnknown5,
    ShortUnknown6,
    NoneTop,
    None,
    LongUnknown1,
    LongUnknown2,
    LongUnknown3,
    LongUnknown4,
    LongUnknown5,
    LongUnknown6,
    LongUnknown7,
    LongUnknown8,
    LongUnknown9,
    LongUnknown10,
    LongUnknown11,
    LongUnknown12,
    LongUnknown13,
    LongUnknown14,
    LongUnknown15,
    LongUnknown16,
    LongUnknown17,
    LongUnknown18,
    LongUnknown19,
    LongUnknown20,
    LongUnknown21,
    LongUnknown22,
    LongUnknown23,
    LongUnknown24,
    LongUnknown25,
    LongUnknown26,
    LongUnknown27,
    LongUnknown28,
    LongUnknown29,
    LongUnknown30,
    LongUnknown31,
    LongUnknown32,
    LongUnknown33,
    LongUnknown34,
    LongUnknown35,
    LongUnknown36,
    LongUnknown37,
    LongUnknown38,
    LongUnknown39,
    LongUnknown40,
    LongUnknown41,
    LongUnknown42,
    LongUnknown43,
    LongUnknown44,
    LongUnknown45,
    LongUnknown46,
    LongUnknown47,
    LongUnknown48,
    LongUnknown49,
    LongUnknown50,
    LongUnknown51,
    LongUnknown52,
    LongUnknown53,
    LongUnknown54,
    LongUnknown55,
    LongUnknown56,
    LongUnknown57,
    LongUnknown58,
    LongUnknown59,
    LongUnknown60,
    LongUnknown61,
    LongUnknown62,
    LongUnknown63,
    LongUnknown64,
    LongUnknown65,
    LongUnknown66,
    TwoMediumFrontStrandsOneLongBackPonyTail,
    TwoFrontStrandsLongBackPonyTail,
    PartingFrontTwoLongBackPonyTails,
    TwoFrontStrandsOneLongBackPonyTail,
    LongBackPonyTail,
    LongFrontTwoLongBackPonyTails,
    StrandsTwoShortSidedPonyTails,
    TwoMediumSidedPonyTails,
    ShortFrontTwoBackPonyTails,
    TwoShortSidedPonyTails,
    TwoLongSidedPonyTails,
    LongFrontTwoBackPonyTails,

    Max = LongFrontTwoBackPonyTails,
};

enum class MoleType : u8 {
    None, // Default
    OneDot,

    Max = OneDot,
};

enum class HairFlip : u8 {
    Left, // Default
    Right,

    Max = Right,
};

enum class CommonColor : u8 {
    // For simplicity common colors aren't listed
    Max = 99,
    Count = 100,
};

enum class FavoriteColor : u8 {
    Red, // Default
    Orange,
    Yellow,
    LimeGreen,
    Green,
    Blue,
    LightBlue,
    Pink,
    Purple,
    Brown,
    White,
    Black,

    Max = Black,
};

enum class EyeType : u8 {
    Normal, // Default
    NormalLash,
    WhiteLash,
    WhiteNoBottom,
    OvalAngledWhite,
    AngryWhite,
    DotLashType1,
    Line,
    DotLine,
    OvalWhite,
    RoundedWhite,
    NormalShadow,
    CircleWhite,
    Circle,
    CircleWhiteStroke,
    NormalOvalNoBottom,
    NormalOvalLarge,
    NormalRoundedNoBottom,
    SmallLash,
    Small,
    TwoSmall,
    NormalLongLash,
    WhiteTwoLashes,
    WhiteThreeLashes,
    DotAngry,
    DotAngled,
    Oval,
    SmallWhite,
    WhiteAngledNoBottom,
    WhiteAngledNoLeft,
    SmallWhiteTwoLashes,
    LeafWhiteLash,
    WhiteLargeNoBottom,
    Dot,
    DotLashType2,
    DotThreeLashes,
    WhiteOvalTop,
    WhiteOvalBottom,
    WhiteOvalBottomFlat,
    WhiteOvalTwoLashes,
    WhiteOvalThreeLashes,
    WhiteOvalNoBottomTwoLashes,
    DotWhite,
    WhiteOvalTopFlat,
    WhiteThinLeaf,
    StarThreeLashes,
    LineTwoLashes,
    CrowsFeet,
    WhiteNoBottomFlat,
    WhiteNoBottomRounded,
    WhiteSmallBottomLine,
    WhiteNoBottomLash,
    WhiteNoPartialBottomLash,
    WhiteOvalBottomLine,
    WhiteNoBottomLashTopLine,
    WhiteNoPartialBottomTwoLashes,
    NormalTopLine,
    WhiteOvalLash,
    RoundTired,
    WhiteLarge,

    Max = WhiteLarge,
};

enum class MouthType : u8 {
    Neutral, // Default
    NeutralLips,
    Smile,
    SmileStroke,
    SmileTeeth,
    LipsSmall,
    LipsLarge,
    Wave,
    WaveAngrySmall,
    NeutralStrokeLarge,
    TeethSurprised,
    LipsExtraLarge,
    LipsUp,
    NeutralDown,
    Surprised,
    TeethMiddle,
    NeutralStroke,
    LipsExtraSmall,
    Malicious,
    LipsDual,
    NeutralComma,
    NeutralUp,
    TeethLarge,
    WaveAngry,
    LipsSexy,
    SmileInverted,
    LipsSexyOutline,
    SmileRounded,
    LipsTeeth,
    NeutralOpen,
    TeethRounded,
    WaveAngrySmallInverted,
    NeutralCommaInverted,
    TeethFull,
    SmileDownLine,
    Kiss,

    Max = Kiss,
};

enum class FontRegion : u8 {
    Standard, // Default
    China,
    Korea,
    Taiwan,

    Max = Taiwan,
};

enum class FacelineType : u8 {
    Sharp, // Default
    Rounded,
    SharpRounded,
    SharpRoundedSmall,
    Large,
    LargeRounded,
    SharpSmall,
    Flat,
    Bump,
    Angular,
    FlatRounded,
    AngularSmall,

    Max = AngularSmall,
};

enum class FacelineColor : u8 {
    Beige, // Default
    WarmBeige,
    Natural,
    Honey,
    Chestnut,
    Porcelain,
    Ivory,
    WarmIvory,
    Almond,
    Espresso,

    Max = Espresso,
    Count = Max + 1,
};

enum class FacelineWrinkle : u8 {
    None, // Default
    TearTroughs,
    FacialPain,
    Cheeks,
    Folds,
    UnderTheEyes,
    SplitChin,
    Chin,
    BrowDroop,
    MouthFrown,
    CrowsFeet,
    FoldsCrowsFrown,

    Max = FoldsCrowsFrown,
};

enum class FacelineMake : u8 {
    None, // Default
    CheekPorcelain,
    CheekNatural,
    EyeShadowBlue,
    CheekBlushPorcelain,
    CheekBlushNatural,
    CheekPorcelainEyeShadowBlue,
    CheekPorcelainEyeShadowNatural,
    CheekBlushPorcelainEyeShadowEspresso,
    Freckles,
    LionsManeBeard,
    StubbleBeard,

    Max = StubbleBeard,
};

enum class EyebrowType : u8 {
    FlatAngledLarge, // Default
    LowArchRoundedThin,
    SoftAngledLarge,
    MediumArchRoundedThin,
    RoundedMedium,
    LowArchMedium,
    RoundedThin,
    UpThin,
    MediumArchRoundedMedium,
    RoundedLarge,
    UpLarge,
    FlatAngledLargeInverted,
    MediumArchFlat,
    AngledThin,
    HorizontalLarge,
    HighArchFlat,
    Flat,
    MediumArchLarge,
    LowArchThin,
    RoundedThinInverted,
    HighArchLarge,
    Hairy,
    Dotted,
    None,

    Max = None,
};

enum class NoseType : u8 {
    Normal, // Default
    Rounded,
    Dot,
    Arrow,
    Roman,
    Triangle,
    Button,
    RoundedInverted,
    Potato,
    Grecian,
    Snub,
    Aquiline,
    ArrowLeft,
    RoundedLarge,
    Hooked,
    Fat,
    Droopy,
    ArrowLarge,

    Max = ArrowLarge,
};

enum class BeardType : u8 {
    None,
    Goatee,
    GoateeLong,
    LionsManeLong,
    LionsMane,
    Full,

    Min = Goatee,
    Max = Full,
};

enum class MustacheType : u8 {
    None,
    Walrus,
    Pencil,
    Horseshoe,
    Normal,
    Toothbrush,

    Min = Walrus,
    Max = Toothbrush,
};

enum class GlassType : u8 {
    None,
    Oval,
    Wayfarer,
    Rectangle,
    TopRimless,
    Rounded,
    Oversized,
    CatEye,
    Square,
    BottomRimless,
    SemiOpaqueRounded,
    SemiOpaqueCatEye,
    SemiOpaqueOval,
    SemiOpaqueRectangle,
    SemiOpaqueAviator,
    OpaqueRounded,
    OpaqueCatEye,
    OpaqueOval,
    OpaqueRectangle,
    OpaqueAviator,

    Max = OpaqueAviator,
    Count = Max + 1,
};

enum class BeardAndMustacheFlag : u32 {
    Beard = 1,
    Mustache,
    All = Beard | Mustache,
};
DECLARE_ENUM_FLAG_OPERATORS(BeardAndMustacheFlag);

enum class Source : u32 {
    Database = 0,
    Default = 1,
    Account = 2,
    Friend = 3,
};

enum class SourceFlag : u32 {
    None = 0,
    Database = 1 << 0,
    Default = 1 << 1,
};
DECLARE_ENUM_FLAG_OPERATORS(SourceFlag);

enum class ValidationResult : u32 {
    NoErrors = 0x0,
    InvalidBeardColor = 0x1,
    InvalidBeardType = 0x2,
    InvalidBuild = 0x3,
    InvalidEyeAspect = 0x4,
    InvalidEyeColor = 0x5,
    InvalidEyeRotate = 0x6,
    InvalidEyeScale = 0x7,
    InvalidEyeType = 0x8,
    InvalidEyeX = 0x9,
    InvalidEyeY = 0xa,
    InvalidEyebrowAspect = 0xb,
    InvalidEyebrowColor = 0xc,
    InvalidEyebrowRotate = 0xd,
    InvalidEyebrowScale = 0xe,
    InvalidEyebrowType = 0xf,
    InvalidEyebrowX = 0x10,
    InvalidEyebrowY = 0x11,
    InvalidFacelineColor = 0x12,
    InvalidFacelineMake = 0x13,
    InvalidFacelineWrinkle = 0x14,
    InvalidFacelineType = 0x15,
    InvalidColor = 0x16,
    InvalidFont = 0x17,
    InvalidGender = 0x18,
    InvalidGlassColor = 0x19,
    InvalidGlassScale = 0x1a,
    InvalidGlassType = 0x1b,
    InvalidGlassY = 0x1c,
    InvalidHairColor = 0x1d,
    InvalidHairFlip = 0x1e,
    InvalidHairType = 0x1f,
    InvalidHeight = 0x20,
    InvalidMoleScale = 0x21,
    InvalidMoleType = 0x22,
    InvalidMoleX = 0x23,
    InvalidMoleY = 0x24,
    InvalidMouthAspect = 0x25,
    InvalidMouthColor = 0x26,
    InvalidMouthScale = 0x27,
    InvalidMouthType = 0x28,
    InvalidMouthY = 0x29,
    InvalidMustacheScale = 0x2a,
    InvalidMustacheType = 0x2b,
    InvalidMustacheY = 0x2c,
    InvalidNoseScale = 0x2e,
    InvalidNoseType = 0x2f,
    InvalidNoseY = 0x30,
    InvalidRegionMove = 0x31,
    InvalidCreateId = 0x32,
    InvalidName = 0x33,
    InvalidChecksum = 0x34,
    InvalidType = 0x35,
};

struct Nickname {
    std::array<char16_t, MaxNameSize> data{};

    // Checks for null or dirty strings
    bool IsValid() const {
        if (data[0] == 0) {
            return false;
        }

        std::size_t index = 1;
        while (index < MaxNameSize && data[index] != 0) {
            index++;
        }
        while (index < MaxNameSize && data[index] == 0) {
            index++;
        }
        return index == MaxNameSize;
    }
};
static_assert(sizeof(Nickname) == 0x14, "Nickname is an invalid size");

struct DefaultMii {
    u32 face_type{};
    u32 face_color{};
    u32 face_wrinkle{};
    u32 face_makeup{};
    u32 hair_type{};
    u32 hair_color{};
    u32 hair_flip{};
    u32 eye_type{};
    u32 eye_color{};
    u32 eye_scale{};
    u32 eye_aspect{};
    u32 eye_rotate{};
    u32 eye_x{};
    u32 eye_y{};
    u32 eyebrow_type{};
    u32 eyebrow_color{};
    u32 eyebrow_scale{};
    u32 eyebrow_aspect{};
    u32 eyebrow_rotate{};
    u32 eyebrow_x{};
    u32 eyebrow_y{};
    u32 nose_type{};
    u32 nose_scale{};
    u32 nose_y{};
    u32 mouth_type{};
    u32 mouth_color{};
    u32 mouth_scale{};
    u32 mouth_aspect{};
    u32 mouth_y{};
    u32 mustache_type{};
    u32 beard_type{};
    u32 beard_color{};
    u32 mustache_scale{};
    u32 mustache_y{};
    u32 glasses_type{};
    u32 glasses_color{};
    u32 glasses_scale{};
    u32 glasses_y{};
    u32 mole_type{};
    u32 mole_scale{};
    u32 mole_x{};
    u32 mole_y{};
    u32 height{};
    u32 weight{};
    u32 gender{};
    u32 favorite_color{};
    u32 region_move{};
    u32 font_region{};
    u32 type{};
    Nickname nickname;
};
static_assert(sizeof(DefaultMii) == 0xd8, "DefaultMii has incorrect size.");

struct DatabaseSessionMetadata {
    u32 interface_version;
    u32 magic;
    u64 update_counter;

    bool IsInterfaceVersionSupported(u32 version) const {
        return version <= interface_version;
    }
};

} // namespace Service::Mii
