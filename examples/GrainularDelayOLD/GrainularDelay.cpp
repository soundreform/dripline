#include "arm_math.h"
#include "bypass.h"
#include "daisy.h"
#include "daisysp.h"
#include "dripline.h"
#include "qparameter.h"
#include "tap.h"

#define MAX_DELAY 96000
#define NUM_GRAINS 16
#define WIND_SIZE 4096

using daisy::AudioHandle;
using daisy::SaiHandle;
using daisysp::CrossFade;
using daisysp::DelayLine;
using daisysp::fastlog2f;
using daisysp::fclamp;
using daisysp::fonepole;
using daisysp::kOneTwelfth;
using daisysp::SoftLimit;
using dripline::AudioBypass;
using dripline::DigitalControl;
using dripline::Dripline;
using dripline::QParameter;

enum class RoutingMode
{
    PARALLEL,
    PING_PONG,
    SERIES,
};

enum class RhythmMode
{
    FREE,
    SYNCED,
    BURST,
};

enum class Direction
{
    FORWARD,
    RANDOM,
    REVERSE,
};

enum class PitchQuantizeMode
{
    FREE,
    SEMITONES,
    OCTAVES_FIFTHS,
};

enum class WindowShape
{
    PLUCK,
    SMOOTH,
    SWELL,
};

enum class DensitySubdivision
{
    QUARTER_NOTES,        // 1x
    DOTTED_EIGHTH,        // 1.5x
    EIGHTH_NOTES,         // 2x
    EIGHTH_NOTE_TRIPLETS, // 3x
    SIXTEENTH_NOTES,      // 4x
};

struct Settings
{
    bool true_bypass;
    bool operator==(const Settings &other) const { return true_bypass == other.true_bypass; }
    bool operator!=(const Settings &other) const { return !(*this == other); }
};

/**
 * @brief High-level parameters for the granular engine.
 * Decouples hardware mapping from DSP logic.
 */
struct GranularSettings
{
    float pitch;
    float grain_size;
    float density;
    float time_spray;
    float pitch_spray;
    float pan_spread;
    float feedback;
    float drive;
    float tone;
    float output_level;
    float mix;
    RoutingMode routing;
    PitchQuantizeMode quantize;
    Direction direction;
    WindowShape window;
    RhythmMode rhythm;
    bool freeze;
};

Dripline hw;
daisy::PersistentStorage<Settings> storage(hw.seed.qspi);

/**
 * @brief Represents a single granular voice.
 * Handles phase accumulation, windowing, and stereo spatialization
 * using Hermite interpolation for high-fidelity pitch shifting.
 */
template <size_t max_delay, size_t wind_size>
class Grain
{
public:
    Grain() = default;
    ~Grain() = default;

    struct Output
    {
        float L;
        float R;
    };

    void Trigger(float pos_samples, float duration_samples, float pitch, float pan, const float *window_table_ptr)
    {
        pos_ = pos_samples;
        duration_ = duration_samples;
        phase_ = 0.0f;
        phase_inc_ = 1.0f / duration_samples;
        pitch_ = pitch;
        active_ = true;

        float angle = pan * HALFPI_F;
        pan_L_ = arm_cos_f32(angle);
        pan_R_ = arm_sin_f32(angle);
        window_table_ptr_ = window_table_ptr;
    }

    Output Process(DelayLine<float, max_delay> *buf_L, DelayLine<float, max_delay> *buf_R)
    {
        if (!active_)
        {
            return {0.0f, 0.0f};
        }

        // Calculate current read position within the delay buffer
        float read_pos = pos_ + (phase_ * duration_ * pitch_);

        // Fetch samples using Hermite (cubic) interpolation to prevent aliasing/muffling
        float L = buf_L->ReadHermite(read_pos);
        float R = buf_R->ReadHermite(read_pos);

        // Perform linear interpolation on the window lookup table
        float idx_f = phase_ * (float)wind_size;

        int idx_i = (int)idx_f;
        float frac = idx_f - (float)idx_i;
        float window = window_table_ptr_[idx_i] + frac * (window_table_ptr_[idx_i + 1] - window_table_ptr_[idx_i]);

        phase_ += phase_inc_;
        if (phase_ >= 1.0f)
        {
            active_ = false;
        }

        return {L * window * pan_L_, R * window * pan_R_};
    }

    bool IsActive() const { return active_; }

private:
    float pos_, duration_, phase_, phase_inc_, pitch_;
    float pan_L_, pan_R_;
    const float *window_table_ptr_;
    bool active_ = false;
};

/**
 * @brief The master granular processor.
 * Manages voice allocation, rhythmic scheduling, hardware parameter smoothing,
 * and the multi-topology feedback routing matrix.
 */
template <size_t max_delay, size_t num_grains, size_t wind_size>
class GranularEngine
{
public:
    struct Config
    {
        float sample_rate;
        DelayLine<float, max_delay> *buffer_L;
        DelayLine<float, max_delay> *buffer_R;
        AudioBypass *bypass_L;
        AudioBypass *bypass_R;
        float *hann_table;
        float *percussive_table;
        float *reverse_percussive_table;
    };

    GranularEngine() = default;
    ~GranularEngine() = default;

    void Init(const Config &config)
    {
        sample_rate_ = config.sample_rate;
        inv_sample_rate_ = 1.0f / sample_rate_;
        hann_window_table_ = config.hann_table;
        percussive_window_table_ = config.percussive_table;
        reverse_percussive_window_table_ = config.reverse_percussive_table;
        bypass_L_ptr_ = config.bypass_L;
        bypass_R_ptr_ = config.bypass_R;

        for (int i = 0; i <= wind_size; i++)
        {
            hann_window_table_[i] = 0.5f * (1.0f - cosf(TWOPI_F * (float)i / (float)wind_size));
            percussive_window_table_[i] = 1.0f - ((float)i / (float)wind_size);
            reverse_percussive_window_table_[i] = (float)i / (float)wind_size;
        }
        current_window_table_ptr_ = hann_window_table_;

        buffer_L_ = config.buffer_L;
        if (buffer_L_)
            buffer_L_->Reset();

        buffer_R_ = config.buffer_R;
        if (buffer_R_)
            buffer_R_->Reset();

        // Initialize defaults
        settings_.pitch = 1.0f;
        settings_.grain_size = 100.0f;
        settings_.density = 10.0f;
        settings_.feedback = 0.5f;
        settings_.tone = 0.5f;
        settings_.mix = 0.5f;
        settings_.routing = RoutingMode::PING_PONG;
        settings_.quantize = PitchQuantizeMode::SEMITONES;
        settings_.direction = Direction::RANDOM;
        settings_.rhythm = RhythmMode::SYNCED;

        mixer_.Init();
        routing_mode_ = RoutingMode::PING_PONG;
        feedback_ = 0.5f;

        tone_lp_L_ = tone_lp_R_ = 0.0f;
        tone_hp_L_ = tone_hp_R_ = 0.0f;

        // Scheduler Defaults
        trigger_acc_ = 0.0f;
        current_trigger_threshold_ = 1.0f;
        density_ = 10.0f;

        // Smoothing states
        feedback_smoothed_ = 0.5f;
        mix_pos_smoothed_ = 0.5f;

        // State Flags
        bypassed_ = false;
        freeze_active_ = false;
        clipping_ = false;

        rng_.Init(123456789);
    }

    void ProcessBlock(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
    {
        UpdateSmoothing();

        // Calculate increments for sample-accurate smoothing
        float feedback_inc = (feedback_target_ - feedback_) / (float)size;
        float mix_inc = (mix_pos_target_ - mix_pos_) / (float)size;

        for (size_t i = 0; i < size; i++)
        {
            feedback_ += feedback_inc;
            mix_pos_ += mix_inc;
            mixer_.SetPos(mix_pos_);

            float bg_L = bypass_L_ptr_->Process();
            float bg_R = bypass_R_ptr_->Process();

            UpdateScheduler();

            float wet_L = 0.0f, wet_R = 0.0f;
            SumActiveGrains(wet_L, wet_R);

            float dry_L = (bypassed_ && !true_bypass_mode_) ? 0.0f : in[0][i];
            float dry_R = (bypassed_ && !true_bypass_mode_) ? 0.0f : in[1][i];

            ProcessFeedbackPath(dry_L, dry_R, wet_L, wet_R, settings_.drive);

            float mix_L = RouteLeftMix(dry_L, dry_R, wet_L, wet_R) * settings_.output_level;
            float mix_R = RouteRightMix(dry_L, dry_R, wet_L, wet_R) * settings_.output_level;

            ApplyFinalOutput(in[0][i], in[1][i], mix_L, mix_R, bg_L, bg_R, out[0][i], out[1][i]);
        }
    }

    // --- Parameter Setters with Delta Checks ---

    void SetSettings(const GranularSettings &s)
    {
        // Handle state changes that require immediate logic
        if (settings_.rhythm == RhythmMode::FREE && s.rhythm != RhythmMode::FREE)
        {
            current_trigger_threshold_ = 1.0f;
        }

        if (settings_.window != s.window)
        {
            switch (s.window)
            {
            case WindowShape::PLUCK:
                current_window_table_ptr_ = percussive_window_table_;
                break;
            case WindowShape::SWELL:
                current_window_table_ptr_ = reverse_percussive_window_table_;
                break;
            default:
                current_window_table_ptr_ = hann_window_table_;
                break;
            }
        }

        bool boundary_change = (settings_.grain_size != s.grain_size || settings_.time_spray != s.time_spray || settings_.pitch != s.pitch || settings_.pitch_spray != s.pitch_spray);

        settings_ = s;
        freeze_active_ = s.freeze;

        if (boundary_change)
        {
            UpdateQuantizedPitch();
            RecalculateSafetyBoundaries();
        }
    }

    void SetBypass(bool bypassed) { bypassed_ = bypassed; }
    void SetBypassMode(bool true_bypass) { true_bypass_mode_ = true_bypass; }
    void ClearBuffer()
    {
        buffer_L_->Reset();
        buffer_R_->Reset();
    }

    // --- State Getters ---
    bool IsFrozen() const { return freeze_active_; }
    bool HasClipped()
    {
        bool c = clipping_;
        clipping_ = false;
        return c;
    }

    int GetActiveGrainCount() const
    {
        int count = 0;
        for (int i = 0; i < num_grains; i++)
        {
            if (grains_[i].IsActive())
            {
                count++;
            }
        }
        return count;
    }

    float GetDensityHz() const { return density_; }
    float GetTappedHz() const { return settings_.tapped_hz; }
    bool IsTapActive() const { return settings_.tap_active; }
    DensitySubdivision GetDensitySubdivision() const { return settings_.subdivision; }
    float GetDensityTarget() const { return settings_.density; }

private:
    /**
     * @brief Advances the grain trigger clock.
     * In FREE mode, adds jitter to the threshold to prevent metallic
     * resonance at high densities.
     */
    void UpdateScheduler()
    {
        trigger_acc_ += settings_.density * inv_sample_rate_;
        while (trigger_acc_ >= current_trigger_threshold_)
        {
            trigger_acc_ -= current_trigger_threshold_;
            int num = (settings_.rhythm == RhythmMode::BURST) ? 4 : (settings_.rhythm == RhythmMode::SYNCED ? 2 : 1);

            // Trigger the required amount of grains for the current rhythm mode
            for (int k = 0; k < num; k++)
            {
                TriggerNextGrain();
            }

            current_trigger_threshold_ = (settings_.rhythm == RhythmMode::FREE) ? 0.85f + (rng_.NextFloat() * 0.3f) : 1.0f;
            if (settings_.rhythm != RhythmMode::FREE)
            {
                break;
            }
        }
    }

    void SumActiveGrains(float &wet_L, float &wet_R)
    {
        for (int g = 0; g < num_grains; g++)
        {
            typename Grain<max_delay, wind_size>::Output go = grains_[g].Process(buffer_L_, buffer_R_);
            wet_L += go.L;
            wet_R += go.R;
        }
        wet_L *= 0.5f;
        wet_R *= 0.5f;
    }

    /**
     * @brief Handles feedback routing, saturation, filtering, and
     * buffer writing for the granular delay lines.
     */
    void ProcessFeedbackPath(float dry_L, float dry_R, float wet_L, float wet_R, float drive)
    {
        float fb_L = RouteLeftFeedback(dry_L * drive, dry_R * drive, wet_L, wet_R);
        float fb_R = RouteRightFeedback(dry_L * drive, dry_R * drive, wet_L, wet_R);

        if (fabsf(fb_L) > 1.0f || fabsf(fb_R) > 1.0f)
        {
            clipping_ = true;
        }

        // Bipolar Tone control (Lowpass < 0.5, Highpass > 0.5)
        float t_L = ProcessTone(fb_L, tone_lp_L_, tone_hp_L_);
        float t_R = ProcessTone(fb_R, tone_lp_R_, tone_hp_R_);

        // Only write to the delay line if "Freeze" isn't locking the buffer
        if (!freeze_active_)
        {
            buffer_L_->Write(ProcessFeedback(t_L));
            buffer_R_->Write(ProcessFeedback(t_R));
        }
    }

    void ApplyFinalOutput(float in_L,
                          float in_R,
                          float mix_L,
                          float mix_R,
                          float bg_L,
                          float bg_R,
                          float &out_L,
                          float &out_R)
    {
        if (bypassed_ && !true_bypass_mode_)
        {
            out_L = ProcessOutput(in_L + mix_L);
            out_R = ProcessOutput(in_R + mix_R);
        }
        else
        {
            float gate_L = true_bypass_mode_ ? bg_L : 1.0f;
            float gate_R = true_bypass_mode_ ? bg_R : 1.0f;

            out_L = ProcessOutput(mix_L) * gate_L;
            out_R = ProcessOutput(mix_R) * gate_R;
        }
    }

    /**
     * @brief Smoothes hardware ADC values and updates dependent DSP parameters
     * like filter coefficients and safety boundaries.
     */
    void UpdateSmoothing()
    {
        const float kSmooth = 0.05f;

        // Smooth signal-path critical targets
        fonepole(feedback_target_, fclamp(settings_.feedback, 0.0f, 0.85f), kSmooth);
        fonepole(mix_pos_target_, settings_.mix, kSmooth);
        fonepole(tone_val_, settings_.tone, kSmooth);

        // Smooth density for jitter-free scheduling
        fonepole(density_, settings_.density, kSmooth);

        // Convert smoothed Tone value to Filter Coefficients
        float lp_param = (tone_val_ <= 0.5f) ? (tone_val_ * 2.0f) : 1.0f;
        float hp_param = (tone_val_ >= 0.5f) ? ((tone_val_ - 0.5f) * 2.0f) : 0.0f;
        float lp_f = 200.0f + (lp_param * lp_param * 18000.0f);
        float hp_f = 10.0f + (hp_param * hp_param * 3000.0f);

        tone_lp_alpha_ = 1.0f - expf(-TWOPI_F * lp_f * inv_sample_rate_);
        tone_hp_alpha_ = 1.0f - expf(-TWOPI_F * hp_f * inv_sample_rate_);

        RecalculateSafetyBoundaries();
    }

    /**
     * @brief Finds an inactive grain and initializes it with current
     * pitch, position (with spray), and pan values.
     */
    void TriggerNextGrain()
    {
        for (int i = 0; i < num_grains; i++)
        {
            if (!grains_[i].IsActive())
            {
                float dur_samples = (settings_.grain_size * 0.001f) * sample_rate_;
                float spray_samples = (settings_.time_spray * 0.001f) * sample_rate_;

                // Randomize read-head starting position within the "spray" window
                float rand_spray = rng_.NextFloat() * spray_samples;
                float pos = min_safe_delay_ + rand_spray;

                // Calculate per-grain pitch deviation
                float rand_p_off = ((rng_.NextFloat() * 2.0f) - 1.0f) * settings_.pitch_spray;

                // Determine grain playback direction
                float base_p = quantized_pitch_base_;
                if (settings_.direction == Direction::FORWARD)
                {
                    base_p = fabsf(base_p);
                }
                else if (settings_.direction == Direction::REVERSE)
                {
                    base_p = -fabsf(base_p);
                }
                else if (settings_.direction == Direction::RANDOM)
                {
                    bool reverse = rng_.NextFloat() > 0.5f;
                    base_p = reverse ? -fabsf(base_p) : fabsf(base_p);
                }

                float grain_pitch = base_p + rand_p_off;

                // Panning based on spread amount
                float rand_pan = ((rng_.NextFloat() * 2.0f) - 1.0f) * 0.5f * settings_.pan_spread;
                float pan = fclamp(0.5f + rand_pan, 0.0f, 1.0f);

                grains_[i].Trigger(pos, dur_samples, grain_pitch, pan, current_window_table_ptr_);
                return;
            }
        }
    }

    // --- Delay Buffers ---
    DelayLine<float, max_delay> *buffer_L_;
    DelayLine<float, max_delay> *buffer_R_;

    // --- Lookup Tables ---
    float *hann_window_table_;
    float *percussive_window_table_;
    float *reverse_percussive_window_table_;

    // --- Audio State ---
    float sample_rate_;
    float inv_sample_rate_;
    Grain<max_delay, wind_size> grains_[num_grains];
    AudioBypass *bypass_L_ptr_;
    AudioBypass *bypass_R_ptr_;
    bool bypassed_, true_bypass_mode_, clipping_;
    Xorshift32 rng_;
    CrossFade mixer_;

    // --- Scheduler Variables ---
    float trigger_acc_, current_trigger_threshold_;

    // --- Parameters ---
    GranularSettings settings_;
    float feedback_, tone_val_, mix_pos_, density_;
    float quantized_pitch_base_;
    bool freeze_active_;

    float feedback_target_, mix_pos_target_;
    float feedback_smoothed_, mix_pos_smoothed_;

    float min_safe_delay_;
    float max_safe_delay_;

    const float *current_window_table_ptr_;

    float tone_lp_alpha_, tone_hp_alpha_;
    float tone_lp_L_, tone_lp_R_, tone_hp_L_, tone_hp_R_;

    /**
     * @brief Prevents read heads from colliding with write heads by
     * calculating minimum delay offsets based on grain size and pitch.
     */
    void RecalculateSafetyBoundaries()
    {
        float dur_samples = (settings_.grain_size * 0.001f) * sample_rate_;
        float spray_samples = (settings_.time_spray * 0.001f) * sample_rate_;

        // Account for the maximum possible pitch any grain might have
        // after applying Pitch Spray to prevent read-head wrap-around.
        float max_p = daisysp::fmax(1.0f, fabsf(quantized_pitch_base_) + settings_.pitch_spray);

        min_safe_delay_ = (dur_samples * max_p) + spray_samples + 100.0f;
        max_safe_delay_ = (float)max_delay - dur_samples - spray_samples - 100.0f;

        if (max_safe_delay_ < min_safe_delay_)
        {
            max_safe_delay_ = min_safe_delay_;
        }
    }

    /**
     * @brief Maps the raw pitch multiplier into semitone steps or
     * musical intervals (Octaves/Fifths) based on the quantize mode.
     */
    void UpdateQuantizedPitch()
    {
        float p_abs = fabsf(settings_.pitch);
        float p_sign = (settings_.pitch >= 0.0f) ? 1.0f : -1.0f;

        if (p_abs < 0.001f)
        {
            quantized_pitch_base_ = 0.0f;
        }
        else if (settings_.quantize == PitchQuantizeMode::FREE)
        {
            quantized_pitch_base_ = settings_.pitch;
        }
        else
        {
            float semitones = 12.0f * fastlog2f(p_abs);
            if (settings_.quantize == PitchQuantizeMode::SEMITONES)
            {
                semitones = roundf(semitones);
            }
            else
            {
                int st_rounded = (int)roundf(semitones);
                int octave = st_rounded / 12;
                if (st_rounded < 0 && st_rounded % 12 != 0)
                {
                    octave--;
                }
                int rel = st_rounded - (octave * 12);
                int snapped_rel = (rel < 4) ? 0 : (rel < 10) ? 7
                                                             : 12;
                semitones = (float)(octave * 12 + snapped_rel);
            }
            quantized_pitch_base_ = p_sign * exp2f(semitones * kOneTwelfth);
        }
    }

    float RouteLeftFeedback(float dry_L, float dry_R, float wet_L, float wet_R)
    {
        switch (routing_mode_)
        {
        case RoutingMode::PARALLEL:
            return dry_L + (wet_L * feedback_);
        case RoutingMode::PING_PONG:
            return dry_L + (wet_R * feedback_);
        case RoutingMode::SERIES:
            return wet_R + (wet_L * feedback_);
        default:
            break;
        }
        return dry_L;
    }

    float RouteRightFeedback(float dry_L, float dry_R, float wet_L, float wet_R)
    {
        switch (routing_mode_)
        {
        case RoutingMode::PARALLEL:
            return dry_R + (wet_R * feedback_);
        case RoutingMode::PING_PONG:
            return dry_R + (wet_L * feedback_);
        case RoutingMode::SERIES:
            return dry_R + (wet_R * feedback_);
        default:
            break;
        }
        return dry_R;
    }

    float RouteLeftMix(float dry_L, float dry_R, float wet_L, float wet_R)
    {
        return mixer_.Process(dry_L, wet_L);
    }

    float RouteRightMix(float dry_L, float dry_R, float wet_L, float wet_R)
    {
        return mixer_.Process(dry_R, wet_R);
    }

    // Bipolar one-pole filter logic
    float ProcessTone(float in, float &lp_state, float &hp_state)
    {
        hp_state += tone_hp_alpha_ * (in - hp_state);
        float filtered = in - hp_state;

        lp_state += tone_lp_alpha_ * (filtered - lp_state);
        return lp_state;
    }

    // Non-linear saturation approximation: x / (1 + |x|)
    float ProcessFeedback(float in) { return in / (1.0f + fabsf(in)); }

    float ProcessOutput(float in) { return SoftLimit(in); }
};

/**
 * @brief Maps hardware controls to granular settings.
 */
class GranularController
{
public:
    GranularController() = default;

    void Init(Dripline *hw)
    {
        hw_ = hw;
        param_density_sub_.Init(hw_->knobs[Dripline::KNOB_3], 5);
    }

    GranularSettings Update(const dripline::TapTempo &tap)
    {
        GranularSettings s;

        // Raw knob mapping
        s.pitch = hw_->knobs[Dripline::KNOB_1].Value() * 4.0f - 2.0f;
        s.grain_size = 10.0f + (hw_->knobs[Dripline::KNOB_2].Value() * 490.0f);
        s.time_spray = hw_->knobs[Dripline::KNOB_4].Value() * 500.0f;
        s.pitch_spray = hw_->knobs[Dripline::KNOB_5].Value();
        s.pan_spread = hw_->knobs[Dripline::KNOB_6].Value();
        s.feedback = hw_->knobs[Dripline::KNOB_7].Value();
        s.drive = 0.5f + hw_->knobs[Dripline::KNOB_8].Value() * 3.5f;
        s.tone = hw_->knobs[Dripline::KNOB_9].Value();
        s.output_level = hw_->knobs[Dripline::KNOB_11].Value() * 2.0f;
        s.mix = hw_->knobs[Dripline::KNOB_12].Value();

        // Tap Tempo & Subdivision logic
        s.tap_active = tap.IsActive();
        s.tapped_hz = tap.GetFrequencyHz();
        s.subdivision = (DensitySubdivision)param_density_sub_.Process();

        float mult = 1.0f;
        switch (s.subdivision)
        {
        case DensitySubdivision::DOTTED_EIGHTH:
            mult = 1.5f;
            break;
        case DensitySubdivision::EIGHTH_NOTES:
            mult = 2.0f;
            break;
        case DensitySubdivision::EIGHTH_NOTE_TRIPLETS:
            mult = 3.0f;
            break;
        case DensitySubdivision::SIXTEENTH_NOTES:
            mult = 4.0f;
            break;
        default:
            mult = 1.0f;
            break;
        }

        if (s.tap_active && s.tapped_hz > 0.0f)
            s.density = s.tapped_hz * mult;
        else
            s.density = 0.1f + (hw_->knobs[Dripline::KNOB_3].Value() * 99.9f);

        // Toggle Switches
        s.routing = GetRoutingMode();
        s.quantize = GetQuantizeMode();
        s.direction = GetDirection();
        s.window = GetWindowShape();
        s.rhythm = GetRhythmMode();
        s.freeze = (hw_->toggles[Dripline::TOGGLE_SWITCH_4].GetPosition() == DigitalControl::Position::DOWN);

        return s;
    }

private:
    RoutingMode GetRoutingMode()
    {
        auto pos = hw_->toggles[Dripline::TOGGLE_SWITCH_1].GetPosition();
        if (pos == DigitalControl::Position::UP)
            return RoutingMode::PARALLEL;
        if (pos == DigitalControl::Position::DOWN)
            return RoutingMode::SERIES;
        return RoutingMode::PING_PONG;
    }

    PitchQuantizeMode GetQuantizeMode()
    {
        auto pos = hw_->toggles[Dripline::TOGGLE_SWITCH_2].GetPosition();
        if (pos == DigitalControl::Position::UP)
            return PitchQuantizeMode::FREE;
        if (pos == DigitalControl::Position::DOWN)
            return PitchQuantizeMode::OCTAVES_FIFTHS;
        return PitchQuantizeMode::SEMITONES;
    }

    Direction GetDirection()
    {
        auto pos = hw_->toggles[Dripline::TOGGLE_SWITCH_3].GetPosition();
        if (pos == DigitalControl::Position::UP)
            return Direction::FORWARD;
        if (pos == DigitalControl::Position::DOWN)
            return Direction::REVERSE;
        return Direction::RANDOM;
    }

    WindowShape GetWindowShape()
    {
        auto pos = hw_->toggles[Dripline::TOGGLE_SWITCH_5].GetPosition();
        if (pos == DigitalControl::Position::UP)
            return WindowShape::PLUCK;
        if (pos == DigitalControl::Position::DOWN)
            return WindowShape::SWELL;
        return WindowShape::SMOOTH;
    }

    RhythmMode GetRhythmMode()
    {
        auto pos = hw_->toggles[Dripline::TOGGLE_SWITCH_6].GetPosition();
        if (pos == DigitalControl::Position::UP)
            return RhythmMode::FREE;
        if (pos == DigitalControl::Position::DOWN)
            return RhythmMode::BURST;
        return RhythmMode::SYNCED;
    }

    Dripline *hw_;
    QParameter param_density_sub_;
};

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dl_L;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS dl_R;

float DTCM_MEM_SECTION hann_window_table[WIND_SIZE + 1];
float DTCM_MEM_SECTION percussive_window_table[WIND_SIZE + 1];
float DTCM_MEM_SECTION reverse_percussive_window_table[WIND_SIZE + 1];

GranularEngine<MAX_DELAY, NUM_GRAINS, WIND_SIZE> engine;
dripline::TapTempo tap_tempo;
AudioBypass bypass_L, bypass_R;
GranularController controller;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    tap_tempo.Process(size);
    engine.ProcessBlock(in, out, size);
}

void HandleUserInteraction(bool &bypassed, bool &fs1_long_triggered, int &panic_timer, const Settings &settings)
{
    bool fs1_held = hw.footswitches[Dripline::FOOTSWITCH_1].TimeHeldMs() > 500.0f;
    bool fs1_down = hw.footswitches[Dripline::FOOTSWITCH_1].Pressed();

    if (hw.footswitches[Dripline::FOOTSWITCH_1].FallingEdge())
    {
        if (!fs1_long_triggered)
        {
            bypassed = !bypassed;
            engine.SetBypass(bypassed);
            bypass_L.SetBypass(settings.true_bypass ? bypassed : false);
            bypass_R.SetBypass(settings.true_bypass ? bypassed : false);
        }
        fs1_long_triggered = false;
    }

    bool momentary_freeze = false;
    if (fs1_held && fs1_down)
    {
        fs1_long_triggered = true;
        momentary_freeze = true;
    }

    // Toggle freeze logic is now part of controller but momentary freeze from footswitch is handled here

    if (hw.footswitches[Dripline::FOOTSWITCH_2].RisingEdge())
    {
        tap_tempo.Tap();
    }

    // FS 2 Long Press: Panic/Clear
    if (hw.footswitches[Dripline::FOOTSWITCH_2].TimeHeldMs() > 500.0f && hw.footswitches[Dripline::FOOTSWITCH_2].Pressed())
    {
        engine.ClearBuffer();
        tap_tempo.Reset();
        panic_timer = 200;
    }
}

void UpdateUI(bool bypassed, int &panic_timer, float &l2_phase, const Settings &settings)
{
    float l1r = 0, l1g = 0, l1b = 0;
    float l2r = 0, l2g = 0, l2b = 0;

    if (panic_timer > 0)
    {
        l1r = l2r = 1.0f;
        panic_timer -= 10;
    }
    else
    {
        // LED 1 Logic (Effect Status / Clipping / Freeze)
        if (engine.HasClipped())
        {
            l1r = 1.0f;
        }
        else if (engine.IsFrozen())
        {
            l1b = 1.0f;
        }
        else if (bypassed)
        {
            l1r = settings.true_bypass ? 0.0f : 0.15f;
        }
        else
        {
            l1g = 1.0f;
        }

        // LED 2 Logic (Tempo Phase & Color via Density Knob)
        float freq = engine.IsTapActive() ? engine.GetTappedHz() : engine.GetDensityHz();
        l2_phase = fmodf(l2_phase + freq * 0.01f, 1.0f);

        // Use the actual subdivision state for LED color to match engine/hysteresis
        switch (engine.GetDensitySubdivision())
        {
        case DensitySubdivision::QUARTER_NOTES:
            l2r = l2g = l2b = 1.0f;
            break; // White
        case DensitySubdivision::DOTTED_EIGHTH:
            l2r = 1.0f;
            l2g = 1.0f;
            break; // Yellow
        case DensitySubdivision::EIGHTH_NOTES:
            l2g = 1.0f;
            l2b = 1.0f;
            break; // Cyan
        case DensitySubdivision::EIGHTH_NOTE_TRIPLETS:
            l2r = 1.0f;
            l2b = 1.0f;
            break; // Magenta
        case DensitySubdivision::SIXTEENTH_NOTES:
            l2b = 1.0f;
            break; // Blue
        default:
            break;
        }

        float active_ratio = (float)engine.GetActiveGrainCount() / NUM_GRAINS;
        float brightness = (l2_phase < 0.5f ? 1.0f : 0.0f) * (0.3f + 0.7f * active_ratio);

        l2r *= brightness;
        l2g *= brightness;
        l2b *= brightness;
    }

    hw.SetLed(Dripline::LED_1, l1r, l1g, l1b);
    hw.SetLed(Dripline::LED_2, l2r, l2g, l2b);
    hw.UpdateLeds();
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    Settings default_settings;
    default_settings.true_bypass = true;
    storage.Init(default_settings);
    Settings &settings = storage.GetSettings();

    hw.ProcessDigitalControls();
    // Boot Gesture: Hold FS 1 to toggle True Bypass vs Trails
    if (hw.footswitches[Dripline::FOOTSWITCH_1].Pressed())
    {
        settings.true_bypass = !settings.true_bypass;
        storage.Save();

        for (int i = 0; i < 40; i++)
        {
            float v = (i % 2 == 0) ? 1.0f : 0.0f;
            hw.SetLed(Dripline::LED_1, v, v, v);
            hw.UpdateLeds();
            System::Delay(50);
        }
    }
    hw.SetLed(Dripline::LED_1, 0.0f, 0.0f, 0.0f);
    hw.UpdateLeds();

    bypass_L.Init(&hw, Dripline::CHANNEL_LEFT);
    bypass_R.Init(&hw, Dripline::CHANNEL_RIGHT);

    controller.Init(&hw);

    GranularEngine<MAX_DELAY, NUM_GRAINS, WIND_SIZE>::Config engine_cfg;
    engine_cfg.sample_rate = hw.AudioSampleRate();
    engine_cfg.buffer_L = &dl_L;
    engine_cfg.buffer_R = &dl_R;
    engine_cfg.bypass_L = &bypass_L;
    engine_cfg.bypass_R = &bypass_R;
    engine_cfg.hann_table = hann_window_table;
    engine_cfg.percussive_table = percussive_window_table;
    engine_cfg.reverse_percussive_table = reverse_percussive_window_table;

    engine.Init(engine_cfg);
    engine.SetBypassMode(settings.true_bypass);

    tap_tempo.Init(hw.AudioSampleRate());

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    bool bypassed = false;
    bool fs1_long_triggered = false;
    float led2_phase = 0.0f;
    int panic_flash_timer = 0;

    while (true)
    {
        hw.ProcessAllControls();

        auto settings_block = controller.Update(tap_tempo);
        engine.SetSettings(settings_block);

        HandleUserInteraction(bypassed, fs1_long_triggered, panic_flash_timer, settings);
        UpdateUI(bypassed, panic_flash_timer, led2_phase, settings);
        hw.DelayMs(10);
    }
}
