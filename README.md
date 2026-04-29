# FBS-Project

A Windows x64 C++17 project with a kernel-mode driver and a DirectX 11 overlay application.

## Architecture

```
FBS-Project/
├── src/               User-mode application (C++17, DirectX 11, ImGui)
├── include/           Headers, data structures, and third-party libraries
│   ├── cs2/           Auto-generated game offsets
│   └── imgui/         ImGui library
├── kernel/FBSKernel/  Kernel-mode driver (WDK)
├── output/            Generated offset dumps (multiple languages)
├── build.bat          Build the application
├── build_driver.bat   Build the kernel driver
└── Start_Cheat.bat    Load the driver via KDMapper
```

## Requirements

- **Visual Studio 2022 Community** with C++ Desktop Development workload
- **Windows Driver Kit (WDK)** for building the kernel driver
- **Windows SDK** (x64)

## Build Instructions

### 1. Build the Kernel Driver

```batch
build_driver.bat
```

This uses MSBuild to compile `FBSKernel.sln` in Release x64 mode.
Output: `kernel/FBSKernel/x64/Release/FBSKernel_V3.sys`

### 2. Build the Application

```batch
build.bat
```

This uses MSVC (cl.exe) with C++17 and links against DirectX 11.
Output: `bin/d3d11_helper.exe`

### 3. Run

```batch
Start_Cheat.bat
```

Requires Administrator privileges. Uses KDMapper to load the driver.

## Features

### Overlay (DirectX 11 + ImGui)
- Transparent fullscreen window
- Configurable menu with sidebar navigation (INSERT to toggle)

### ESP (Extra Sensory Perception)
- Bounding boxes (Corner / Filled / 2D)
- Health & armor bars
- Skeleton rendering (15 bone connections)
- Player names, distance, money
- Dropped weapons display
- C4 bomb location & timer
- Snaplines, head dots

### Combat
- **Aimbot**: FOV-based targeting, smoothing, bone selection, recoil compensation
- **RCS**: Delta-based recoil control with decay tracking
- **Triggerbot**: Configurable delay, headshot-only mode

### Movement
- **BunnyHop**: Kernel-level scroll wheel injection with strafe assist

### Configuration
- Save/Load settings to config files
- Per-feature toggles and parameter tuning
- KeyBind system for custom hotkeys

### Communication
- **Shared Memory** (fast path): Spin-lock polling for minimal latency
- **IOCTL** (fallback): Standard DeviceIoControl

## Threading Model

```
Main Thread (Render Loop)
├── ESP rendering
├── Menu drawing
└── Weapon/Bomb ESP

Background Threads:
├── Entity update thread (~200Hz)
├── BunnyHop thread (~1000Hz)
├── Triggerbot thread (~1000Hz)
└── Aimbot + RCS thread (~1000Hz)
```

## Configuration

Settings are saved in `.cfg` files. Use the System Config tab in the menu to save/load configurations.

## Project Structure

| Component | Description |
|---|---|
| `KernelInterface` | Driver communication (SHM + IOCTL) |
| `Memory` | Process memory management |
| `EntityManager` | Double-buffered entity cache with mutex |
| `ESP` | Screen-space rendering functions |
| `Overlay` | DirectX 11 transparent window |
| `Menu` | ImGui-based configuration UI |
| `ConfigSystem` | Settings persistence |
| `Animation` | Smooth interpolation utilities |
