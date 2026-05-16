#include "daisy.h"
#include "dripline.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using dripline::Dripline;

Dripline hw;

float hue = 0.0f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    float r = 0.5f + 0.5f * sinf(hue + 0.0f);
    float g = 0.5f + 0.5f * sinf(hue + 2.0943951f);
    float b = 0.5f + 0.5f * sinf(hue + 4.1887902f);

    hw.SetLed(Dripline::LED_1, r, g, b);
    hw.SetLed(Dripline::LED_2, b, r, g);
    hw.UpdateLeds();

    hue += 0.05f;
    if (hue > 6.2831853f)
    {
        hue -= 6.2831853f;
    }

    for (size_t i = 0; i < size; i++)
    {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_16KHZ);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true)
    {

        hw.DelayMs(50.0f);
    }
}
