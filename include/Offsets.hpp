#pragma once
#include <cstdint>
#include <cstddef>

// Include the raw dumps directly from a2x/cs2-dumper
#include "cs2/offsets.hpp"
#include "cs2/client_dll.hpp"
#include "cs2/buttons.hpp"

// We can define aliases if we want, but using cs2_dumper::offsets and cs2_dumper::offsets::client_dll
// directly gives us 100% access to every single offset (bones, weapons, recoil, etc.)
namespace offsets {
    using namespace cs2_dumper::offsets::client_dll;
    using namespace cs2_dumper::schemas::client_dll;
    using namespace cs2_dumper::buttons;

    // Common Schema Offsets for easier access
    constexpr std::ptrdiff_t m_pGameSceneNode = cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode;
    constexpr std::ptrdiff_t m_modelState = cs2_dumper::schemas::client_dll::CGameSceneNode::m_modelState;
    constexpr std::ptrdiff_t m_hPawn = cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn;
}
