// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/glue/time/standard_steady_clock_resource.h"
#include "core/hle/service/psc/time/errors.h"

namespace Service::Glue::Time {
namespace {
[[maybe_unused]] constexpr u32 Max77620PmicSession = 0x3A000001;
[[maybe_unused]] constexpr u32 Max77620RtcSession = 0x3B000001;

Result GetTimeInSeconds(Core::System& system, s64& out_time_s) {
    out_time_s = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count();

    if (Settings::values.custom_rtc_enabled) {
        out_time_s += Settings::values.custom_rtc_offset.GetValue();
    }
    R_SUCCEED();
}
} // namespace

StandardSteadyClockResource::StandardSteadyClockResource(Core::System& system) : m_system{system} {}

void StandardSteadyClockResource::Initialize(Common::UUID* out_source_id,
                                             Common::UUID* external_source_id) {
    constexpr size_t NUM_TRIES{20};

    size_t i{0};
    Result res{ResultSuccess};
    for (; i < NUM_TRIES; i++) {
        res = SetCurrentTime();
        if (res == ResultSuccess) {
            break;
        }
        Kernel::Svc::SleepThread(m_system, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::milliseconds(1))
                                               .count());
    }

    if (i < NUM_TRIES) {
        m_set_time_result = ResultSuccess;
        if (*external_source_id != Service::PSC::Time::ClockSourceId{}) {
            m_clock_source_id = *external_source_id;
        } else {
            m_clock_source_id = Common::UUID::MakeRandom();
        }
    } else {
        m_set_time_result = res;
        auto ticks{m_system.CoreTiming().GetClockTicks()};
        m_time = -Service::PSC::Time::ConvertToTimeSpan(ticks).count();
        m_clock_source_id = Common::UUID::MakeRandom();
    }

    if (out_source_id) {
        *out_source_id = m_clock_source_id;
    }
}

bool StandardSteadyClockResource::GetResetDetected() {
    // TODO:
    // call Rtc::GetRtcResetDetected(Max77620RtcSession)
    // if detected:
    //      SetSys::SetExternalSteadyClockSourceId(invalid_id)
    //      Rtc::ClearRtcResetDetected(Max77620RtcSession)
    // set m_rtc_reset to result
    // Instead, only set reset to true if we're booting for the first time.
    m_rtc_reset = false;
    return m_rtc_reset;
}

Result StandardSteadyClockResource::SetCurrentTime() {
    auto start_tick{m_system.CoreTiming().GetClockTicks()};

    s64 rtc_time_s{};
    // TODO R_TRY(Rtc::GetTimeInSeconds(rtc_time_s, Max77620RtcSession))
    R_TRY(GetTimeInSeconds(m_system, rtc_time_s));

    auto end_tick{m_system.CoreTiming().GetClockTicks()};
    auto diff{Service::PSC::Time::ConvertToTimeSpan(end_tick - start_tick)};
    // Why is this here?
    R_UNLESS(diff < std::chrono::milliseconds(101), Service::PSC::Time::ResultRtcTimeout);

    auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
    s64 boot_time{rtc_time_s * one_second_ns -
                  Service::PSC::Time::ConvertToTimeSpan(end_tick).count()};

    std::scoped_lock l{m_mutex};
    m_time = boot_time;
    R_SUCCEED();
}

Result StandardSteadyClockResource::GetRtcTimeInSeconds(s64& out_time) {
    // TODO
    // R_TRY(Rtc::GetTimeInSeconds(time_s, Max77620RtcSession)
    R_RETURN(GetTimeInSeconds(m_system, out_time));
}

void StandardSteadyClockResource::UpdateTime() {
    constexpr size_t NUM_TRIES{3};

    size_t i{0};
    Result res{ResultSuccess};
    for (; i < NUM_TRIES; i++) {
        res = SetCurrentTime();
        if (res == ResultSuccess) {
            break;
        }
        Kernel::Svc::SleepThread(m_system, std::chrono::duration_cast<std::chrono::nanoseconds>(
                                               std::chrono::milliseconds(1))
                                               .count());
    }
}

} // namespace Service::Glue::Time
