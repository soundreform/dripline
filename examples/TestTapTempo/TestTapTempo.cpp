#include "daisy.h"
#include "dripline.h"
#include "dripline/tap.h"

using daisy::AudioHandle;
using daisy::SaiHandle;
using dripline::Dripline;
using dripline::TapTempo;

Dripline hw;
TapTempo tap;

// Variables for visual status feedback
float beat_phase = 0.0f;
bool tap_occurred = false;
int tap_flash_timer = 0;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    tap.Process(size);

    // Trigger a tap on the rising edge of Footswitch 1
    if (hw.footswitches[Dripline::FOOTSWITCH_1].RisingEdge()) {
        tap.Tap();
        tap_occurred = true;
    }

    // Track the phase of the beat (0.0 to 1.0) based on current tempo
    beat_phase += (float)size / tap.GetIntervalSamples();
    if (beat_phase >= 1.0f) {
        beat_phase -= 1.0f;
    }

    for (size_t i = 0; i < size; i++) {
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

int main(void) {
    hw.Init();
    hw.SetAudioBlockSize(48);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // Initialize TapTempo with the hardware sample rate
    tap.Init(hw.AudioSampleRate());

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while (true) {
        // LED 1: Pulse Magenta on the beat
        float pulse = (beat_phase < 0.15f) ? 1.0f : 0.0f;
        hw.SetLed(Dripline::LED_1, pulse, 0.0f, pulse);

        // LED 2: Flash Blue briefly when a manual tap is registered
        if (tap_occurred) {
            tap_flash_timer = 15; // ~150ms flash at 10ms loop rate
            tap_occurred = false;
        }

        if (tap_flash_timer > 0) {
            hw.SetLed(Dripline::LED_2, 0.0f, 0.0f, 1.0f);
            tap_flash_timer--;
        } else {
            hw.SetLed(Dripline::LED_2, 0.0f, 0.0f, 0.0f);
        }

        hw.UpdateLeds();
        hw.DelayMs(10);
    }
}
