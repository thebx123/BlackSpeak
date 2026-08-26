@echo off
setlocal enabledelayedexpansion
title BlackSpeak - Auto Publisher to GitHub

echo ===================================================
echo   BlackSpeak - Automated Build ^& GitHub Publisher
echo ===================================================
echo.

:: Step 1: Run unified builder to synchronize .env and build .ts3_addon
echo [*] Building and synchronizing project assets from .env...
python "%~dp0build.py"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] Build failed! Please fix compilation errors before uploading.
    echo ===================================================
    pause
    exit /b %ERRORLEVEL%
)

:: Step 2: Extract version from .env or .env.example
set "ENV_FILE=%~dp0.env"
if not exist "!ENV_FILE!" set "ENV_FILE=%~dp0.env.example"
set "VERSION=1.0.0"
if exist "!ENV_FILE!" (
    for /f "tokens=1,2 delims==" %%a in (!ENV_FILE!) do (
        set "KEY=%%a"
        set "VAL=%%b"
        if "!KEY!"=="PLUGIN_VERSION" set "VERSION=!VAL!"
    )
)

echo.
echo [*] Staging all files for Git...
git add -A

:: Step 3: Check if git has anything new to commit
git diff --cached --quiet
if %ERRORLEVEL% EQU 0 (
    echo [*] Working tree clean.
) else (
    set "COMMIT_MSG=Release v!VERSION!: Automated build and update sync"
    echo [*] Committing changes: "!COMMIT_MSG!"
    git commit -m "!COMMIT_MSG!"
)

:: Step 4: Create / Update version tag
set "TAG_NAME=v!VERSION!"
echo [*] Tagging release as !TAG_NAME!...
git tag -f !TAG_NAME! -m "Release !TAG_NAME!"

:: Step 5: Push branch and tags to GitHub
echo.
echo [*] Pushing updates and release tag to GitHub...
git push origin main --tags --force
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] Git push failed. Please verify your internet connection or GitHub authentication.
    echo ===================================================
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo [✓] Successfully uploaded and created Release on GitHub!
echo [*] GitHub Actions is now publishing the Release assets.
echo [*] Repository: https://github.com/thebx123/BlackSpeak
echo [*] Releases:   https://github.com/thebx123/BlackSpeak/releases
echo ===================================================
echo.
pause
