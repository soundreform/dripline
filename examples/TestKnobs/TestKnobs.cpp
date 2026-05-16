#include <cmath>
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

    static float phase[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    constexpr float kMaxBlinkHz = 4.0f;
    constexpr float kTwoPi = 6.283185307179586f;
    const float dt = 0.01f; // 10ms loop

    while (true)
    {
        hw.ProcessAllControls();

        float blinkRates[6] = {
            hw.knobs[hw.KNOB_1].Value() * kMaxBlinkHz,
            hw.knobs[hw.KNOB_2].Value() * kMaxBlinkHz,
            hw.knobs[hw.KNOB_3].Value() * kMaxBlinkHz,
            hw.knobs[hw.KNOB_4].Value() * kMaxBlinkHz,
            hw.knobs[hw.KNOB_5].Value() * kMaxBlinkHz,
            hw.knobs[hw.KNOB_6].Value() * kMaxBlinkHz,
        };

        // Scale brightness slightly to prevent PWM overflow/flicker at 100% knob travel
        float baseColors[6] = {
            hw.knobs[hw.KNOB_7].Value() * 0.98f,
            hw.knobs[hw.KNOB_8].Value() * 0.98f,
            hw.knobs[hw.KNOB_9].Value() * 0.98f,
            hw.knobs[hw.KNOB_10].Value() * 0.98f,
            hw.knobs[hw.KNOB_11].Value() * 0.98f,
            hw.knobs[hw.KNOB_12].Value() * 0.98f,
        };

        float blink[6];
        for (size_t ch = 0; ch < 6; ch++)
        {
            if (blinkRates[ch] > 0.002f)
            {
                phase[ch] += blinkRates[ch] * dt;
                if (phase[ch] >= 1.0f)
                {
                    phase[ch] -= 1.0f;
                }
                blink[ch] = 0.5f * (1.0f + sinf(kTwoPi * phase[ch]));
            }
            else
            {
                blink[ch] = 1.0f;
                phase[ch] = 0.25f; // Reset phase to peak of sine wave (sin(pi/2) = 1)
            }
        }

        hw.SetLed(Dripline::LED_1,
                  baseColors[0] * blink[0],
                  baseColors[1] * blink[1],
                  baseColors[2] * blink[2]);

        hw.SetLed(Dripline::LED_2,
                  baseColors[3] * blink[3],
                  baseColors[4] * blink[4],
                  baseColors[5] * blink[5]);

        hw.UpdateLeds();
        hw.DelayMs(10);
    }
}
