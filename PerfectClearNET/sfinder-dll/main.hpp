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

#endif