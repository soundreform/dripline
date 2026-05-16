#include "daisy.h"
#include "dripline.h"
#include "qparameter.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using dripline::Dripline;
using dripline::QParameter;

Dripline hw;
QParameter knob_quant;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    // Process hardware inputs (knobs/switches)
    hw.ProcessAllControls();
    // Update quantization based on the current state of KNOB_1
    knob_quant.Process();

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

    // Initialize the quantizer to map KNOB_1 into 8 discrete bins (0-7)
    knob_quant.Init(hw.knobs[Dripline::KNOB_1], 8);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true)
    {
        // Retrieve the current quantized value
        int bin = knob_quant.Value();

        // Map the 8 bins to distinct colors on LED_1 to show status
        switch (bin)
        {
        case 0:
            hw.SetLed(Dripline::LED_1, 0.0f, 0.0f, 0.0f);
            break; // Off
        case 1:
            hw.SetLed(Dripline::LED_1, 1.0f, 0.0f, 0.0f);
            break; // Red
        case 2:
            hw.SetLed(Dripline::LED_1, 1.0f, 0.3f, 0.0f);
            break; // Orange
        case 3:
            hw.SetLed(Dripline::LED_1, 1.0f, 1.0f, 0.0f);
            break; // Yellow
        case 4:
            hw.SetLed(Dripline::LED_1, 0.0f, 1.0f, 0.0f);
            break; // Green
        case 5:
            hw.SetLed(Dripline::LED_1, 0.0f, 0.0f, 1.0f);
            break; // Blue
        case 6:
            hw.SetLed(Dripline::LED_1, 0.5f, 0.0f, 1.0f);
            break; // Purple
        case 7:
            hw.SetLed(Dripline::LED_1, 1.0f, 1.0f, 1.0f);
            break; // White
        default:
            break;
        }

        // If the bin is above 0, let's use LED_2 as a secondary "active" indicator
        if (bin > 0)
        {
            hw.SetLed(Dripline::LED_2, 0.5f, 0.5f, 0.5f); // Dim white
        }
        else
        {
            hw.SetLed(Dripline::LED_2, 0.0f, 0.0f, 0.0f);
        }

        // Transmit the LED data to the PCA9685 driver via I2C DMA
        hw.UpdateLeds();

        // Small delay to prevent saturating the I2C bus
        // and to give the UI loop room to breathe
        hw.DelayMs(10);
    }
}
