#include "bypass.h"
#include "daisysp.h"
#include "dripline.h"
#include "qparameter.h"

using namespace daisysp;
using namespace dripline;

Dripline hw;
Tremolo treml, tremr;
AudioBypass bp;
QParameter wave_ctrl;

void AudioCallback(AudioHandle::InputBuffer in,
                   AudioHandle::OutputBuffer out,
                   size_t size)
{
    hw.ProcessAllControls();

    // Knob 1: LFO Freq
    float freq = hw.knobs[Dripline::KNOB_1].Value() * 20.f;
    treml.SetFreq(freq);
    tremr.SetFreq(freq);

    // Knob 2: LFO Depth
    float depth = hw.knobs[Dripline::KNOB_2].Value();
    treml.SetDepth(depth);
    tremr.SetDepth(depth);

    // Knob 3: Waveform
    int waveform = wave_ctrl.Process();
    treml.SetWaveform(waveform);
    tremr.SetWaveform(waveform);

    if (hw.footswitches[Dripline::FOOTSWITCH_1].RisingEdge())
        bp.SetBypass(!bp.Value() > 0.5f);

    for (size_t i = 0; i < size; i++)
    {
        float bypass_gain = bp.Process();
        out[0][i] = treml.Process(in[0][i]) * bypass_gain + in[0][i] * (1.0f - bypass_gain);
        out[1][i] = tremr.Process(in[1][i]) * bypass_gain + in[1][i] * (1.0f - bypass_gain);
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    treml.Init(sample_rate);
    tremr.Init(sample_rate);
    bp.Init(&hw, Dripline::CHANNEL_LEFT);
    wave_ctrl.Init(hw.knobs[Dripline::KNOB_3], 5);

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
