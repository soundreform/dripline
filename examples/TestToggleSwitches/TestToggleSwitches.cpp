#include "daisy.h"
#include "dripline.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using dripline::DigitalControl;
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

float MapBrightness(DigitalControl::Position pos)
{
    switch (pos)
    {
    case DigitalControl::Position::CENTER:
        return 0.5f;
    case DigitalControl::Position::UP:
        return 1.0f;
    case DigitalControl::Position::DOWN:
    default:
        return 0.0f;
    }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true)
    {
        hw.ProcessAllControls();

        // Read the state of each toggle switch
        DigitalControl::Position ts1 = hw.toggles[Dripline::TOGGLE_SWITCH_1].GetPosition();
        DigitalControl::Position ts2 = hw.toggles[Dripline::TOGGLE_SWITCH_2].GetPosition();
        DigitalControl::Position ts3 = hw.toggles[Dripline::TOGGLE_SWITCH_3].GetPosition();
        DigitalControl::Position ts4 = hw.toggles[Dripline::TOGGLE_SWITCH_4].GetPosition();
        DigitalControl::Position ts5 = hw.toggles[Dripline::TOGGLE_SWITCH_5].GetPosition();
        DigitalControl::Position ts6 = hw.toggles[Dripline::TOGGLE_SWITCH_6].GetPosition();

        // Map toggle switch states to LED_1 RGB components
        hw.SetLed(Dripline::LED_1, MapBrightness(ts1), MapBrightness(ts2), MapBrightness(ts3));

        // Map toggle switch states to LED_2 RGB components
        hw.SetLed(Dripline::LED_2, MapBrightness(ts4), MapBrightness(ts5), MapBrightness(ts6));

        // Update the LEDs
        hw.UpdateLeds();

        hw.DelayMs(10);
    }
}
