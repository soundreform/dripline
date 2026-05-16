#include "bypass.h"
#include <cmath>

using dripline::AudioBypass;

void AudioBypass::Init(Dripline *hw, Dripline::Channel channel)
{
    Init(hw->AudioSampleRate(), &hw->relays[channel], &hw->mutes[channel]);
}

void AudioBypass::Init(float sample_rate, GPIO *relay, GPIO *mute)
{
    relay_ = relay;
    mute_ = mute;

    bypassed_ = true;
    transitioning_ = false;
    gain_ = 0.0f;

    mute_->Write(false);
    relay_->Write(false);

    fade_in_ramp_.Init(sample_rate, 10.0f, Ramp::Dir::UP);
    fade_out_ramp_.Init(sample_rate, 2.0f, Ramp::Dir::DOWN);
    mute_wait_ramp_.Init(sample_rate, 3.0f, Ramp::Dir::UP);
    relay_wait_ramp_.Init(sample_rate, 2.0f, Ramp::Dir::UP);
}

float AudioBypass::Process()
{
    TransitionHardware();
    return gain_;
}

float AudioBypass::Value()
{
    return gain_;
}

void AudioBypass::SetBypass(bool bypass)
{
    if (bypassed_ != bypass)
    {
        bypassed_ = bypass;
        transitioning_ = true;
    }
}

void AudioBypass::TransitionHardware()
{
    if (!transitioning_)
    {
        return;
    }

    if (bypassed_ && fade_out_ramp_.Process() > 0.0f)
    {
        gain_ = fade_out_ramp_.Value();
        return;
    }

    mute_->Write(true);

    if (relay_wait_ramp_.Process() < 1.0f)
    {
        return;
    }

    relay_->Write(!bypassed_);

    if (mute_wait_ramp_.Process() < 1.0f)
    {
        return;
    }

    mute_->Write(false);

    if (!bypassed_ && fade_in_ramp_.Process() < 1.0f)
    {
        gain_ = fade_in_ramp_.Value();
        return;
    }

    fade_in_ramp_.Reset();
    fade_out_ramp_.Reset();
    mute_wait_ramp_.Reset();
    relay_wait_ramp_.Reset();

    gain_ = bypassed_ ? 0.0f : 1.0f;
    transitioning_ = false;
}
