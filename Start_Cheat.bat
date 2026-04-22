@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo         FBS Kernel Stealth Loader (KDMapper)
echo           Bypasses Driver Signature Enforcement
echo ========================================================
echo.

:: Check Admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] Please run this script as Administrator!
    pause
    exit /b 1
)

:: Ensure Driver Exists
set DRIVER_PATH="%~dp0kernel\FBSKernel\x64\Release\FBSKernel_V3.sys"
if not exist %DRIVER_PATH% (
    echo [ERROR] Driver not found at %DRIVER_PATH%
    echo Please build the project first!
    pause
    exit /b 1
)

:: Check for KDmapper
if not exist kdmapper.exe (
    echo [INFO] KDMapper not found. Downloading the latest release...
    curl -L -o kdmapper.exe "https://github.com/TheCruZ/kdmapper/releases/download/v2.0/kdmapper.exe"
    
    if not exist kdmapper.exe (
        echo [ERROR] Failed to download KDMapper. Please download it manually from:
        echo https://github.com/TheCruZ/kdmapper/releases
        echo Place kdmapper.exe in this folder.
        pause
        exit /b 1
    )
    echo [SUCCESS] Download complete!
)

echo.
echo [WARNING] Make sure your Anti-Virus/Windows Defender is OFF.
echo [WARNING] KDMapper exploits a driver which gets detected by AV.
echo.
pause

echo.
echo [STATUS] Mapping FBSKernel_V3.sys stealthily into memory...
kdmapper.exe %DRIVER_PATH%

echo.
if %errorLevel% equ 0 (
    echo [SUCCESS] Driver mapped successfully! You can now start the cheat.
) else (
    echo [ERROR] Mapping failed. Check the KDMapper logs above.
)
pause
