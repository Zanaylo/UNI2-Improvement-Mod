#pragma once

#include "Core/utils.h"

#include <intrin.h>

#include <cstdint>

#define IS_GAME_CALLER() IsAddressInGameModule(reinterpret_cast<uintptr_t>(_ReturnAddress()))
