#include "bypass.h"
#include "daisysp.h"
#include "dripline.h"

using namespace daisysp;
using namespace dripline;

Flanger flanger;
Dripline hw;
AudioBypass bp;

float wet;
float deltarget, del;
float lfotarget, lfo;

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    hw.ProcessAllControls();

    // Knob 1: Wet/Dry
    wet = hw.knobs[Dripline::KNOB_1].Value();

    // Knob 2: LFO Freq
    float val = hw.knobs[Dripline::KNOB_2].Value();
    flanger.SetLfoFreq(val * val * 10.f);

    // Knob 3: LFO Depth
    lfotarget = hw.knobs[Dripline::KNOB_3].Value();

    // Knob 4: Delay
    deltarget = hw.knobs[Dripline::KNOB_4].Value();

    // Knob 5: Feedback
    flanger.SetFeedback(hw.knobs[Dripline::KNOB_5].Value());

    if (hw.footswitches[Dripline::FOOTSWITCH_1].RisingEdge())
        bp.SetBypass(!bp.Value() > 0.5f);

    for (size_t i = 0; i < size; i++)
    {
        float bypass_gain = bp.Process();

        fonepole(del, deltarget, .0001f); // smooth at audio rate
        flanger.SetDelay(del);

        fonepole(lfo, lfotarget, .0001f); // smooth at audio rate
        flanger.SetLfoDepth(lfo);

        float sig = flanger.Process(in[0][i]);
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

    deltarget = del = 0.f;
    lfotarget = lfo = 0.f;
    flanger.Init(sample_rate);
    bp.Init(&hw, Dripline::CHANNEL_LEFT);

    wet = .9f;

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
