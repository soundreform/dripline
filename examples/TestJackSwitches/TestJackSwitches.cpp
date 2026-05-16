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
        bool l_in = hw.jacks[Dripline::JACK_SWITCH_LEFT_IN].Pressed();
        bool l_out = hw.jacks[Dripline::JACK_SWITCH_LEFT_OUT].Pressed();
        hw.SetLed(Dripline::LED_1, l_in ? 1.0f : 0.0f, 0.0f, l_out ? 1.0f : 0.0f);

        bool r_in = hw.jacks[Dripline::JACK_SWITCH_RIGHT_IN].Pressed();
        bool r_out = hw.jacks[Dripline::JACK_SWITCH_RIGHT_OUT].Pressed();
        hw.SetLed(Dripline::LED_2, r_in ? 1.0f : 0.0f, 0.0f, r_out ? 1.0f : 0.0f);

        hw.UpdateLeds();

        hw.DelayMs(10);
    }
}
