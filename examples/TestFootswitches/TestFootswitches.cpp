#include "daisy.h"
#include "dripline.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using dripline::Dripline;

Dripline hw;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
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
        hw.ProcessAllControls();

        // Turn LED_1 Red when Footswitch 1 is pressed
        bool fs1 = hw.footswitches[Dripline::FOOTSWITCH_1].Pressed();
        hw.SetLed(Dripline::LED_1, fs1 ? 1.0f : 0.0f, 0.0f, 0.0f);

        // Turn LED_2 Green when Footswitch 2 is pressed
        bool fs2 = hw.footswitches[Dripline::FOOTSWITCH_2].Pressed();
        hw.SetLed(Dripline::LED_2, 0.0f, fs2 ? 1.0f : 0.0f, 0.0f);

        hw.UpdateLeds();
        hw.DelayMs(10);
    }
}
