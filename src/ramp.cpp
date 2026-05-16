#include "ramp.h"

using dripline::Ramp;

void Ramp::Init(float sample_rate, float duration, Dir direction)
{
    val_ = 0.0f;
    step_ = 1.0f / ((duration / 1000.0f) * sample_rate);
    dir_ = direction;
    Reset();
}

float Ramp::Process()
{
    switch (dir_)
    {
    case Dir::UP:
        val_ += step_;
        if (val_ > 1.0f)
        {
            val_ = 1.0f;
        }
        break;
    case Dir::DOWN:
        val_ -= step_;
        if (val_ < 0.0f)
        {
            val_ = 0.0f;
        }
        break;
    }

    return val_;
}

float Ramp::Value()
{
    return val_;
}

void Ramp::Reset()
{
    val_ = dir_ == Dir::UP ? 0.0f : 1.0f;
}
