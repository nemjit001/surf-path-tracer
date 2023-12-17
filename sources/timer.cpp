#include "timer.h"

#include <chrono>

#include "types.h"

Timer::Timer()
    :
    m_last(std::chrono::steady_clock::now())
{
    tick();
}

void Timer::tick()
{
    TimePoint now = std::chrono::steady_clock::now();
    std::chrono::duration<F32> diff = now - m_last;
    m_delta = diff.count();
    m_last = now;
}
