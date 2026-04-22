@echo off
set /p msg="Enter commit message (press Enter for default 'Auto update'): "
if "%msg%"=="" set msg=Auto update %date% %time%

echo Adding changes...
git add .

echo Committing...
git commit -m "%msg%"

echo Pushing to GitHub...
git push origin main

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Push failed. Check your connection or repository status.
) else (
    echo.
    echo [SUCCESS] Changes uploaded successfully!
)

pause
