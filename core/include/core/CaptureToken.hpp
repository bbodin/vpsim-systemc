#ifndef VPSIM_CORE_CAPTURETOKEN_HPP
#define VPSIM_CORE_CAPTURETOKEN_HPP

#include <string>

namespace vpsim {

    enum CaptureAction : uint8_t {
        ACTION_NONE = 0,
        ACTION_BENCHMARK_START = 1,
        ACTION_BENCHMARK_STOP  = 2,
        ACTION_SNAPSHOT         = 3
    };

    constexpr uint64_t ACTION_SHIFT = 56;
    constexpr uint64_t COUNTER_MASK = 0x00FFFFFFFFFFFFFFULL;


    typedef uint64_t CaptureToken;

    inline CaptureToken makeCaptureToken(CaptureAction action, uint64_t counter)
    {
        return (static_cast<uint64_t>(action) << ACTION_SHIFT) |
            (counter & COUNTER_MASK);
    }


    inline CaptureAction getCaptureAction(CaptureToken token)
    {
        return static_cast<CaptureAction>(token >> ACTION_SHIFT);
    }


    inline uint64_t getCaptureCounter(CaptureToken token)
    {
        return token & COUNTER_MASK;
    }

} // End of vpsim namespace

#endif