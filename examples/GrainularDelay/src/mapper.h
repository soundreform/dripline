#pragma once
#ifndef GD_MAPPER_H
#define GD_MAPPER_H

#include "engine.h"
#include "window.h"
#include <cmath>

enum class KnobId : int {
    PITCH = 0,
    SIZE = 1,
    DENSITY = 2,
    TIME_SPRAY = 3,
    PITCH_SPRAY = 4,
    PAN_SPREAD = 5,
    FEEDBACK = 6,
    DRIVE = 7,
    TONE = 8,
    EXPRESSION = 9,
    OUTPUT = 10,
    MIX = 11,
    UNASSIGNED = -1
};

enum class SwitchId : int {
    ROUTING = 0,
    QUANTIZE = 1,
    DIRECTION = 2,
    DEPTH = 3,
    WINDOW = 4,
    RHYTHM = 5
};

struct HardwareState {
    float knobs[12];     // 0.0 to 1.0 smoothed ADC values
    int switches[6];     // 0: DOWN, 1: CENTER, 2: UP
    float expression;    // 0.0 to 1.0 smoothed expression pedal
    bool fs_1_held;      // Momentary freeze state
    bool fs_2_held;      // Used for expression target selection
    int expr_target_bin; // 0 to 6
    float tap_tempo_hz;  // > 0.0 if a tapped tempo is active
    bool bypass_state;   // Current active vs bypassed state
};

class Mapper {
  public:
    Mapper() = default;
    ~Mapper() = default;

    void Init(float sample_rate) {
        sample_rate_ = sample_rate;
        expr_target_ = KnobId::UNASSIGNED;
        window_table_.Init();
    }

    bool Map(const HardwareState &hw, EngineParams &p) {
        bool new_assignment = CheckExpressionAssignment(hw);

        float k_pitch = GetEffectiveKnob(hw, KnobId::PITCH);
        float k_size = GetEffectiveKnob(hw, KnobId::SIZE);
        float k_density = GetEffectiveKnob(hw, KnobId::DENSITY);
        float k_time_spray = GetEffectiveKnob(hw, KnobId::TIME_SPRAY);
        float k_pitch_spray = GetEffectiveKnob(hw, KnobId::PITCH_SPRAY);
        float k_pan_spread = GetEffectiveKnob(hw, KnobId::PAN_SPREAD);
        float k_feedback = GetEffectiveKnob(hw, KnobId::FEEDBACK);
        float k_drive = GetEffectiveKnob(hw, KnobId::DRIVE);
        float k_tone = GetEffectiveKnob(hw, KnobId::TONE);
        float k_out_lvl = GetEffectiveKnob(hw, KnobId::OUTPUT);
        float k_mix = GetEffectiveKnob(hw, KnobId::MIX);

        int s_routing = GetSwitch(hw, SwitchId::ROUTING);
        int s_quantize = GetSwitch(hw, SwitchId::QUANTIZE);
        int s_direction = GetSwitch(hw, SwitchId::DIRECTION);
        int s_depth = GetSwitch(hw, SwitchId::DEPTH);
        int s_window = GetSwitch(hw, SwitchId::WINDOW);
        int s_rhythm = GetSwitch(hw, SwitchId::RHYTHM);

        p.grain.duration = (0.01f + (k_size * 0.49f)) * sample_rate_;
        p.grain.time_spray = (k_time_spray * 0.5f) * sample_rate_;
        p.grain.pitch_spray = k_pitch_spray;
        p.grain.pan_spread = k_pan_spread;
        p.grain.quantize_mode = (s_quantize == 2) ? QuantizeMode::FREE : (s_quantize == 1) ? QuantizeMode::SEMITONES : QuantizeMode::OCTAVES_FIFTHS;
        p.grain.delay_min = sample_rate_ * 0.25f;
        p.grain.pitch = GetPitch(s_direction, k_pitch);
        p.grain.random_direction = (s_direction == 1);

        p.grain.window.size = WindowTable::kTableSize;
        if (s_window == 2) {
            p.grain.window.table = window_table_.GetPluck();
        } else if (s_window == 0) {
            p.grain.window.table = window_table_.GetSwell();
        } else {
            p.grain.window.table = window_table_.GetSmooth();
        }

        p.router.feedback = k_feedback * 0.85f;
        p.router.drive = k_drive * 2.0f;
        p.router.tone = k_tone;
        p.router.output = k_out_lvl * 2.0f;
        p.router.mix = k_mix;
        p.router.freeze = hw.fs_1_held;
        p.router.trails_bypass = hw.bypass_state;
        p.router.mode = (s_routing == 2) ? RouterMode::PARALLEL : (s_routing == 1) ? RouterMode::PING_PONG : RouterMode::SERIES;

        p.mod.depth = (s_depth == 2) ? ModDepth::SOME : (s_depth == 1) ? ModDepth::NONE : ModDepth::MORE;

        p.scheduler.mode = (s_rhythm == 2) ? SchedulerMode::FREE : (s_rhythm == 1) ? SchedulerMode::SYNCED : SchedulerMode::BURST;
        p.scheduler.density = GetDensity(hw.tap_tempo_hz, k_density);

        return new_assignment;
    }

    KnobId GetExpressionTarget() const {
        return expr_target_;
    }

  private:
    float sample_rate_;
    KnobId expr_target_;
    WindowTable window_table_;

    bool CheckExpressionAssignment(const HardwareState &hw) {
        bool assigned = false;
        if (hw.fs_2_held) {
            const KnobId targets[7] = {
                KnobId::UNASSIGNED, KnobId::PITCH, KnobId::SIZE, KnobId::DENSITY, KnobId::TIME_SPRAY, KnobId::PITCH_SPRAY, KnobId::FEEDBACK,
            };

            int bin = hw.expr_target_bin;
            if (bin < 0) {
                bin = 0;
            }
            if (bin > 6) {
                bin = 6;
            }

            KnobId new_target = targets[bin];

            if (expr_target_ != new_target) {
                expr_target_ = new_target;
                assigned = true;
            }
        }
        return assigned;
    }

    float GetEffectiveKnob(const HardwareState &hw, KnobId index) const {
        int idx = static_cast<int>(index);
        float val = hw.knobs[idx];
        // If FS2 is held, we are selecting the expression target, so bypass the
        // expression offset
        if (index == expr_target_ && !hw.fs_2_held) {
            float depth = (hw.knobs[static_cast<int>(KnobId::EXPRESSION)] - 0.5f) * 2.0f; // Scale 0..1 to -1.0..1.0
            val += hw.expression * depth;
            if (val < 0.0f) {
                val = 0.0f;
            }
            if (val > 1.0f) {
                val = 1.0f;
            }
        }
        return val;
    }

    int GetSwitch(const HardwareState &hw, SwitchId index) const {
        return hw.switches[static_cast<int>(index)];
    }

    float GetDensity(float tempo_hz, float knob_val) const {
        if (tempo_hz > 0.0f) {
            return tempo_hz * GetSubdivisionMultiplier(knob_val);
        } else {
            return expf(knob_val * 4.60517f); // 1Hz to 100Hz exponential curve
        }
    }

    float GetSubdivisionMultiplier(float knob_val) const {
        if (knob_val < 0.2f) {
            return 1.0f; // 1/4
        }
        if (knob_val < 0.4f) {
            return 1.5f; // Dotted 1/8
        }
        if (knob_val < 0.6f) {
            return 2.0f; // 1/8
        }
        if (knob_val < 0.8f) {
            return 3.0f; // Triplet 1/8
        }
        return 4.0f; // 1/16
    }

    float GetPitch(int direction, float knob_val) const {
        float pitch = (knob_val * 4.0f) - 2.0f;
        if (direction == 0) {
            return -fabsf(pitch);
        }
        return fabsf(pitch);
    }
};

#endif
