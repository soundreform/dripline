#pragma once
#ifndef GD_UI_H
#define GD_UI_H

#include "engine.h"
#include "mapper.h"
#include <cmath>

struct RgbColor {
    float r;
    float g;
    float b;
};

struct UiOutputs {
    RgbColor status;
    RgbColor tempo;
};

struct UiEvents {
    bool boot_bypass_toggled = false;
    bool buffer_cleared = false;
    bool expr_target_changed = false;
    KnobId expr_target = KnobId::UNASSIGNED;
    bool clipping = false;
    bool trails_bypass = false; // Global setting mode
};

class UiManager {
  public:
    UiManager() = default;
    ~UiManager() = default;

    void Init(float control_rate) {
        dt_ = 1.0f / control_rate;
        boot_timer_ = 0.0f;
        panic_timer_ = 0.0f;
        expr_timer_ = 0.0f;
        clip_timer_ = 0.0f;
        lfo_phase_ = 0.0f;
        last_expr_target_ = KnobId::UNASSIGNED;
    }

    UiOutputs Process(const HardwareState &hw, const EngineParams &p, const UiEvents &events, int active_grain_count, float tempo_phase) {
        if (events.boot_bypass_toggled) {
            boot_timer_ = 2.0f;
        }
        if (events.buffer_cleared) {
            panic_timer_ = 0.25f;
        }
        if (events.expr_target_changed) {
            expr_timer_ = 0.5f;
            last_expr_target_ = events.expr_target;
        }
        if (events.clipping) {
            clip_timer_ = 0.05f;
        }

        if (boot_timer_ > 0.0f) {
            boot_timer_ -= dt_;
        }
        if (panic_timer_ > 0.0f) {
            panic_timer_ -= dt_;
        }
        if (expr_timer_ > 0.0f) {
            expr_timer_ -= dt_;
        }
        if (clip_timer_ > 0.0f) {
            clip_timer_ -= dt_;
        }

        lfo_phase_ += dt_ * 10.0f; // 10Hz rapid flasher
        if (lfo_phase_ >= 1.0f) {
            lfo_phase_ -= 1.0f;
        }
        bool flash_state = lfo_phase_ > 0.5f;

        RgbColor status = {0.0f, 0.0f, 0.0f};
        RgbColor tempo = {0.0f, 0.0f, 0.0f};

        // --- Tempo & Rhythm ---
        RgbColor base_tempo_color = GetSubdivisionColor(hw.knobs[static_cast<int>(KnobId::DENSITY)]);

        float brightness = 0.1f + (static_cast<float>(active_grain_count) / 16.0f) * 0.9f;
        float pulse = expf(-5.0f * tempo_phase); // Sharp percussive pulse curve

        tempo.r = base_tempo_color.r * brightness * pulse;
        tempo.g = base_tempo_color.g * brightness * pulse;
        tempo.b = base_tempo_color.b * brightness * pulse;

        // --- Global Panic Override ---
        if (panic_timer_ > 0.0f) {
            return {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
        }

        // --- Status Priorities ---
        if (boot_timer_ > 0.0f) {
            if (flash_state) {
                status = {1.0f, 1.0f, 1.0f};
            }
        } else if (expr_timer_ > 0.0f) {
            if (flash_state) {
                status = GetTargetColor(last_expr_target_);
            }
        } else if (clip_timer_ > 0.0f) {
            status = {1.0f, 0.0f, 0.0f}; // Red clipping
        } else if (p.router.freeze) {
            status = {0.0f, 0.0f, 1.0f}; // Blue freeze
        } else if (hw.bypass_state) {
            status = events.trails_bypass ? RgbColor{0.1f, 0.0f, 0.0f} : RgbColor{0.0f, 0.0f, 0.0f};
        } else {
            status = {0.0f, 1.0f, 0.0f}; // Green active
        }

        return {status, tempo};
    }

  private:
    float dt_;
    float boot_timer_;
    float panic_timer_;
    float expr_timer_;
    float clip_timer_;
    float lfo_phase_;
    KnobId last_expr_target_;

    RgbColor GetSubdivisionColor(float knob_val) const {
        if (knob_val < 0.2f) {
            return {1.0f, 1.0f, 1.0f}; // White
        }
        if (knob_val < 0.4f) {
            return {1.0f, 1.0f, 0.0f}; // Yellow
        }
        if (knob_val < 0.6f) {
            return {0.0f, 1.0f, 1.0f}; // Cyan
        }
        if (knob_val < 0.8f) {
            return {1.0f, 0.0f, 1.0f}; // Magenta
        }
        return {0.0f, 0.0f, 1.0f}; // Blue
    }

    RgbColor GetTargetColor(KnobId target) const {
        switch (target) {
        case KnobId::PITCH:
            return {0.0f, 0.0f, 1.0f};
        case KnobId::SIZE:
            return {0.0f, 1.0f, 0.0f};
        case KnobId::DENSITY:
            return {1.0f, 1.0f, 0.0f};
        case KnobId::TIME_SPRAY:
            return {0.0f, 1.0f, 1.0f};
        case KnobId::PITCH_SPRAY:
            return {1.0f, 0.0f, 1.0f};
        case KnobId::FEEDBACK:
            return {1.0f, 0.5f, 0.0f};
        default:
            return {1.0f, 1.0f, 1.0f};
        }
    }
};

#endif
