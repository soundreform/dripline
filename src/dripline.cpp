#include "daisy.h"
#include "daisysp.h"
#include "dripline.h"

using daisy::AdcChannelConfig;
using daisy::I2CHandle;
using daisy::LedDriverPca9685;
using daisy::Pin;
using daisy::System;
using daisysp::fclamp;
using dripline::DigitalControl;
using dripline::Dripline;

static constexpr Pin PIN_LED_ENABLED = daisy::seed::D0;
static constexpr Pin PIN_D1_UNUSED = daisy::seed::D1;
static constexpr Pin PIN_D2_UNUSED = daisy::seed::D2;
static constexpr Pin PIN_FOOTSWITCH_1 = daisy::seed::D3;
static constexpr Pin PIN_RELAY_LEFT = daisy::seed::D4;
static constexpr Pin PIN_JACK_SWITCH_LEFT_OUT = daisy::seed::D5;
static constexpr Pin PIN_JACK_SWITCH_LEFT_IN = daisy::seed::D6;
static constexpr Pin PIN_KNOB_MUX_2_C = daisy::seed::D7;
static constexpr Pin PIN_KNOB_MUX_2_B = daisy::seed::D8;
static constexpr Pin PIN_KNOB_MUX_2_A = daisy::seed::D9;
static constexpr Pin PIN_D10_UNUSED = daisy::seed::D10;
static constexpr Pin PIN_LED_SCL = daisy::seed::D11;
static constexpr Pin PIN_LED_SDA = daisy::seed::D12;
static constexpr Pin PIN_MUTE_LEFT = daisy::seed::D13;
static constexpr Pin PIN_D14_UNUSED = daisy::seed::D14;
static constexpr Pin PIN_D15_UNUSED = daisy::seed::D15;
static constexpr Pin PIN_EXPRESSION = daisy::seed::D16;
static constexpr Pin PIN_D17_UNUSED = daisy::seed::D17;
static constexpr Pin PIN_TOGGLE_MUX_CLOCK = daisy::seed::D18;
static constexpr Pin PIN_TOGGLE_MUX_LOAD = daisy::seed::D19;
static constexpr Pin PIN_MUTE_RIGHT = daisy::seed::D20;
static constexpr Pin PIN_TOGGLE_MUX_COM = daisy::seed::D21;
static constexpr Pin PIN_KNOB_MUX_2_COM = daisy::seed::D22;
static constexpr Pin PIN_KNOB_MUX_1_COM = daisy::seed::D23;
static constexpr Pin PIN_KNOB_MUX_1_A = daisy::seed::D24;
static constexpr Pin PIN_KNOB_MUX_1_B = daisy::seed::D25;
static constexpr Pin PIN_KNOB_MUX_1_C = daisy::seed::D26;
static constexpr Pin PIN_JACK_SWITCH_RIGHT_OUT = daisy::seed::D27;
static constexpr Pin PIN_JACK_SWITCH_RIGHT_IN = daisy::seed::D28;
static constexpr Pin PIN_RELAY_RIGHT = daisy::seed::D29;
static constexpr Pin PIN_FOOTSWITCH_2 = daisy::seed::D30;

static constexpr size_t ADDR_KNOB_MUX_1 = 0;
static constexpr size_t ADDR_KNOB_MUX_2 = 1;
static constexpr size_t ADDR_KNOB_1 = 1;
static constexpr size_t ADDR_KNOB_2 = 2;
static constexpr size_t ADDR_KNOB_3 = 4;
static constexpr size_t ADDR_KNOB_4 = 2;
static constexpr size_t ADDR_KNOB_5 = 4;
static constexpr size_t ADDR_KNOB_6 = 6;
static constexpr size_t ADDR_KNOB_7 = 0;
static constexpr size_t ADDR_KNOB_8 = 6;
static constexpr size_t ADDR_KNOB_9 = 7;
static constexpr size_t ADDR_KNOB_10 = 0;
static constexpr size_t ADDR_KNOB_11 = 1;
static constexpr size_t ADDR_KNOB_12 = 7;

static constexpr size_t knob_addrs[Dripline::KNOB_LAST][2] = {
    {ADDR_KNOB_MUX_2, ADDR_KNOB_1},
    {ADDR_KNOB_MUX_2, ADDR_KNOB_2},
    {ADDR_KNOB_MUX_2, ADDR_KNOB_3},
    {ADDR_KNOB_MUX_1, ADDR_KNOB_4},
    {ADDR_KNOB_MUX_1, ADDR_KNOB_5},
    {ADDR_KNOB_MUX_1, ADDR_KNOB_6},
    {ADDR_KNOB_MUX_2, ADDR_KNOB_7},
    {ADDR_KNOB_MUX_2, ADDR_KNOB_8},
    {ADDR_KNOB_MUX_2, ADDR_KNOB_9},
    {ADDR_KNOB_MUX_1, ADDR_KNOB_10},
    {ADDR_KNOB_MUX_1, ADDR_KNOB_11},
    {ADDR_KNOB_MUX_1, ADDR_KNOB_12},
};

static constexpr size_t ADDR_TOGGLE_1_UP = 15;
static constexpr size_t ADDR_TOGGLE_1_DOWN = 14;
static constexpr size_t ADDR_TOGGLE_2_UP = 13;
static constexpr size_t ADDR_TOGGLE_2_DOWN = 12;
static constexpr size_t ADDR_TOGGLE_3_UP = 9;
static constexpr size_t ADDR_TOGGLE_3_DOWN = 10;
static constexpr size_t ADDR_TOGGLE_4_UP = 6;
static constexpr size_t ADDR_TOGGLE_4_DOWN = 5;
static constexpr size_t ADDR_TOGGLE_5_UP = 2;
static constexpr size_t ADDR_TOGGLE_5_DOWN = 3;
static constexpr size_t ADDR_TOGGLE_6_UP = 0;
static constexpr size_t ADDR_TOGGLE_6_DOWN = 1;

static constexpr size_t toggle_addrs[Dripline::TOGGLE_SWITCH_LAST][2] = {
    {ADDR_TOGGLE_1_UP, ADDR_TOGGLE_1_DOWN},
    {ADDR_TOGGLE_2_UP, ADDR_TOGGLE_2_DOWN},
    {ADDR_TOGGLE_3_UP, ADDR_TOGGLE_3_DOWN},
    {ADDR_TOGGLE_4_UP, ADDR_TOGGLE_4_DOWN},
    {ADDR_TOGGLE_5_UP, ADDR_TOGGLE_5_DOWN},
    {ADDR_TOGGLE_6_UP, ADDR_TOGGLE_6_DOWN},
};

static constexpr Pin footswitch_addrs[Dripline::FOOTSWITCH_LAST] = {
    PIN_FOOTSWITCH_1,
    PIN_FOOTSWITCH_2,
};

static constexpr Pin jack_addrs[Dripline::JACK_SWITCH_LAST] = {
    PIN_JACK_SWITCH_LEFT_IN,
    PIN_JACK_SWITCH_RIGHT_IN,
    PIN_JACK_SWITCH_LEFT_OUT,
    PIN_JACK_SWITCH_RIGHT_OUT,
};

static constexpr Pin relay_addrs[Dripline::CHANNEL_LAST] = {
    PIN_RELAY_LEFT,
    PIN_RELAY_RIGHT,
};

static constexpr Pin mute_addrs[Dripline::CHANNEL_LAST] = {
    PIN_MUTE_LEFT,
    PIN_MUTE_RIGHT,
};

static constexpr size_t ADDR_LED_2_R = 5;
static constexpr size_t ADDR_LED_2_G = 3;
static constexpr size_t ADDR_LED_2_B = 1;
static constexpr size_t ADDR_LED_1_R = 14;
static constexpr size_t ADDR_LED_1_G = 12;
static constexpr size_t ADDR_LED_1_B = 10;

static constexpr size_t led_addrs[Dripline::LED_LAST][3] = {
    {ADDR_LED_1_R, ADDR_LED_1_G, ADDR_LED_1_B},
    {ADDR_LED_2_R, ADDR_LED_2_G, ADDR_LED_2_B},
};

static constexpr I2CHandle::Config led_i2c_config = {
    periph : I2CHandle::Config::Peripheral::I2C_1,
    pin_config : {
        scl : PIN_LED_SCL,
        sda : PIN_LED_SDA,
    },
    speed : I2CHandle::Config::Speed::I2C_1MHZ,
};

static LedDriverPca9685<1, true>::DmaBuffer DMA_BUFFER_MEM_SECTION led_dma_buffer_a, led_dma_buffer_b;

void Dripline::Init(bool boost)
{
    seed.Init(boost);
    SetAudioBlockSize(48);
    InitLeds();
    InitAnalogControls();
    InitDigitalControls();
    InitBypasses();
}

void Dripline::DelayMs(size_t del)
{
    seed.DelayMs(del);
}

void Dripline::StartAudio(AudioHandle::AudioCallback cb)
{
    seed.StartAudio(cb);
}

void Dripline::ChangeAudioCallback(AudioHandle::AudioCallback cb)
{
    seed.ChangeAudioCallback(cb);
}

void Dripline::StopAudio()
{
    seed.StopAudio();
}

void Dripline::SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate)
{
    seed.SetAudioSampleRate(samplerate);
    SetHidUpdateRates();
}

float Dripline::AudioSampleRate()
{
    return seed.AudioSampleRate();
}

void Dripline::SetAudioBlockSize(size_t size)
{
    seed.SetAudioBlockSize(size);
    SetHidUpdateRates();
}

size_t Dripline::AudioBlockSize()
{
    return seed.AudioBlockSize();
}

float Dripline::AudioCallbackRate()
{
    return seed.AudioCallbackRate();
}

void Dripline::StartAdc()
{
    seed.adc.Start();
}

void Dripline::StopAdc()
{
    seed.adc.Stop();
}

void Dripline::ProcessAnalogControls()
{
    for (size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Process();
    }
    expression.Process();
}

void Dripline::ProcessDigitalControls()
{
    toggle_sr_.Update();

    for (size_t i = 0; i < FOOTSWITCH_LAST; i++)
    {
        footswitches[i].Debounce();
    }

    for (size_t i = 0; i < JACK_SWITCH_LAST; i++)
    {
        jacks[i].Debounce();
    }
}

void Dripline::SetHidUpdateRates()
{
    for (size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].SetSampleRate(AudioCallbackRate());
    }

    expression.SetSampleRate(AudioCallbackRate());
}

void Dripline::InitAnalogControls()
{
    AdcChannelConfig cfg[3];
    cfg[0].InitMux(PIN_KNOB_MUX_1_COM, 8, PIN_KNOB_MUX_1_A, PIN_KNOB_MUX_1_B, PIN_KNOB_MUX_1_C);
    cfg[1].InitMux(PIN_KNOB_MUX_2_COM, 8, PIN_KNOB_MUX_2_A, PIN_KNOB_MUX_2_B, PIN_KNOB_MUX_2_C);
    cfg[2].InitSingle(PIN_EXPRESSION);
    seed.adc.Init(cfg, 3);

    float acr = AudioCallbackRate();

    for (size_t i = 0; i < KNOB_LAST; i++)
    {
        knobs[i].Init(seed.adc.GetMuxPtr(knob_addrs[i][0], knob_addrs[i][1]), acr);
    }

    expression.Init(seed.adc.GetPtr(2), acr);
}

void Dripline::InitDigitalControls()
{
    toggle_sr_.Init(PIN_TOGGLE_MUX_CLOCK, PIN_TOGGLE_MUX_COM, PIN_TOGGLE_MUX_LOAD);

    for (size_t i = 0; i < TOGGLE_SWITCH_LAST; i++)
    {
        toggles[i].Init(&toggle_sr_, toggle_addrs[i][0], toggle_addrs[i][1]);
    }

    for (size_t i = 0; i < FOOTSWITCH_LAST; i++)
    {
        footswitches[i].Init(footswitch_addrs[i]);
    }

    for (size_t i = 0; i < JACK_SWITCH_LAST; i++)
    {
        jacks[i].Init(jack_addrs[i]);
    }
}

void Dripline::InitLeds()
{
    led_power_.Init(PIN_LED_ENABLED, GPIO::Mode::OUTPUT);
    led_power_.Write(false);

    uint8_t addr[1] = {0x00};
    I2CHandle i2c;
    i2c.Init(led_i2c_config);
    led_driver_.Init(i2c, addr, led_dma_buffer_a, led_dma_buffer_b);

    SetLed(LED_1, 0.0f, 0.0f, 0.0f);
    SetLed(LED_2, 0.0f, 0.0f, 0.0f);
    UpdateLeds();

    DelayMs(10.0f);
    led_power_.Write(true);
}

void Dripline::InitBypasses()
{
    for (size_t i = 0; i < CHANNEL_LAST; i++)
    {
        relays[i].Init(relay_addrs[i], GPIO::Mode::OUTPUT);
        relays[i].Write(true);

        mutes[i].Init(mute_addrs[i], GPIO::Mode::OUTPUT);
        mutes[i].Write(false);
    }
}

void Dripline::UpdateLeds()
{
    led_driver_.SwapBuffersAndTransmit();
}

void Dripline::SetLed(Led led, float r, float g, float b)
{
    led_driver_.SetLed(led_addrs[led][0], static_cast<uint8_t>(fclamp(r, 0.0f, 1.0f) * 255));
    led_driver_.SetLed(led_addrs[led][1], static_cast<uint8_t>(fclamp(g, 0.0f, 1.0f) * 255));
    led_driver_.SetLed(led_addrs[led][2], static_cast<uint8_t>(fclamp(b, 0.0f, 1.0f) * 255));
}

void DigitalControl::Init(ShiftRegister165<2> *sr, uint8_t up_idx, uint8_t down_idx)
{
    sr_ = sr;
    up_idx_ = up_idx;
    down_idx_ = down_idx;
}

DigitalControl::Position DigitalControl::GetPosition()
{
    bool up = sr_->Get(up_idx_);
    bool down = sr_->Get(down_idx_);

    if (up && !down)
    {
        return Position::UP;
    }
    else if (!up && down)
    {
        return Position::DOWN;
    }
    else
    {
        return Position::CENTER;
    }
}
