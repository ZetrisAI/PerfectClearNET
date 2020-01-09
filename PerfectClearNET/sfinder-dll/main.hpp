#ifndef MAIN_H
#define MAIN_H

#include "Windows.h"
#define DLL extern "C" __declspec(dllexport)

#include <sstream>
#include <vector>

#include "callback.hpp"
#include "core/field.hpp"
#include "finder/perfect_clear.hpp"

core::Factory factory = core::Factory::create();
core::srs::MoveGenerator moveGenerator = core::srs::MoveGenerator(factory);
finder::PerfectClearFinder<core::srs::MoveGenerator> pcfinder = finder::PerfectClearFinder<core::srs::MoveGenerator>(factory, moveGenerator);

static const unsigned char BitsSetTable256[256] = 
{
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
};

#endif