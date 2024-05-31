// SPDX-FileCopyrightText: Copyright 2024 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "core/hle/service/am/lifecycle_manager.h"

namespace Service::AM {

LifecycleManager::LifecycleManager(Core::System& system, KernelHelpers::ServiceContext& context,
                                   bool is_application)
    : m_system_event(context), m_operation_mode_changed_system_event(context),
      m_hdcp_state_changed_event(context), m_is_application(is_application) {}

LifecycleManager::~LifecycleManager() = default;

Event& LifecycleManager::GetSystemEvent() {
    return m_system_event;
}

Event& LifecycleManager::GetOperationModeChangedSystemEvent() {
    return m_operation_mode_changed_system_event;
}

Event& LifecycleManager::GetHDCPStateChangedEvent() {
    return m_hdcp_state_changed_event;
}

void LifecycleManager::PushUnorderedMessage(AppletMessage message) {
    m_unordered_messages.push_back(message);
    this->SignalSystemEventIfNeeded();
}

AppletMessage LifecycleManager::PopMessageInOrderOfPriority() {
    if (m_has_resume) {
        m_has_resume = false;
        return AppletMessage::Resume;
    }

    if (m_has_acknowledged_exit != m_has_requested_exit) {
        m_has_acknowledged_exit = m_has_requested_exit;
        return AppletMessage::Exit;
    }

    if (m_focus_state_changed_notification_enabled) {
        if (!m_is_application) {
            if (m_requested_focus_state != m_acknowledged_focus_state) {
                m_acknowledged_focus_state = m_requested_focus_state;
                switch (m_requested_focus_state) {
                case FocusState::InFocus:
                    return AppletMessage::ChangeIntoForeground;
                case FocusState::NotInFocus:
                    return AppletMessage::ChangeIntoBackground;
                default:
                    ASSERT(false);
                }
            }
        } else if (m_has_focus_state_changed) {
            m_has_focus_state_changed = false;
            return AppletMessage::FocusStateChanged;
        }
    }

    if (m_has_requested_request_to_prepare_sleep != m_has_acknowledged_request_to_prepare_sleep) {
        m_has_acknowledged_request_to_prepare_sleep = true;
        return AppletMessage::RequestToPrepareSleep;
    }

    if (m_requested_request_to_display_state != m_acknowledged_request_to_display_state) {
        m_acknowledged_request_to_display_state = m_requested_request_to_display_state;
        return AppletMessage::RequestToDisplay;
    }

    if (m_has_operation_mode_changed) {
        m_has_operation_mode_changed = false;
        return AppletMessage::OperationModeChanged;
    }

    if (m_has_performance_mode_changed) {
        m_has_performance_mode_changed = false;
        return AppletMessage::PerformanceModeChanged;
    }

    if (m_has_sd_card_removed) {
        m_has_sd_card_removed = false;
        return AppletMessage::SdCardRemoved;
    }

    if (m_has_sleep_required_by_high_temperature) {
        m_has_sleep_required_by_high_temperature = false;
        return AppletMessage::SleepRequiredByHighTemperature;
    }

    if (m_has_sleep_required_by_low_battery) {
        m_has_sleep_required_by_low_battery = false;
        return AppletMessage::SleepRequiredByLowBattery;
    }

    if (m_has_auto_power_down) {
        m_has_auto_power_down = false;
        return AppletMessage::AutoPowerDown;
    }

    if (m_has_album_screen_shot_taken) {
        m_has_album_screen_shot_taken = false;
        return AppletMessage::AlbumScreenShotTaken;
    }

    if (m_has_album_recording_saved) {
        m_has_album_recording_saved = false;
        return AppletMessage::AlbumRecordingSaved;
    }

    if (!m_unordered_messages.empty()) {
        const auto message = m_unordered_messages.front();
        m_unordered_messages.pop_front();
        return message;
    }

    return AppletMessage::None;
}

bool LifecycleManager::ShouldSignalSystemEvent() {
    if (m_focus_state_changed_notification_enabled) {
        if (!m_is_application) {
            if (m_requested_focus_state != m_acknowledged_focus_state) {
                return true;
            }
        } else if (m_has_focus_state_changed) {
            return true;
        }
    }

    return !m_unordered_messages.empty() || m_has_resume ||
           (m_has_requested_exit != m_has_acknowledged_exit) ||
           (m_has_requested_request_to_prepare_sleep !=
            m_has_acknowledged_request_to_prepare_sleep) ||
           m_has_operation_mode_changed || m_has_performance_mode_changed ||
           m_has_sd_card_removed || m_has_sleep_required_by_high_temperature ||
           m_has_sleep_required_by_low_battery || m_has_auto_power_down ||
           (m_requested_request_to_display_state != m_acknowledged_request_to_display_state) ||
           m_has_album_screen_shot_taken || m_has_album_recording_saved;
}

void LifecycleManager::OnOperationAndPerformanceModeChanged() {
    if (m_operation_mode_changed_notification_enabled) {
        m_has_operation_mode_changed = true;
    }
    if (m_performance_mode_changed_notification_enabled) {
        m_has_performance_mode_changed = true;
    }
    m_operation_mode_changed_system_event.Signal();
    this->SignalSystemEventIfNeeded();
}

void LifecycleManager::SignalSystemEventIfNeeded() {
    // Check our cached value for the system event.
    const bool applet_message_available = m_applet_message_available;

    // If it's not current, we need to do an update, either clearing or signaling.
    if (applet_message_available != this->ShouldSignalSystemEvent()) {
        if (!applet_message_available) {
            m_system_event.Signal();
            m_applet_message_available = true;
        } else {
            m_system_event.Clear();
            m_applet_message_available = false;
        }
    }
}

bool LifecycleManager::PopMessage(AppletMessage* out_message) {
    const auto message = this->PopMessageInOrderOfPriority();
    this->SignalSystemEventIfNeeded();

    *out_message = message;
    return message != AppletMessage::None;
}

void LifecycleManager::SetFocusHandlingMode(bool suspend) {
    switch (m_focus_handling_mode) {
    case FocusHandlingMode::AlwaysSuspend:
    case FocusHandlingMode::SuspendHomeSleep:
        if (!suspend) {
            // Disallow suspension.
            m_focus_handling_mode = FocusHandlingMode::NoSuspend;
        }
        break;
    case FocusHandlingMode::NoSuspend:
        if (suspend) {
            // Allow suspension temporally.
            m_focus_handling_mode = FocusHandlingMode::SuspendHomeSleep;
        }
        break;
    }
}

void LifecycleManager::SetOutOfFocusSuspendingEnabled(bool enabled) {
    switch (m_focus_handling_mode) {
    case FocusHandlingMode::AlwaysSuspend:
        if (!enabled) {
            // Allow suspension temporally.
            m_focus_handling_mode = FocusHandlingMode::SuspendHomeSleep;
        }
        break;
    case FocusHandlingMode::SuspendHomeSleep:
    case FocusHandlingMode::NoSuspend:
        if (enabled) {
            // Allow suspension.
            m_focus_handling_mode = FocusHandlingMode::AlwaysSuspend;
        }
        break;
    }
}

void LifecycleManager::RemoveForceResumeIfPossible() {
    // If resume is not forced, we have nothing to do.
    if (m_suspend_mode != SuspendMode::ForceResume) {
        return;
    }

    // Check activity state.
    // If we are already resumed, we can remove the forced state.
    switch (m_activity_state) {
    case ActivityState::ForegroundVisible:
    case ActivityState::ForegroundObscured:
        m_suspend_mode = SuspendMode::NoOverride;
        return;

    default:
        break;
    }

    // Check focus handling mode.
    switch (m_focus_handling_mode) {
    case FocusHandlingMode::AlwaysSuspend:
    case FocusHandlingMode::SuspendHomeSleep:
        // If the applet allows suspension, we can remove the forced state.
        m_suspend_mode = SuspendMode::NoOverride;
        break;

    case FocusHandlingMode::NoSuspend:
        // If the applet is not an application, we can remove the forced state.
        // Only applications can be forced to resume.
        if (!m_is_application) {
            m_suspend_mode = SuspendMode::NoOverride;
        }
    }
}

bool LifecycleManager::IsRunnable() const {
    // If suspend is forced, return that.
    if (m_forced_suspend) {
        return false;
    }

    // Check suspend mode override.
    switch (m_suspend_mode) {
    case SuspendMode::NoOverride:
        // Continue processing.
        break;

    case SuspendMode::ForceResume:
        // The applet is runnable during forced resumption when its exit is requested.
        return m_has_requested_exit;

    case SuspendMode::ForceSuspend:
        // The applet is never runnable during forced suspension.
        return false;
    }

    // Always run if exit is requested.
    if (m_has_requested_exit) {
        return true;
    }

    if (m_activity_state == ActivityState::ForegroundVisible) {
        // The applet is runnable now.
        return true;
    }

    if (m_activity_state == ActivityState::ForegroundObscured) {
        switch (m_focus_handling_mode) {
        case FocusHandlingMode::AlwaysSuspend:
            // The applet is not runnable while running the applet.
            return false;

        case FocusHandlingMode::SuspendHomeSleep:
            // The applet is runnable while running the applet.
            return true;

        case FocusHandlingMode::NoSuspend:
            // The applet is always runnable.
            return true;
        }
    }

    // The activity is a suspended one.
    // The applet should be suspended unless it has disabled suspension.
    return m_focus_handling_mode == FocusHandlingMode::NoSuspend;
}

FocusState LifecycleManager::GetFocusStateWhileForegroundObscured() const {
    switch (m_focus_handling_mode) {
    case FocusHandlingMode::AlwaysSuspend:
        // The applet never learns it has lost focus.
        return FocusState::InFocus;

    case FocusHandlingMode::SuspendHomeSleep:
        // The applet learns it has lost focus when launching a child applet.
        return FocusState::NotInFocus;

    case FocusHandlingMode::NoSuspend:
        // The applet always learns it has lost focus.
        return FocusState::NotInFocus;

    default:
        UNREACHABLE();
    }
}

FocusState LifecycleManager::GetFocusStateWhileBackground(bool is_obscured) const {
    switch (m_focus_handling_mode) {
    case FocusHandlingMode::AlwaysSuspend:
        // The applet never learns it has lost focus.
        return FocusState::InFocus;

    case FocusHandlingMode::SuspendHomeSleep:
        // The applet learns it has lost focus when launching a child applet.
        return is_obscured ? FocusState::NotInFocus : FocusState::InFocus;

    case FocusHandlingMode::NoSuspend:
        // The applet always learns it has lost focus.
        return m_is_application ? FocusState::Background : FocusState::NotInFocus;

    default:
        UNREACHABLE();
    }
}

bool LifecycleManager::UpdateRequestedFocusState() {
    FocusState new_state{};

    if (m_suspend_mode == SuspendMode::NoOverride) {
        // With no forced suspend or resume, we take the focus state designated
        // by the combination of the activity flag and the focus handling mode.
        switch (m_activity_state) {
        case ActivityState::ForegroundVisible:
            new_state = FocusState::InFocus;
            break;

        case ActivityState::ForegroundObscured:
            new_state = this->GetFocusStateWhileForegroundObscured();
            break;

        case ActivityState::BackgroundVisible:
            new_state = this->GetFocusStateWhileBackground(false);
            break;

        case ActivityState::BackgroundObscured:
            new_state = this->GetFocusStateWhileBackground(true);
            break;

        default:
            UNREACHABLE();
        }
    } else {
        // With forced suspend or resume, the applet is guaranteed to be background.
        new_state = this->GetFocusStateWhileBackground(false);
    }

    if (new_state != m_requested_focus_state) {
        // Mark the focus state as ready for update.
        m_requested_focus_state = new_state;

        // We changed the focus state.
        return true;
    }

    // We didn't change the focus state.
    return false;
}

} // namespace Service::AM
