
#include "daisy.h"
#include "daisysp.h"
#include "dripline.h"
#include "dripline/bypass.h"
#include "dripline/fsw.h"
#include "dripline/led.h"
#include "dripline/qparam.h"
#include "dripline/sm.h"
#include "dripline/tap.h"
#include "dsp/engine.h"
#include "dsp/grain.h"
#include "dsp/modulator.h"
#include "dsp/quantizer.h"
#include "dsp/router.h"
#include "dsp/scheduler.h"

#define MAX_DELAY_SAMPLES 96000
#define GRAIN_SIZE 16
#define CONTROL_RATE_MS 10

using daisy::AudioHandle;
using daisy::Color;
using daisy::PersistentStorage;
using daisysp::DelayLine;
using daisysp::fclamp;
using dripline::AudioBypass;
using dripline::DigitalControl;
using dripline::Dripline;
using dripline::Footswitch;
using dripline::LedManager;
using dripline::QParameter;
using dripline::StateMachine;
using dripline::TapTempo;
using dsp::Engine;
using dsp::EngineConfig;
using dsp::EngineOutput;
using dsp::GrainDirection;
using dsp::ModulatorDepth;
using dsp::QuantizeMode;
using dsp::RawEngineParams;
using dsp::RouterMode;
using dsp::SchedulerRhythm;
using dsp::SchedulerSubdivision;
using dsp::WindowShape;
using dsp::WindowTable;

enum class BypassMode {
    TRUE_BYPASS,
    TRAILS_BYPASS,
};

enum class ExprTarget {
    UNASSIGNED,
    PITCH,
    DURATION,
    DENSITY,
    TIME_SPRAY,
    PITCH_SPRAY,
    FEEDBACK,
    COUNT,
};

enum class State {
    BYPASSED,
    ACTIVE,
    EXPR_ASSIGN,
};

enum class Event {
    FSW1_TAPPED,
    FSW2_LONG_PRESSED,
    FSW2_RELEASED,
};

struct Settings {
    BypassMode bypass_mode;
    ExprTarget expr_target;

    bool operator!=(const Settings &other) const {
        return !(other.bypass_mode == bypass_mode && other.expr_target == expr_target);
    }
};

struct UIContext {
    State state;
    BypassMode byp_mode;
    ExprTarget expr_pending;
    SchedulerSubdivision subdivision;
    size_t grain_size;
    int active_grain_count;
    float tempo_freq_hz;
    bool is_frozen;
};

class LedRenderer {
  public:
    LedRenderer()
        : anims_(colors_) {
    }

    void Init(LedManager *leds) {
        leds_ = leds;
    }

    void Update(const UIContext &ctx) {
        switch (ctx.state) {
        case State::ACTIVE:
            SetStatus(ctx);
            SetTempo(ctx);
            break;
        case State::BYPASSED:
            SetStatus(ctx);
            leds_->Set(Dripline::LED_2, &anims_.off);
            break;
        case State::EXPR_ASSIGN:
            SetStatus(ctx);
            SetExprAssign(ctx);
            break;
        default:
            leds_->Set(Dripline::LED_1, &anims_.off);
            leds_->Set(Dripline::LED_2, &anims_.off);
            break;
        }
        leds_->Update();
    }

    void RenderPanicFlash() {
        LedManager::AnimationStep sequence[] = {{&anims_.panic_solid, 100}, {&anims_.off, 0}};
        leds_->SetSequence(Dripline::LED_1, sequence, 2);
        leds_->SetSequence(Dripline::LED_2, sequence, 2);
    }

    void RenderBootConfirmFlash() {
        leds_->Set(Dripline::LED_1, &anims_.boot_confirm, 200);
        leds_->Update(true);
    }

    void RenderExprAssignConfirmFlash(ExprTarget target) {
        anims_.expr_confirm.SetColor(GetExprTargetColor(target));
        leds_->Set(Dripline::LED_2, &anims_.expr_confirm, 500);
    }

    void RenderClipFlash() {
        leds_->Set(Dripline::LED_1, &anims_.panic_solid, 50);
    }

  private:
    void SetStatus(const UIContext &ctx) {
        if (ctx.is_frozen) {
            leds_->Set(Dripline::LED_1, &anims_.frozen);
        } else if (ctx.state == State::ACTIVE || ctx.state == State::EXPR_ASSIGN) {
            leds_->Set(Dripline::LED_1, &anims_.active);
        } else if (ctx.state == State::BYPASSED) {
            if (ctx.byp_mode == BypassMode::TRUE_BYPASS) {
                leds_->Set(Dripline::LED_1, &anims_.true_bypass);
            } else {
                leds_->Set(Dripline::LED_1, &anims_.trails_bypass);
            }
        }
    }

    void SetTempo(const UIContext &ctx) {
        anims_.tempo_pulse.SetRate(ctx.tempo_freq_hz);
        anims_.tempo_pulse.SetColor(GetSubdivisionColor(ctx.subdivision));
        float brightness = fclamp((float)ctx.active_grain_count / (float)ctx.grain_size, 0.1f, 1.0f);
        anims_.tempo_pulse.SetBrightness(brightness);
        leds_->Set(Dripline::LED_2, &anims_.tempo_pulse);
    }

    void SetExprAssign(const UIContext &ctx) {
        anims_.expr_assign.SetColor(GetExprTargetColor(ctx.expr_pending));
        leds_->Set(Dripline::LED_2, &anims_.expr_assign);
    }

    Color GetSubdivisionColor(SchedulerSubdivision sub) {
        switch (sub) {
        case dsp::SchedulerSubdivision::QUARTER:
            return colors_.white;
        case dsp::SchedulerSubdivision::DOTTED_EIGHTH:
            return colors_.yellow;
        case dsp::SchedulerSubdivision::EIGHTH:
            return colors_.cyan;
        case dsp::SchedulerSubdivision::EIGHTH_TRIPLETS:
            return colors_.magenta;
        case dsp::SchedulerSubdivision::SIXTEENTH:
            return colors_.blue;
        default:
            return colors_.white;
        }
    }

    Color GetExprTargetColor(ExprTarget target) {
        switch (target) {
        case ExprTarget::UNASSIGNED:
            return colors_.white;
        case ExprTarget::PITCH:
            return colors_.blue;
        case ExprTarget::DURATION:
            return colors_.green;
        case ExprTarget::DENSITY:
            return colors_.yellow;
        case ExprTarget::TIME_SPRAY:
            return colors_.cyan;
        case ExprTarget::PITCH_SPRAY:
            return colors_.magenta;
        case ExprTarget::FEEDBACK:
            return colors_.orange;
        default:
            return colors_.white;
        }
    }

    LedManager *leds_;

    struct Colors {
        daisy::Color white{1.f, 1.f, 1.f};
        daisy::Color yellow{1.f, 1.f, 0.f};
        daisy::Color cyan{0.f, 1.f, 1.f};
        daisy::Color magenta{1.f, 0.f, 1.f};
        daisy::Color blue{0.f, 0.f, 1.f};
        daisy::Color green{0.f, 1.f, 0.f};
        daisy::Color orange{1.f, 0.5f, 0.f};
        daisy::Color red{1.f, 0.f, 0.f};
        daisy::Color dim_red{0.2f, 0.f, 0.f};
    } colors_;

    struct Animations {
        Animations(const Colors &c)
            : active(c.green),
              trails_bypass(c.dim_red),
              frozen(c.blue),
              expr_assign(c.white),
              panic_solid(c.red),
              tempo_pulse(c.white, 1000),
              boot_confirm(c.white, 50),
              expr_confirm(c.white, 100) {
        }

        LedManager::Solid active;
        LedManager::Solid trails_bypass;
        LedManager::Solid frozen;
        LedManager::Solid expr_assign;
        LedManager::Solid panic_solid;
        LedManager::Off true_bypass;
        LedManager::Off off;
        LedManager::Pulse tempo_pulse;
        LedManager::Blink boot_confirm;
        LedManager::Blink expr_confirm;
    } anims_;
};

template <size_t max_delay, size_t grain_size, size_t control_rate>
class App {
  public:
    App() = default;
    ~App() = default;

    void Init(Dripline *hw, Engine<max_delay, grain_size> *eng, PersistentStorage<Settings> *storage) {
        hw_ = hw;
        eng_ = eng;
        storage_ = storage;

        tap_.Init(hw_->AudioSampleRate());

        leds_.Init(hw_);
        led_renderer_.Init(&leds_);

        byp1_.Init(hw_, Dripline::CHANNEL_LEFT);
        byp2_.Init(hw_, Dripline::CHANNEL_RIGHT);

        expr_target_param_.Init(&hw_->knobs[Dripline::KNOB_10], static_cast<int>(ExprTarget::COUNT));

        fsw1_.Init(&hw_->footswitches[Dripline::FOOTSWITCH_1]);
        fsw1_.OnTap([this] { HandleFSW1Tap(); });
        fsw1_.OnMomentary(500.0f, [this] { HandleFSW1ShortPress(); }, [this] { HandleFSW1Release(); });

        fsw2_.Init(&hw_->footswitches[Dripline::FOOTSWITCH_2]);
        fsw2_.OnTap([this] { HandleFSW2Tap(); });
        fsw2_.OnHold(500.0f, 1000.0f, [this] { HandleFSW2ShortPress(); });
        fsw2_.OnMomentary(1000.0f, [this] { HandleFSW2LongPress(); }, [this] { HandleFSW2Release(); });

        sm_.Init(State::BYPASSED);
        sm_.AddTransition(Event::FSW1_TAPPED, State::ACTIVE, State::BYPASSED);
        sm_.AddTransition(Event::FSW1_TAPPED, State::BYPASSED, State::ACTIVE);
        sm_.AddTransition(Event::FSW2_LONG_PRESSED, State::ACTIVE, State::EXPR_ASSIGN);
        sm_.AddTransition(Event::FSW2_RELEASED, State::EXPR_ASSIGN, State::ACTIVE);
        sm_.OnEnter(State::BYPASSED, [this] { OnEnterBypassed(); });
        sm_.OnExit(State::BYPASSED, [this] { OnExitBypassed(); });
        sm_.OnExit(State::EXPR_ASSIGN, [this] { OnExitExprAssign(); });
    }

    void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
        tap_.Process(size);

        bool clipped_in_block = false;

        for (size_t i = 0; i < size; i++) {
            EngineOutput engine_out = eng_->Process({in[0][i], in[1][i]});
            if (engine_out.clipped) {
                clipped_in_block = true;
            }

            float outl_gain = 1.0f;
            float outr_gain = 1.0f;

            if (byp_mode_ == BypassMode::TRUE_BYPASS) {
                outl_gain = byp1_.Process();
                outr_gain = byp2_.Process();
            }

            out[0][i] = engine_out.L * outl_gain;
            out[1][i] = engine_out.R * outr_gain;
        }

        if (clipped_in_block) {
            clipped_flag_ = true;
        }
    }

    void Boot() {
        // Load the initial settings from persistent storage first.
        Settings &settings = storage_->GetSettings();
        byp_mode_ = settings.bypass_mode;
        expr_target_ = settings.expr_target;

        // Then, check if the footswitch is held down. If not, we're done.
        if (!hw_->footswitches[Dripline::FOOTSWITCH_1].RawState()) {
            return;
        }

        // If the switch is held, enter the gesture-handling loop.
        uint32_t start_time = hw_->seed.system.GetNow();
        while (hw_->seed.system.GetNow() - start_time < 5000) {
            hw_->ProcessDigitalControls();

            // On release, toggle the bypass mode, save it, and provide feedback.
            if (hw_->footswitches[Dripline::FOOTSWITCH_1].FallingEdge()) {
                settings.bypass_mode = (settings.bypass_mode == BypassMode::TRUE_BYPASS) ? BypassMode::TRAILS_BYPASS : BypassMode::TRUE_BYPASS;
                storage_->Save();
                led_renderer_->RenderBootConfirmFlash();
                byp_mode_ = settings.bypass_mode; // Update internal state to match
                break;                            // Gesture complete, exit loop.
            }

            hw_->DelayMs(1);
        }
    }

    void Tick() {
        hw_->ProcessAllControls();

        fsw1_.Process();
        fsw2_.Process();

        if (clipped_flag_.exchange(false)) {
            led_renderer_.RenderClipFlash();
        }

        switch (sm_.GetState()) {
        case State::ACTIVE:
            TickActive();
            break;
        case State::EXPR_ASSIGN:
            TickExprAssign();
            break;
        case State::BYPASSED:
        default:
            break;
        }

        led_renderer_.Update(GetUIContext());

        hw_->DelayMs(control_rate);
    }

  private:
    void HandleFSW1Tap() {
        sm_.Trigger(Event::FSW1_TAPPED);
    }

    void HandleFSW1ShortPress() {
        eng_params_.router.freeze = true;
    }

    void HandleFSW1Release() {
        eng_params_.router.freeze = false;
    }

    void HandleFSW2LongPress() {
        sm_.Trigger(Event::FSW2_LONG_PRESSED);
    }

    void HandleFSW2Release() {
        sm_.Trigger(Event::FSW2_RELEASED);
    }

    void HandleFSW2Tap() {
        tap_.Tap();
    }

    void HandleFSW2ShortPress() {
        eng_->Reset();
        led_renderer_.RenderPanicFlash();
    }

    void OnEnterBypassed() {
        eng_params_.router.bypass = true;
        byp1_.SetBypass(true);
        byp2_.SetBypass(true);
    }

    void OnExitBypassed() {
        eng_params_.router.bypass = false;
        byp1_.SetBypass(false);
        byp2_.SetBypass(false);
    }

    void OnExitExprAssign() {
        Settings &settings = storage_->GetSettings();
        settings.expr_target = expr_target_pending_;
        expr_target_ = expr_target_pending_;
        storage_->Save();
        led_renderer_.RenderExprAssignConfirmFlash(expr_target_);
    }

    void TickActive() {
        float pitch = hw_->knobs[Dripline::KNOB_1].Value();
        float duration = hw_->knobs[Dripline::KNOB_2].Value();
        float manual_freq = hw_->knobs[Dripline::KNOB_3].Value();
        float time_spray = hw_->knobs[Dripline::KNOB_4].Value();
        float pitch_spray = hw_->knobs[Dripline::KNOB_5].Value();
        float pan_spread = hw_->knobs[Dripline::KNOB_6].Value();
        float feedback = hw_->knobs[Dripline::KNOB_7].Value();
        float drive = hw_->knobs[Dripline::KNOB_8].Value();
        float tone = hw_->knobs[Dripline::KNOB_9].Value();
        float expr_depth_knob = hw_->knobs[Dripline::KNOB_10].Value();
        float output = hw_->knobs[Dripline::KNOB_11].Value();
        float mix = hw_->knobs[Dripline::KNOB_12].Value();

        float expr_pedal = hw_->expression.Value();
        float expr_depth = (expr_depth_knob - 0.5f) * 2.0f;
        float expr_mod = expr_pedal * expr_depth;

        switch (expr_target_) {
        case ExprTarget::PITCH:
            pitch += expr_mod;
            break;
        case ExprTarget::DURATION:
            duration += expr_mod;
            break;
        case ExprTarget::DENSITY:
            manual_freq += expr_mod;
            break;
        case ExprTarget::TIME_SPRAY:
            time_spray += expr_mod;
            break;
        case ExprTarget::PITCH_SPRAY:
            pitch_spray += expr_mod;
            break;
        case ExprTarget::FEEDBACK:
            feedback += expr_mod;
            break;
        case ExprTarget::UNASSIGNED:
        case ExprTarget::COUNT:
        default:
            break;
        }

        eng_params_.grain_manager.pitch = pitch;
        eng_params_.grain_manager.duration = duration;
        eng_params_.scheduler.tapped_freq = tap_.GetFrequencyHz();
        eng_params_.scheduler.manual_freq = manual_freq;
        eng_params_.grain_manager.time_spray = time_spray;
        eng_params_.grain_manager.pitch_spray = pitch_spray;
        eng_params_.grain_manager.pan_spread = pan_spread;
        eng_params_.router.feedback = feedback;
        eng_params_.router.drive = drive;
        eng_params_.router.tone = tone;
        eng_params_.router.output = output;
        eng_params_.router.mix = mix;

        switch (hw_->toggles[Dripline::TOGGLE_SWITCH_1].GetPosition()) {
        case DigitalControl::Position::UP:
            eng_params_.router.mode = RouterMode::PARALLEL;
            break;
        case DigitalControl::Position::CENTER:
            eng_params_.router.mode = RouterMode::PING_PONG;
            break;
        case DigitalControl::Position::DOWN:
            eng_params_.router.mode = RouterMode::SERIES;
            break;
        }

        switch (hw_->toggles[Dripline::TOGGLE_SWITCH_2].GetPosition()) {
        case DigitalControl::Position::UP:
            eng_params_.grain_manager.quantize_mode = QuantizeMode::FREE;
            break;
        case DigitalControl::Position::CENTER:
            eng_params_.grain_manager.quantize_mode = QuantizeMode::SEMITONES;
            break;
        case DigitalControl::Position::DOWN:
            eng_params_.grain_manager.quantize_mode = QuantizeMode::OCTAVES_FIFTHS;
            break;
        }

        switch (hw_->toggles[Dripline::TOGGLE_SWITCH_3].GetPosition()) {
        case DigitalControl::Position::UP:
            eng_params_.grain_manager.direction = GrainDirection::FORWARD;
            break;
        case DigitalControl::Position::CENTER:
            eng_params_.grain_manager.direction = GrainDirection::RANDOM;
            break;
        case DigitalControl::Position::DOWN:
            eng_params_.grain_manager.direction = GrainDirection::REVERSE;
            break;
        }

        switch (hw_->toggles[Dripline::TOGGLE_SWITCH_4].GetPosition()) {
        case DigitalControl::Position::UP:
            eng_params_.modulator.depth = ModulatorDepth::SOME;
            break;
        case DigitalControl::Position::CENTER:
            eng_params_.modulator.depth = ModulatorDepth::NONE;
            break;
        case DigitalControl::Position::DOWN:
            eng_params_.modulator.depth = ModulatorDepth::MORE;
            break;
        }

        switch (hw_->toggles[Dripline::TOGGLE_SWITCH_5].GetPosition()) {
        case DigitalControl::Position::UP:
            eng_params_.grain_manager.window = WindowShape::PLUCK;
            break;
        case DigitalControl::Position::CENTER:
            eng_params_.grain_manager.window = WindowShape::SMOOTH;
            break;
        case DigitalControl::Position::DOWN:
            eng_params_.grain_manager.window = WindowShape::SWELL;
            break;
        }

        switch (hw_->toggles[Dripline::TOGGLE_SWITCH_6].GetPosition()) {
        case DigitalControl::Position::UP:
            eng_params_.scheduler.rhythm = SchedulerRhythm::FREE;
            break;
        case DigitalControl::Position::CENTER:
            eng_params_.scheduler.rhythm = SchedulerRhythm::SYNCED;
            break;
        case DigitalControl::Position::DOWN:
            eng_params_.scheduler.rhythm = SchedulerRhythm::BURST;
            break;
        }

        eng_->UpdateParams(eng_params_);
    }

    void TickExprAssign() {
        expr_target_pending_ = static_cast<ExprTarget>(expr_target_param_.Process());
    }

    UIContext GetUIContext() {
        State state = sm_.GetState();
        float tempo_freq_hz = eng_->GetFrequencyHz();
        SchedulerSubdivision subdivision = eng_->GetSubdivision();
        int active_grain_count = eng_->GetActiveGrainCount();

        return {
            .state = state,
            .byp_mode = byp_mode_,
            .expr_pending = expr_target_pending_,
            .subdivision = subdivision,
            .grain_size = grain_size,
            .active_grain_count = active_grain_count,
            .tempo_freq_hz = tempo_freq_hz,
            .is_frozen = eng_params_.router.freeze,
        };
    }

    void TempDebug() {
        // debug leds
        hw_->SetLed(Dripline::LED_1, 1.0f, 0.0f, 0.0f);
        hw_->UpdateLeds();
    }

    AudioBypass byp1_, byp2_;
    Dripline *hw_;
    Engine<max_delay, grain_size> *eng_;
    Footswitch fsw1_, fsw2_;
    LedManager leds_;
    LedRenderer led_renderer_;
    PersistentStorage<Settings> *storage_;
    QParameter expr_target_param_;
    RawEngineParams eng_params_;
    StateMachine<State, Event> sm_;
    TapTempo tap_;

    BypassMode byp_mode_;
    ExprTarget expr_target_ = ExprTarget::UNASSIGNED;
    ExprTarget expr_target_pending_ = ExprTarget::UNASSIGNED;
    std::atomic<bool> clipped_flag_{false};
};

App<MAX_DELAY_SAMPLES, GRAIN_SIZE, CONTROL_RATE_MS> app;
DelayLine<float, MAX_DELAY_SAMPLES> DSY_SDRAM_BSS buf1, buf2;
Dripline hw;
Engine<MAX_DELAY_SAMPLES, GRAIN_SIZE> eng;
PersistentStorage<Settings> storage(hw.seed.qspi);
WindowBank DTCM_MEM_SECTION window_bank;

int main() {
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    storage.Init({
        .bypass_mode = BypassMode::TRUE_BYPASS,
        .expr_target = ExprTarget::UNASSIGNED,
    });
    window_bank.Init();

    buf1.Init();
    buf2.Init();

    EngineConfig<MAX_DELAY_SAMPLES> eng_cnf = {
        .sample_rate = hw.AudioSampleRate(),
        .control_rate = 1000.0f / static_cast<float>(CONTROL_RATE_MS),
        .L = &buf1,
        .R = &buf2,
        .window_bank = &window_bank,
    };
    eng.Init(eng_cnf);

    app.Init(&hw, &eng, &storage);
    app.Boot();

    hw.StartAdc();
    hw.StartAudio([](AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) { app.AudioCallback(in, out, size); });

    while (true) {
        app.Tick();
    }

    return 0;
}
