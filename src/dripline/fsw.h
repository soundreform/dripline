#pragma once
#ifndef DL_FOOTSWITCH_H
#define DL_FOOTSWITCH_H

#include "daisy.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <vector>

using daisy::Switch;

namespace dripline {

/**
 * @brief A high-level abstraction for footswitch gestures.
 *
 * This class translates raw switch presses into meaningful UI gestures like
 * taps, holds, and momentary actions, simplifying the implementation of
 * complex user interactions.
 */
class Footswitch {
  public:
    using Callback = std::function<void()>;

    // A one-shot action that fires on release after a specific hold duration.
    struct Hold {
        float min_duration_ms;
        float max_duration_ms;
        Callback callback;
    };

    Footswitch() = default;
    ~Footswitch() = default;

    /** @brief Initializes the Footswitch with a pointer to a daisy::Switch object. */
    void Init(Switch *sw) {
        sw_ = sw;
    }

    /**
     * @brief Checks for switch events and fires registered callbacks.
     * This should be called in a loop after the underlying Switch has been debounced.
     */
    void Process() {
        // On press, reset all our state-tracking flags for this press cycle.
        if (sw_->RisingEdge()) {
            momentary_action_was_active_ = false;
            for (auto &m : momentary_cbs_) {
                m.is_active = false;
            }
            if (on_press_) {
                on_press_();
            }
        }

        // Check for momentary actions starting while the switch is held.
        if (sw_->Pressed()) {
            float held_duration_ms = sw_->TimeHeldMs();
            // Callbacks are sorted longest to shortest.
            for (auto &m : momentary_cbs_) {
                if (held_duration_ms >= m.start_duration_ms && !m.is_active) {
                    if (m.on_start) {
                        m.on_start();
                    }
                    m.is_active = true;
                    momentary_action_was_active_ = true;
                    break; // Only one momentary action can start per press.
                }
            }
        }

        // On release, arbitrate between momentary end, hold, and tap.
        if (sw_->FallingEdge()) {
            float held_duration_ms = sw_->TimeHeldMs();

            // 1. Highest priority: End any active momentary action.
            if (momentary_action_was_active_) {
                // Find the active momentary action and fire its on_end callback.
                for (auto &m : momentary_cbs_) {
                    if (m.is_active) {
                        if (m.on_end) {
                            m.on_end();
                        }
                        return; // This was the event for this release, so we're done.
                    }
                }
            }

            // 2. If not a momentary action, check for a timed hold action.
            // Callbacks are sorted longest to shortest.
            for (const auto &h : hold_cbs_) {
                if (held_duration_ms >= h.min_duration_ms && held_duration_ms < h.max_duration_ms) {
                    if (h.callback) {
                        h.callback();
                    }
                    return; // This was the event, so we're done.
                }
            }

            // 3. If no hold or momentary action fired, it's a tap.
            if (on_tap_) {
                on_tap_();
            }
        }
    }

    /** @brief Fires immediately when the switch is physically pressed (rising edge). */
    void OnPress(Callback cb) {
        on_press_ = cb;
    }

    /** @brief Fires on release if the press was shorter than any defined hold. */
    void OnTap(Callback cb) {
        on_tap_ = cb;
    }

    /** @brief Fires on release after holding for a duration within the specified range. */
    void OnHold(float min_duration_ms, float max_duration_ms, Callback cb) {
        hold_cbs_.push_back({min_duration_ms, max_duration_ms, cb});
        // Sort by min_duration descending to check longest holds first.
        std::sort(hold_cbs_.begin(), hold_cbs_.end(), [](const Hold &a, const Hold &b) { return a.min_duration_ms > b.min_duration_ms; });
    }

    /** @brief Convenience overload for a hold that starts at a specific time and has no max. */
    void OnHold(float min_duration_ms, Callback cb) {
        OnHold(min_duration_ms, std::numeric_limits<float>::max(), cb);
    }

    /**
     * @brief Defines a momentary action. Fires `on_start` when held for `start_duration_ms`,
     * and fires `on_end` when released *after* `on_start` has fired.
     */
    void OnMomentary(float start_duration_ms, Callback on_start, Callback on_end) {
        momentary_cbs_.push_back({start_duration_ms, on_start, on_end, false});
        // Sort by start_duration descending to check longest holds first.
        std::sort(momentary_cbs_.begin(), momentary_cbs_.end(), [](const Momentary &a, const Momentary &b) { return a.start_duration_ms > b.start_duration_ms; });
    }

  private:
    // An action that starts while held and ends on release.
    struct Momentary {
        float start_duration_ms;
        Callback on_start;
        Callback on_end;
        bool is_active; // State for the current press cycle.
    };

    Switch *sw_;
    bool momentary_action_was_active_ = false;

    Callback on_press_ = nullptr;
    Callback on_tap_ = nullptr;

    std::vector<Hold> hold_cbs_;
    std::vector<Momentary> momentary_cbs_;
};

} // namespace dripline

#endif // DL_FOOTSWITCH_H
