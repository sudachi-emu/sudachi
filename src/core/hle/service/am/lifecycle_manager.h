// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>

#include "core/hle/service/am/am_types.h"
#include "core/hle/service/os/event.h"

namespace Core {
class System;
}

namespace Service::AM {

enum class ActivityState : u32 {
    ForegroundVisible = 0,
    ForegroundObscured = 1,
    BackgroundVisible = 2,
    BackgroundObscured = 3,
};

enum class FocusHandlingMode : u32 {
    AlwaysSuspend = 0,
    SuspendHomeSleep = 1,
    NoSuspend = 2,
};

enum class SuspendMode : u32 {
    NoOverride = 0,
    ForceResume = 1,
    ForceSuspend = 2,
};

class LifecycleManager {
public:
    explicit LifecycleManager(Core::System& system, KernelHelpers::ServiceContext& context,
                              bool is_application);
    ~LifecycleManager();

public:
    Event& GetSystemEvent();
    Event& GetOperationModeChangedSystemEvent();
    Event& GetHDCPStateChangedEvent();

public:
    bool IsApplication() {
        return m_is_application;
    }

    bool GetForcedSuspend() {
        return m_forced_suspend;
    }

    bool GetExitRequested() {
        return m_has_requested_exit;
    }

    ActivityState GetActivityState() {
        return m_activity_state;
    }

    FocusState GetAndClearFocusState() {
        m_acknowledged_focus_state = m_requested_focus_state;
        return m_acknowledged_focus_state;
    }

    void SetFocusState(FocusState state) {
        if (m_requested_focus_state != state) {
            m_has_focus_state_changed = true;
        }
        m_requested_focus_state = state;
        this->SignalSystemEventIfNeeded();
    }

    void RequestExit() {
        m_has_requested_exit = true;
        this->SignalSystemEventIfNeeded();
    }

    void RequestResumeNotification() {
        // NOTE: this appears to be a bug in am.
        // If an applet makes a concurrent request to receive resume notifications
        // while it is being suspended, the first resume notification will be lost.
        // This is not the case with other notification types.
        if (m_resume_notification_enabled) {
            m_has_resume = true;
        }
    }

    void OnOperationAndPerformanceModeChanged();

public:
    void SetFocusStateChangedNotificationEnabled(bool enabled) {
        m_focus_state_changed_notification_enabled = enabled;
        this->SignalSystemEventIfNeeded();
    }

    void SetOperationModeChangedNotificationEnabled(bool enabled) {
        m_operation_mode_changed_notification_enabled = enabled;
        this->SignalSystemEventIfNeeded();
    }

    void SetPerformanceModeChangedNotificationEnabled(bool enabled) {
        m_performance_mode_changed_notification_enabled = enabled;
        this->SignalSystemEventIfNeeded();
    }

    void SetResumeNotificationEnabled(bool enabled) {
        m_resume_notification_enabled = enabled;
    }

    void SetActivityState(ActivityState state) {
        m_activity_state = state;
    }

    void SetSuspendMode(SuspendMode mode) {
        m_suspend_mode = mode;
    }

    void SetForcedSuspend(bool enabled) {
        m_forced_suspend = enabled;
    }

public:
    void SetFocusHandlingMode(bool suspend);
    void SetOutOfFocusSuspendingEnabled(bool enabled);
    void RemoveForceResumeIfPossible();
    bool IsRunnable() const;
    bool UpdateRequestedFocusState();
    void SignalSystemEventIfNeeded();

public:
    void PushUnorderedMessage(AppletMessage message);
    bool PopMessage(AppletMessage* out_message);

private:
    FocusState GetFocusStateWhileForegroundObscured() const;
    FocusState GetFocusStateWhileBackground(bool is_obscured) const;

private:
    AppletMessage PopMessageInOrderOfPriority();
    bool ShouldSignalSystemEvent();

private:
    Event m_system_event;
    Event m_operation_mode_changed_system_event;
    Event m_hdcp_state_changed_event;

    std::list<AppletMessage> m_unordered_messages{};

    bool m_is_application{};
    bool m_focus_state_changed_notification_enabled{true};
    bool m_operation_mode_changed_notification_enabled{true};
    bool m_hdcp_state_changed_notification_enabled{true};
    bool m_performance_mode_changed_notification_enabled{true};
    bool m_resume_notification_enabled{};

    bool m_requested_request_to_display_state{};
    bool m_acknowledged_request_to_display_state{};
    bool m_has_resume{};
    bool m_has_focus_state_changed{true};
    bool m_has_album_recording_saved{};
    bool m_has_album_screen_shot_taken{};
    bool m_has_auto_power_down{};
    bool m_has_sleep_required_by_low_battery{};
    bool m_has_sleep_required_by_high_temperature{};
    bool m_has_sd_card_removed{};
    bool m_has_performance_mode_changed{};
    bool m_has_operation_mode_changed{};
    bool m_has_requested_request_to_prepare_sleep{};
    bool m_has_acknowledged_request_to_prepare_sleep{};
    bool m_has_requested_exit{};
    bool m_has_acknowledged_exit{};
    bool m_applet_message_available{};

    bool m_forced_suspend{};
    FocusHandlingMode m_focus_handling_mode{FocusHandlingMode::SuspendHomeSleep};
    ActivityState m_activity_state{ActivityState::ForegroundVisible};
    SuspendMode m_suspend_mode{SuspendMode::NoOverride};
    FocusState m_requested_focus_state{};
    FocusState m_acknowledged_focus_state{};
};

} // namespace Service::AM
