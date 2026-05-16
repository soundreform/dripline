#pragma once
#ifndef DL_QPARAMETER_H
#define DL_QPARAMETER_H

#include "daisy.h"
#include "hid/ctrl.h"

using daisy::AnalogControl;

namespace dripline
{
    /**
     * @brief Quantized parameter mapping tool.
     *
     * Maps a 0.0-1.0 input from an AnalogControl to discrete integer bins.
     * This is useful for mapping continuous controls like knobs to discrete
     * states or enum values.
     */
    class QParameter
    {
    public:
        QParameter() : num_bins_(1), val_(-1) {}
        ~QParameter() = default;

        /**
         * @brief Initializes the parameter.
         * @param input The AnalogControl hardware source.
         * @param num_bins The number of discrete bins to map to.
         */
        void Init(AnalogControl input, int num_bins);

        /**
         * @brief Processes the input and updates the bin value with hysteresis.
         * @return The current bin index (0 to num_bins - 1).
         */
        int Process();

        /**
         * @return The current bin value without processing another sample.
         */
        inline int Value() const { return val_; }

    private:
        AnalogControl in_;
        int num_bins_;
        int val_;
    };
}

#endif
