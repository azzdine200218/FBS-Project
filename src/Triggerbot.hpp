#pragma once
#include <windows.h>
#include <cstdint>
#include "../include/Data.hpp"
#include "../include/KernelInterface.hpp"
#include "Menu.hpp"
#include "../include/EntityManager.hpp"

namespace Triggerbot {
    void Execute(KernelInterface& kernel, ULONG pid, uint64_t clientBase, uint64_t localPlayerPawn, EntityManager& entMgr, const Menu& menu);
}
