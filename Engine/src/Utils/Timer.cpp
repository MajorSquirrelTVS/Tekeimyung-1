#include "Utils/Timer.hpp"
#include <GLFW/glfw3.h>

Timer::Timer()
{
    reset();
}

Timer::~Timer() {}

void    Timer::reset()
{
    _lastReset = glfwGetTime();
}

double  Timer::getElapsedTime() const
{
    return (glfwGetTime() - _lastReset);
}