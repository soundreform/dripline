#pragma once
#ifndef DL_FRNG_H
#define DL_FRNG_H

#include <cstdint>

namespace dripline {

class FastRandom {
  public:
    FastRandom() = default;
    ~FastRandom() = default;

    void Init(uint32_t seed) { state_ = (seed == 0) ? 1 : seed; }

    uint32_t Next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    float NextFloat() { return static_cast<float>(Next()) * 2.3283064365386962890625e-10f; }

  private:
    uint32_t state_ = 1;
};

}; // namespace dripline

#endif
