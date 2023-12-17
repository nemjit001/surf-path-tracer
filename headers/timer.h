#pragma once

#include <chrono>

#include "types.h"

class Timer
{
public:
    Timer();

    void tick();

    inline F32 deltaTime() const
    {
        return m_delta;
    }

private:
    typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;

    TimePoint m_last;
    F32 m_delta;
};
