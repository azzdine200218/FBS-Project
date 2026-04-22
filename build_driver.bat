@echo off
setlocal

echo ====================================
echo        FBS Kernel Compiler
echo ====================================
echo.

:: Try to find MSBuild for Visual Studio 2022
set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%MSBUILD_PATH%" (
    echo [ERROR] Could not find MSBuild.exe!
    echo Ensure Visual Studio 2022 Community and the Windows Driver Kit WDK are installed.
    exit /b 1
)

echo [INFO] Starting MSBuild for FBSKernel.sln...
"%MSBUILD_PATH%" "%~dp0kernel\FBSKernel\FBSKernel.sln" /t:Rebuild /p:Configuration="Release" /p:Platform="x64" /p:SpectreMitigation=false

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Driver Build Failed! Please check the MSBuild errors above.
    pause
    exit /b %errorlevel%
)

echo.
echo [SUCCESS] Driver built successfully!
echo Executable located at: kernel\FBSKernel\x64\Release\FBSKernel_V3.sys
echo.
endlocal
