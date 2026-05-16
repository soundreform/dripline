#include "daisy.h"
#include "dripline.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using dripline::Dripline;

Dripline hw;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();

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
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true)
    {
        hw.DelayMs(10);
    }
}
