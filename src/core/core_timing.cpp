// SPDX-FileCopyrightText: Copyright 2020 sudachi Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <mutex>
#include <string>
#include <tuple>

#ifdef _WIN32
#include "common/windows/timer_resolution.h"
#endif

#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_wait.h"
#endif

#include "core/core_timing.h"
#include "core/hardware_properties.h"

namespace Core::Timing {

constexpr s64 MAX_SLICE_LENGTH = 10000;

std::shared_ptr<EventType> CreateEvent(std::string name, TimedCallback&& callback) {
    return std::make_shared<EventType>(std::move(callback), std::move(name));
}

struct CoreTiming::Event {
    s64 time;
    u64 fifo_order;
    std::weak_ptr<EventType> type;
    s64 reschedule_time;
    heap_t::handle_type handle{};

    // Sort by time, unless the times are the same, in which case sort by
    // the order added to the queue
    friend bool operator>(const Event& left, const Event& right) {
        return std::tie(left.time, left.fifo_order) > std::tie(right.time, right.fifo_order);
    }

    friend bool operator<(const Event& left, const Event& right) {
        return std::tie(left.time, left.fifo_order) < std::tie(right.time, right.fifo_order);
    }
};

CoreTiming::CoreTiming() : clock{Common::CreateOptimalClock()} {}

CoreTiming::~CoreTiming() {
    Reset();
}

void CoreTiming::ThreadEntry(CoreTiming& instance) {
    static constexpr char name[] = "HostTiming";
    Common::SetCurrentThreadName(name);
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    instance.on_thread_init();
    instance.ThreadLoop();
}

void CoreTiming::Initialize(std::function<void()>&& on_thread_init_) {
    Reset();
    on_thread_init = std::move(on_thread_init_);
    event_fifo_id = 0;
    shutting_down = false;
    cpu_ticks = 0;
    if (is_multicore) {
        timer_thread = std::make_unique<std::jthread>(ThreadEntry, std::ref(*this));
    }
}

void CoreTiming::ClearPendingEvents() {
    std::scoped_lock lock{advance_lock, basic_lock};
    event_queue.clear();
    event.Set();
}

void CoreTiming::Pause(bool is_paused) {
    paused = is_paused;
    pause_event.Set();

    if (!is_paused) {
        pause_end_time = GetGlobalTimeNs().count();
    }
}

void CoreTiming::SyncPause(bool is_paused) {
    if (is_paused == paused && paused_set == paused) {
        return;
    }

    Pause(is_paused);
    if (timer_thread) {
        if (!is_paused) {
            pause_event.Set();
        }
        event.Set();
        while (paused_set != is_paused)
            ;
    }

    if (!is_paused) {
        pause_end_time = GetGlobalTimeNs().count();
    }
}

bool CoreTiming::IsRunning() const {
    return !paused_set;
}

bool CoreTiming::HasPendingEvents() const {
    std::scoped_lock lock{basic_lock};
    return !(wait_set && event_queue.empty());
}

void CoreTiming::ScheduleEvent(std::chrono::nanoseconds ns_into_future,
                               const std::shared_ptr<EventType>& event_type, bool absolute_time) {
    {
        std::scoped_lock scope{basic_lock};
        const auto next_time{absolute_time ? ns_into_future : GetGlobalTimeNs() + ns_into_future};

        auto h{event_queue.emplace(Event{next_time.count(), event_fifo_id++, event_type, 0})};
        (*h).handle = h;
    }

    event.Set();
}

void CoreTiming::ScheduleLoopingEvent(std::chrono::nanoseconds start_time,
                                      std::chrono::nanoseconds resched_time,
                                      const std::shared_ptr<EventType>& event_type,
                                      bool absolute_time) {
    {
        std::scoped_lock scope{basic_lock};
        const auto next_time{absolute_time ? start_time : GetGlobalTimeNs() + start_time};

        auto h{event_queue.emplace(
            Event{next_time.count(), event_fifo_id++, event_type, resched_time.count()})};
        (*h).handle = h;
    }

    event.Set();
}

void CoreTiming::UnscheduleEvent(const std::shared_ptr<EventType>& event_type,
                                 UnscheduleEventType type) {
    {
        std::scoped_lock lk{basic_lock};

        std::vector<heap_t::handle_type> to_remove;
        for (auto itr = event_queue.begin(); itr != event_queue.end(); itr++) {
            const Event& e = *itr;
            if (e.type.lock().get() == event_type.get()) {
                to_remove.push_back(itr->handle);
            }
        }

        for (auto& h : to_remove) {
            event_queue.erase(h);
        }

        event_type->sequence_number++;
    }

    // Force any in-progress events to finish
    if (type == UnscheduleEventType::Wait) {
        std::scoped_lock lk{advance_lock};
    }
}

void CoreTiming::AddTicks(u64 ticks_to_add) {
    cpu_ticks += ticks_to_add;
    downcount -= static_cast<s64>(cpu_ticks);
}

void CoreTiming::Idle() {
    cpu_ticks += 1000U;
}

void CoreTiming::ResetTicks() {
    downcount = MAX_SLICE_LENGTH;
}

u64 CoreTiming::GetClockTicks() const {
    if (is_multicore) [[likely]] {
        return clock->GetCNTPCT();
    }
    return Common::WallClock::CPUTickToCNTPCT(cpu_ticks);
}

u64 CoreTiming::GetGPUTicks() const {
    if (is_multicore) [[likely]] {
        return clock->GetGPUTick();
    }
    return Common::WallClock::CPUTickToGPUTick(cpu_ticks);
}

std::optional<s64> CoreTiming::Advance() {
    std::scoped_lock lock{advance_lock, basic_lock};
    global_timer = GetGlobalTimeNs().count();

    while (!event_queue.empty() && event_queue.top().time <= global_timer) {
        const Event& evt = event_queue.top();

        if (const auto event_type{evt.type.lock()}) {
            const auto evt_time = evt.time;
            const auto evt_sequence_num = event_type->sequence_number;

            if (evt.reschedule_time == 0) {
                event_queue.pop();

                basic_lock.unlock();

                event_type->callback(
                    evt_time, std::chrono::nanoseconds{GetGlobalTimeNs().count() - evt_time});

                basic_lock.lock();
            } else {
                basic_lock.unlock();

                const auto new_schedule_time{event_type->callback(
                    evt_time, std::chrono::nanoseconds{GetGlobalTimeNs().count() - evt_time})};

                basic_lock.lock();

                if (evt_sequence_num != event_type->sequence_number) {
                    // Heap handle is invalidated after external modification.
                    continue;
                }

                const auto next_schedule_time{new_schedule_time.has_value()
                                                  ? new_schedule_time.value().count()
                                                  : evt.reschedule_time};

                // If this event was scheduled into a pause, its time now is going to be way
                // behind. Re-set this event to continue from the end of the pause.
                auto next_time{evt.time + next_schedule_time};
                if (evt.time < pause_end_time) {
                    next_time = pause_end_time + next_schedule_time;
                }

                event_queue.update(evt.handle, Event{next_time, event_fifo_id++, evt.type,
                                                     next_schedule_time, evt.handle});
            }
        }

        global_timer = GetGlobalTimeNs().count();
    }

    if (!event_queue.empty()) {
        return event_queue.top().time;
    } else {
        return std::nullopt;
    }
}

void CoreTiming::ThreadLoop() {
    has_started = true;
    while (!shutting_down) {
        while (!paused) {
            paused_set = false;
            const auto next_time = Advance();
            if (next_time) {
                // There are more events left in the queue, wait until the next event.
                auto wait_time = *next_time - GetGlobalTimeNs().count();
                if (wait_time > 0) {
#ifdef _WIN32
                    while (!paused && !event.IsSet() && wait_time > 0) {
                        wait_time = *next_time - GetGlobalTimeNs().count();
                        if (wait_time >= timer_resolution_ns) {
                            Common::Windows::SleepForOneTick();
                        } else {
#ifdef ARCHITECTURE_x86_64
                            Common::X64::MicroSleep();
#else
                            std::this_thread::yield();
#endif
                        }
                    }

                    if (event.IsSet()) {
                        event.Reset();
                    }
#else
                    event.WaitFor(std::chrono::nanoseconds(wait_time));
#endif
                }
            } else {
                // Queue is empty, wait until another event is scheduled and signals us to
                // continue.
                wait_set = true;
                event.Wait();
            }
            wait_set = false;
        }

        paused_set = true;
        pause_event.Wait();
    }
}

void CoreTiming::Reset() {
    paused = true;
    shutting_down = true;
    pause_event.Set();
    event.Set();
    if (timer_thread) {
        timer_thread->join();
    }
    timer_thread.reset();
    has_started = false;
}

std::chrono::nanoseconds CoreTiming::GetGlobalTimeNs() const {
    if (is_multicore) [[likely]] {
        return clock->GetTimeNS();
    }
    return std::chrono::nanoseconds{Common::WallClock::CPUTickToNS(cpu_ticks)};
}

std::chrono::microseconds CoreTiming::GetGlobalTimeUs() const {
    if (is_multicore) [[likely]] {
        return clock->GetTimeUS();
    }
    return std::chrono::microseconds{Common::WallClock::CPUTickToUS(cpu_ticks)};
}

#ifdef _WIN32
void CoreTiming::SetTimerResolutionNs(std::chrono::nanoseconds ns) {
    timer_resolution_ns = ns.count();
}
#endif

} // namespace Core::Timing
