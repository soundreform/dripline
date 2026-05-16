#include "daisy.h"
#include "daisysp.h"
#include "dripline.h"
#include "bypass.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using daisysp::ChorusEngine;
using daisysp::CrossFade;
using daisysp::DelayLine;
using dripline::AudioBypass;
using dripline::Dripline;

Dripline hw;
AudioBypass bypass;
ChorusEngine chorus;

bool bypassed = true;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();

    bypassed ^= hw.footswitches[Dripline::FOOTSWITCH_1].RisingEdge();
    bypass.SetBypass(bypassed);

    for (size_t i = 0; i < size; i++)
    {
        out[0][i] = chorus.Process(in[1][i]) * bypass.Process();
        out[1][i] = 0.0f;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    bypass.Init(&hw, Dripline::CHANNEL_LEFT);

    chorus.Init(hw.AudioSampleRate());
    chorus.SetDelayMs(10.0f);
    chorus.SetFeedback(0.25f);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true)
    {
        if (bypassed)
        {

            hw.SetLed(Dripline::LED_1, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            hw.SetLed(Dripline::LED_1, 1.0f, 0.0f, 0.0f);
        }

        hw.UpdateLeds();

        hw.DelayMs(10);
    }
}
