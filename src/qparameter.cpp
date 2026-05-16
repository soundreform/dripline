#include "qparameter.h"

using dripline::QParameter;

void QParameter::Init(AnalogControl input, int num_bins)
{
    in_ = input;
    num_bins_ = num_bins > 0 ? num_bins : 1;
    val_ = -1; // Initialize to -1 to allow an initial snap on first Process()
}

int QParameter::Process()
{
    const float raw = in_.Process();
    int potential_new_val = static_cast<int>(raw * (float)num_bins_);

    if (potential_new_val >= num_bins_)
        potential_new_val = num_bins_ - 1;
    if (potential_new_val < 0)
        potential_new_val = 0;

    if (val_ < 0)
    {
        val_ = potential_new_val;
    }
    else if (potential_new_val != val_)
    {
        const float bin_size = 1.0f / (float)num_bins_;
        const float margin = bin_size * 0.1f; // 10% bin width hysteresis margin

        if (potential_new_val > val_)
        {
            if (raw > (float)potential_new_val * bin_size + margin)
                val_ = potential_new_val;
        }
        else
        {
            if (raw < (float)(potential_new_val + 1) * bin_size - margin)
                val_ = potential_new_val;
        }
    }

    return val_;
}
