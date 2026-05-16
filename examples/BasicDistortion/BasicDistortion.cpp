#include "daisysp.h"
#include "dripline.h"

using namespace daisysp;
using namespace dripline;

static Dripline hw;

float hardClip(float in)
{
    in = in > 1.f ? 1.f : in;
    in = in < -1.f ? -1.f : in;
    return in;
}

float softClip(float in)
{
    if (in > 0)
        return 1 - expf(-in);
    return -1 + expf(in);
}

bool bypassHard, bypassSoft;
static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    hw.ProcessAllControls();

    float Pregain = hw.knobs[Dripline::KNOB_2].Value() * 10 + 1.2;
    float Gain = hw.knobs[Dripline::KNOB_3].Value() * 100 + 1.2;
    float drywet = hw.knobs[Dripline::KNOB_1].Value();

    if (hw.footswitches[Dripline::FOOTSWITCH_1].RisingEdge())
        bypassSoft = !bypassSoft;
    if (hw.footswitches[Dripline::FOOTSWITCH_2].RisingEdge())
        bypassHard = !bypassHard;

    for (size_t i = 0; i < size; i++)
    {
        for (int chn = 0; chn < 2; chn++)
        {
            float input = in[chn][i] * Pregain;
            float wet = input;

            if (!bypassSoft || !bypassHard)
            {
                wet *= Gain;
            }

            if (!bypassSoft)
            {
                wet = softClip(wet);
            }

            if (!bypassHard)
            {
                wet = hardClip(wet);
            }

            out[chn][i] = wet * drywet + input * (1 - drywet);
        }
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);

    bypassHard = bypassSoft = false;

    // start callback
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    while (1)
    {
        // LED 1: Soft Clip status (Blue = ON)
        // LED 2: Hard Clip status (Red = ON)
        hw.SetLed(Dripline::LED_1, 0.0f, 0.0f, bypassSoft ? 0.0f : 1.0f);
        hw.SetLed(Dripline::LED_2, bypassHard ? 0.0f : 1.0f, 0.0f, 0.0f);
        hw.UpdateLeds();
        hw.DelayMs(6);
    }
}
