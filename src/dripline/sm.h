#pragma once
#ifndef DL_STATEMACHINE_H
#define DL_STATEMACHINE_H

#include <functional>
#include <map>
#include <vector>

namespace dripline {
template <typename TState, typename TEvent>
class StateMachine {
  public:
    using Callback = std::function<void()>;

    void Init(TState initial) {
        current_state_ = initial;
        transitions_.clear();
        on_enter_callbacks_.clear();
        on_exit_callbacks_.clear();
    }

    void AddTransition(TEvent event, TState src, TState dest) {
        transitions_[event][src] = dest;
    }

    void OnEnter(TState state, Callback cb) {
        on_enter_callbacks_[state].push_back(cb);
    }

    void OnExit(TState state, Callback cb) {
        on_exit_callbacks_[state].push_back(cb);
    }

    void Trigger(TEvent event) {
        auto event_transitions_it = transitions_.find(event);
        if (event_transitions_it == transitions_.end()) {
            return;
        }

        auto state_transition_it = event_transitions_it->second.find(current_state_);
        if (state_transition_it == event_transitions_it->second.end()) {
            return;
        }

        TransitionTo(state_transition_it->second);
    }

    void TransitionTo(TState dest_state) {
        if (on_exit_callbacks_.count(current_state_) > 0) {
            for (const auto &cb : on_exit_callbacks_.at(current_state_)) {
                cb();
            }
        }

        if (on_enter_callbacks_.count(dest_state) > 0) {
            for (const auto &cb : on_enter_callbacks_.at(dest_state)) {
                cb();
            }
        }

        current_state_ = dest_state;
    }

    TState GetState() const {
        return current_state_;
    }

  private:
    TState current_state_;
    std::map<TEvent, std::map<TState, TState>> transitions_;
    std::map<TState, std::vector<Callback>> on_enter_callbacks_;
    std::map<TState, std::vector<Callback>> on_exit_callbacks_;
};
}; // namespace dripline

#endif
