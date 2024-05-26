// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/expected.h"

// All the constants in this file come from http://switchbrew.org/index.php?title=Error_codes

/**
 * Identifies the module which caused the error. Error codes can be propagated through a call
 * chain, meaning that this doesn't always correspond to the module where the API call made is
 * contained.
 */
enum class ErrorModule : u32 {
    Common = 0,
    Kernel = 1,
    FS = 2,
    OS = 3, // used for Memory, Thread, Mutex, Nvidia
    HTCS = 4,
    NCM = 5,
    DD = 6,
    OSDBG = 7,
    LR = 8,
    Loader = 9,
    CMIF = 10,
    HIPC = 11,
    TMA = 12,
    DMNT = 13,
    GDS = 14,
    PM = 15,
    NS = 16,
    BSDSockets = 17,
    HTC = 18,
    TSC = 19,
    NCMContent = 20,
    SM = 21,
    RO = 22,
    GC = 23,
    SDMMC = 24,
    OVLN = 25,
    SPL = 26,
    Socket = 27,
    HTCLOW = 29,
    DDSF = 30,
    HTCFS = 31,
    Async = 32,
    Util = 33,
    TIPC = 35,
    ANIF = 37,
    CRT = 39,
    ETHC = 100,
    I2C = 101,
    GPIO = 102,
    UART = 103,
    CPAD = 104,
    Settings = 105,
    FTM = 106,
    WLAN = 107,
    XCD = 108,
    TMP451 = 109,
    NIFM = 110,
    HwOpus = 111,
    LSM6DS3 = 112,
    Bluetooth = 113,
    VI = 114,
    NFP = 115,
    Time = 116,
    FGM = 117,
    OE = 118,
    BH1730FVC = 119,
    PCIe = 120,
    Friends = 121,
    BCAT = 122,
    SSLSrv = 123,
    Account = 124,
    News = 125,
    Mii = 126,
    NFC = 127,
    AM = 128,
    PlayReport = 129,
    AHID = 130,
    Qlaunch = 132,
    PCV = 133,
    USBPD = 134,
    BPC = 135,
    PSM = 136,
    NIM = 137,
    PSC = 138,
    TC = 139,
    USB = 140,
    NSD = 141,
    PCTL = 142,
    BTM = 143,
    LA = 144,
    ETicket = 145,
    NGC = 146,
    ERPT = 147,
    APM = 148,
    CEC = 149,
    Profiler = 150,
    ErrorUpload = 151,
    LIDBE = 152,
    Audio = 153,
    NPNS = 154,
    NPNSHTTPSTREAM = 155,
    IDLE = 156,
    ARP = 157,
    SWKBD = 158,
    BOOT = 159,
    NetDiag = 160,
    NFCMifare = 161,
    UserlandAssert = 162,
    Fatal = 163,
    NIMShop = 164,
    SPSM = 165,
    AOC = 166,
    BGTC = 167,
    UserlandCrash = 168,
    SASBUS = 169,
    PI = 170,
    AudioCtrl = 172,
    LBL = 173,
    JIT = 175,
    HDCP = 176,
    OMM = 177,
    PDM = 178,
    OLSC = 179,
    SREPO = 180,
    Dauth = 181,
    STDFU = 182,
    DBG = 183,
    DHCPS = 186,
    SPI = 187,
    AVM = 188,
    PWM = 189,
    RTC = 191,
    Regulator = 192,
    LED = 193,
    SIO = 195,
    PCM = 196,
    CLKRST = 197,
    POWCTL = 198,
    AudioOld = 201,
    HID = 202,
    LDN = 203,
    CS = 204,
    Irsensor = 205,
    Capture = 206,
    Manu = 208,
    ATK = 209,
    WEB = 210,
    LCS = 211,
    GRC = 212,
    Repair = 213,
    Album = 214,
    RID = 215,
    Migration = 216,
    MigrationLdcServ = 217,
    HIDBUS = 218,
    ENS = 219,
    WebSocket = 223,
    DCDMTP = 227,
    PGL = 228,
    Notification = 229,
    INS = 230,
    LP2P = 231,
    RCD = 232,
    LCM40607 = 233,
    PRC = 235,
    TMAHTC = 237,
    ECTX = 238,
    MNPP = 239,
    HSHL = 240,
    CAPMTP = 242,
    DP2HDMI = 244,
    Cradle = 245,
    SProfile = 246,
    Icm42607p = 248,
    NDRM = 250,
    Fst2 = 251,
    Nex = 306,
    NPLN = 321,
    TSPM = 499,
    DevMenu = 500,
    Nverpt = 520,
    Am_StuckMonitor = 521,
    Pia = 618,
    Eagle = 623,
    GeneralWebApplet = 800,
    WifiWebAuthApplet = 809,
    WhitelistedApplet = 810,
    ShopN = 811,
    Coral = 815
};

/// Encapsulates a Horizon OS error code, allowing it to be separated into its constituent fields.
union Result {
    u32 raw;

    using Module = BitField<0, 9, ErrorModule>;
    using Description = BitField<9, 13, u32>;

    Result() = default;
    constexpr explicit Result(u32 raw_) : raw(raw_) {}

    constexpr Result(ErrorModule module_, u32 description_)
        : raw(Module::FormatValue(module_) | Description::FormatValue(description_)) {}

    [[nodiscard]] constexpr bool IsSuccess() const {
        return raw == 0;
    }

    [[nodiscard]] constexpr bool IsError() const {
        return !IsSuccess();
    }

    [[nodiscard]] constexpr bool IsFailure() const {
        return !IsSuccess();
    }

    [[nodiscard]] constexpr u32 GetInnerValue() const {
        return raw;
    }

    [[nodiscard]] constexpr ErrorModule GetModule() const {
        return Module::ExtractValue(raw);
    }

    [[nodiscard]] constexpr u32 GetDescription() const {
        return Description::ExtractValue(raw);
    }

    [[nodiscard]] constexpr bool Includes(Result result) const {
        return GetInnerValue() == result.GetInnerValue();
    }
};
static_assert(std::is_trivial_v<Result>);

[[nodiscard]] constexpr bool operator==(const Result& a, const Result& b) {
    return a.raw == b.raw;
}

[[nodiscard]] constexpr bool operator!=(const Result& a, const Result& b) {
    return !operator==(a, b);
}

// Convenience functions for creating some common kinds of errors:

/// The default success `Result`.
constexpr Result ResultSuccess(0);

/**
 * Placeholder result code used for unknown error codes.
 *
 * @note This should only be used when a particular error code
 *       is not known yet.
 */
constexpr Result ResultUnknown(UINT32_MAX);

/**
 * A ResultRange defines an inclusive range of error descriptions within an error module.
 * This can be used to check whether the description of a given Result falls within the range.
 * The conversion function returns a Result with its description set to description_start.
 *
 * An example of how it could be used:
 * \code
 * constexpr ResultRange ResultCommonError{ErrorModule::Common, 0, 9999};
 *
 * Result Example(int value) {
 *     const Result result = OtherExample(value);
 *
 *     // This will only evaluate to true if result.module is ErrorModule::Common and
 *     // result.description is in between 0 and 9999 inclusive.
 *     if (ResultCommonError.Includes(result)) {
 *         // This returns Result{ErrorModule::Common, 0};
 *         return ResultCommonError;
 *     }
 *
 *     return ResultSuccess;
 * }
 * \endcode
 */
class ResultRange {
public:
    consteval ResultRange(ErrorModule module, u32 description_start, u32 description_end_)
        : code{module, description_start}, description_end{description_end_} {}

    [[nodiscard]] constexpr operator Result() const {
        return code;
    }

    [[nodiscard]] constexpr bool Includes(Result other) const {
        return code.GetModule() == other.GetModule() &&
               code.GetDescription() <= other.GetDescription() &&
               other.GetDescription() <= description_end;
    }

private:
    Result code;
    u32 description_end;
};

#define R_SUCCEEDED(res) (static_cast<Result>(res).IsSuccess())
#define R_FAILED(res) (static_cast<Result>(res).IsFailure())

namespace ResultImpl {
template <auto EvaluateResult, class F>
class ScopedResultGuard {
    SUDACHI_NON_COPYABLE(ScopedResultGuard);
    SUDACHI_NON_MOVEABLE(ScopedResultGuard);

private:
    Result& m_ref;
    F m_f;

public:
    constexpr ScopedResultGuard(Result& ref, F f) : m_ref(ref), m_f(std::move(f)) {}
    constexpr ~ScopedResultGuard() {
        if (EvaluateResult(m_ref)) {
            m_f();
        }
    }
};

template <auto EvaluateResult>
class ResultReferenceForScopedResultGuard {
private:
    Result& m_ref;

public:
    constexpr ResultReferenceForScopedResultGuard(Result& r) : m_ref(r) {}
    constexpr operator Result&() const {
        return m_ref;
    }
};

template <auto EvaluateResult, typename F>
constexpr ScopedResultGuard<EvaluateResult, F> operator+(
    ResultReferenceForScopedResultGuard<EvaluateResult> ref, F&& f) {
    return ScopedResultGuard<EvaluateResult, F>(static_cast<Result&>(ref), std::forward<F>(f));
}

constexpr bool EvaluateResultSuccess(const Result& r) {
    return R_SUCCEEDED(r);
}
constexpr bool EvaluateResultFailure(const Result& r) {
    return R_FAILED(r);
}

template <auto... R>
constexpr bool EvaluateAnyResultIncludes(const Result& r) {
    return ((r == R) || ...);
}

template <auto... R>
constexpr bool EvaluateResultNotIncluded(const Result& r) {
    return !EvaluateAnyResultIncludes<R...>(r);
}

template <typename T>
constexpr void UpdateCurrentResultReference(T result_reference, Result result) = delete;
// Intentionally not defined

template <>
constexpr void UpdateCurrentResultReference<Result&>(Result& result_reference, Result result) {
    result_reference = result;
}

template <>
constexpr void UpdateCurrentResultReference<const Result>(Result result_reference, Result result) {}
} // namespace ResultImpl

#define DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(COUNTER_VALUE)                                \
    [[maybe_unused]] constexpr bool CONCAT2(HasPrevRef_, COUNTER_VALUE) =                          \
        std::same_as<decltype(__TmpCurrentResultReference), Result&>;                              \
    [[maybe_unused]] Result CONCAT2(PrevRef_, COUNTER_VALUE) = __TmpCurrentResultReference;        \
    [[maybe_unused]] Result CONCAT2(__tmp_result_, COUNTER_VALUE) = ResultSuccess;                 \
    Result& __TmpCurrentResultReference = CONCAT2(HasPrevRef_, COUNTER_VALUE)                      \
                                              ? CONCAT2(PrevRef_, COUNTER_VALUE)                   \
                                              : CONCAT2(__tmp_result_, COUNTER_VALUE)

#define ON_RESULT_RETURN_IMPL(...)                                                                 \
    static_assert(std::same_as<decltype(__TmpCurrentResultReference), Result&>);                   \
    auto CONCAT2(RESULT_GUARD_STATE_, __COUNTER__) =                                               \
        ResultImpl::ResultReferenceForScopedResultGuard<__VA_ARGS__>(                              \
            __TmpCurrentResultReference) +                                                         \
        [&]()

#define ON_RESULT_FAILURE_2 ON_RESULT_RETURN_IMPL(ResultImpl::EvaluateResultFailure)

#define ON_RESULT_FAILURE                                                                          \
    DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(__COUNTER__);                                     \
    ON_RESULT_FAILURE_2

#define ON_RESULT_SUCCESS_2 ON_RESULT_RETURN_IMPL(ResultImpl::EvaluateResultSuccess)

#define ON_RESULT_SUCCESS                                                                          \
    DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(__COUNTER__);                                     \
    ON_RESULT_SUCCESS_2

#define ON_RESULT_INCLUDED_2(...)                                                                  \
    ON_RESULT_RETURN_IMPL(ResultImpl::EvaluateAnyResultIncludes<__VA_ARGS__>)

#define ON_RESULT_INCLUDED(...)                                                                    \
    DECLARE_CURRENT_RESULT_REFERENCE_AND_STORAGE(__COUNTER__);                                     \
    ON_RESULT_INCLUDED_2(__VA_ARGS__)

constexpr inline Result __TmpCurrentResultReference = ResultSuccess;

/// Returns a result.
#define R_RETURN(res_expr)                                                                         \
    {                                                                                              \
        const Result _tmp_r_throw_rc = (res_expr);                                                 \
        ResultImpl::UpdateCurrentResultReference<decltype(__TmpCurrentResultReference)>(           \
            __TmpCurrentResultReference, _tmp_r_throw_rc);                                         \
        return _tmp_r_throw_rc;                                                                    \
    }

/// Returns ResultSuccess()
#define R_SUCCEED() R_RETURN(ResultSuccess)

/// Throws a result.
#define R_THROW(res_expr) R_RETURN(res_expr)

/// Evaluates a boolean expression, and returns a result unless that expression is true.
#define R_UNLESS(expr, res)                                                                        \
    {                                                                                              \
        if (!(expr)) {                                                                             \
            R_THROW(res);                                                                          \
        }                                                                                          \
    }

/// Evaluates an expression that returns a result, and returns the result if it would fail.
#define R_TRY(res_expr)                                                                            \
    {                                                                                              \
        const auto _tmp_r_try_rc = (res_expr);                                                     \
        if (R_FAILED(_tmp_r_try_rc)) {                                                             \
            R_THROW(_tmp_r_try_rc);                                                                \
        }                                                                                          \
    }

/// Evaluates a boolean expression, and succeeds if that expression is true.
#define R_SUCCEED_IF(expr) R_UNLESS(!(expr), ResultSuccess)

#define R_TRY_CATCH(res_expr)                                                                      \
    {                                                                                              \
        const auto R_CURRENT_RESULT = (res_expr);                                                  \
        if (R_FAILED(R_CURRENT_RESULT)) {                                                          \
            if (false)

#define R_END_TRY_CATCH                                                                            \
    else if (R_FAILED(R_CURRENT_RESULT)) {                                                         \
        R_THROW(R_CURRENT_RESULT);                                                                 \
    }                                                                                              \
    }                                                                                              \
    }

#define R_CATCH_ALL()                                                                              \
    }                                                                                              \
    else if (R_FAILED(R_CURRENT_RESULT)) {                                                         \
        if (true)

#define R_CATCH(res_expr)                                                                          \
    }                                                                                              \
    else if ((res_expr) == (R_CURRENT_RESULT)) {                                                   \
        if (true)

#define R_CONVERT(catch_type, convert_type)                                                        \
    R_CATCH(catch_type) {                                                                          \
        R_THROW(static_cast<Result>(convert_type));                                                \
    }

#define R_CONVERT_ALL(convert_type)                                                                \
    R_CATCH_ALL() {                                                                                \
        R_THROW(static_cast<Result>(convert_type));                                                \
    }

#define R_ASSERT(res_expr) ASSERT(R_SUCCEEDED(res_expr))
