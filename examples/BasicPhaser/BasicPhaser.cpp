#include "bypass.h"
#include "daisysp.h"
#include "dripline.h"
#include "qparameter.h"

using namespace daisysp;
using namespace dripline;

Dripline hw;
Phaser phaser;
AudioBypass bp;
QParameter stage_ctrl;

float wet;
float freqtarget, freq;
float lfotarget, lfo;

void Controls()
{
    hw.ProcessAllControls();

    // Knob 1: Wet/Dry
    wet = hw.knobs[Dripline::KNOB_1].Value();

    // Knob 2: LFO Rate
    float k = hw.knobs[Dripline::KNOB_2].Value();
    phaser.SetLfoFreq(k * k * 20.f);

    // Knob 3: LFO Depth
    lfotarget = hw.knobs[Dripline::KNOB_3].Value();

    // Knob 4: Base Frequency
    k = hw.knobs[Dripline::KNOB_4].Value();
    freq = k * k * 7000;

    // Knob 5: Feedback
    phaser.SetFeedback(hw.knobs[Dripline::KNOB_5].Value());

    // Knob 6: Num Stages (1-8)
    phaser.SetPoles(stage_ctrl.Process() + 1);

    if (hw.footswitches[Dripline::FOOTSWITCH_1].RisingEdge())
        bp.SetBypass(!bp.Value() > 0.5f);
}

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    Controls();

    for (size_t i = 0; i < size; i++)
    {
        float bypass_gain = bp.Process();

        fonepole(freq, freqtarget, .0001f); // smooth at audio rate
        phaser.SetFreq(freq);

        fonepole(lfo, lfotarget, .0001f); // smooth at audio rate
        phaser.SetLfoDepth(lfo);

        float sig = phaser.Process(in[0][i]);
        float res = sig * wet + in[0][i] * (1.f - wet);

        out[0][i] = res * bypass_gain + in[0][i] * (1.0f - bypass_gain);
        out[1][i] = res * bypass_gain + in[1][i] * (1.0f - bypass_gain);
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    phaser.Init(sample_rate);
    bp.Init(&hw, Dripline::CHANNEL_LEFT);
    stage_ctrl.Init(hw.knobs[Dripline::KNOB_6], 8);

    wet = .9f;
    freqtarget = freq = 0.f;
    lfotarget = lfo = 0.f;

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while (1)
    {
        float active = bp.Value();
        hw.SetLed(Dripline::LED_1, 1.0f - active, active, 0.0f);
        hw.UpdateLeds();
        hw.DelayMs(6);
    }
}
