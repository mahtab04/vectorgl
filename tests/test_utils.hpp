#pragma once

#include <cmath>
#include <stdexcept>
#include <string>

inline void expect(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

inline void expectNear(float actual, float expected, float tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
    {
        throw std::runtime_error(std::string(message) + ": expected " + std::to_string(expected) + ", got " +
                                 std::to_string(actual));
    }
}
