@echo off
setlocal

set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community"
if not exist "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" (
    exit /b 1
)

call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64

set PROJECT_DIR=%~dp0
set SRC_DIR=%PROJECT_DIR%src
set INC_DIR=%PROJECT_DIR%include
set IMGUI_DIR=%PROJECT_DIR%include\imgui
set OUT_DIR=%PROJECT_DIR%bin

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

set CFLAGS=/nologo /O2 /W3 /MT /EHsc /std:c++17 /I "%PROJECT_DIR%." /I "%IMGUI_DIR%"
set LDFLAGS=/link /OUT:"%OUT_DIR%\d3d11_helper.exe"

cd "%IMGUI_DIR%"
cl /nologo /O2 /W3 /EHsc /std:c++17 /c imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp imgui_impl_dx11.cpp imgui_impl_win32.cpp

cd "%SRC_DIR%"
cl %CFLAGS% main.cpp KernelInterface.cpp Memory.cpp Overlay.cpp Menu.cpp ESP.cpp Triggerbot.cpp "%IMGUI_DIR%\*.obj" %LDFLAGS%

if %errorlevel% neq 0 (
    exit /b %errorlevel%
)

endlocal
