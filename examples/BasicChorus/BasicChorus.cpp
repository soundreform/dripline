#include "bypass.h"
#include "daisysp.h"
#include "dripline.h"

using namespace daisysp;
using namespace dripline;

Dripline hw;
Chorus ch;
AudioBypass bp;

float wet;
float deltarget, del;
float lfotarget, lfo;

void Controls()
{
    hw.ProcessAllControls();

    // Knob 1: Wet/Dry mix
    wet = hw.knobs[Dripline::KNOB_1].Value();

    // Knob 2: LFO Frequency
    float k = hw.knobs[Dripline::KNOB_2].Value();
    ch.SetLfoFreq(k * k * 20.f);

    // Knob 3: LFO Depth
    lfotarget = hw.knobs[Dripline::KNOB_3].Value();

    // Knob 4: Delay
    deltarget = hw.knobs[Dripline::KNOB_4].Value();

    // Knob 5: Feedback
    ch.SetFeedback(hw.knobs[Dripline::KNOB_5].Value());

    // Footswitch 1: Bypass Toggle
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

        fonepole(del, deltarget, .0001f); // smooth at audio rate
        ch.SetDelay(del);

        fonepole(lfo, lfotarget, .0001f); // smooth at audio rate
        ch.SetLfoDepth(lfo);

        ch.Process(in[0][i]);

        float wet_l = ch.GetLeft() * wet + in[0][i] * (1.f - wet);
        float wet_r = ch.GetRight() * wet + in[1][i] * (1.f - wet);

        out[0][i] = wet_l * bypass_gain + in[0][i] * (1.0f - bypass_gain);
        out[1][i] = wet_r * bypass_gain + in[1][i] * (1.0f - bypass_gain);
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    ch.Init(sample_rate);
    bp.Init(&hw, Dripline::CHANNEL_LEFT);

    wet = .9f;
    deltarget = del = 0.f;
    lfotarget = lfo = 0.f;

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while (1)
    {
        // LED 1 indicates effect status (Green = Active, Red = Bypassed)
        float active = bp.Value();
        hw.SetLed(Dripline::LED_1, 1.0f - active, active, 0.0f);
        hw.UpdateLeds();
        hw.DelayMs(6);
    }
}
