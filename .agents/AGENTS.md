# Project Guidelines & Rules

## 1. Language & Localization Standard (Strict English-Only)
- **Code & Comments:** All C++, Python, QSS, TPL, and batch scripts, including all code comments, variable names, functions, classes, and debug messages, MUST be written exclusively in **English**.
- **Documentation & Repository Files:** All repository files such as `README.md`, `package.ini`, `version.json`, `.env.example`, and configuration manifests MUST be written strictly in **English**.
- **Git & GitHub Operations:** All commit messages, pull requests, release descriptions, tags, and issue discussions MUST be authored exclusively in **English**.
- **No Non-English Text:** No foreign language characters (including Persian, Arabic, etc.) are allowed within codebases, comments, or documentation files in this repository.

## 2. Architecture & Packaging Guidelines
- **Unified Architecture:** The project uses a single unified C++ plugin (`plugins/modern_black.cpp` -> `plugins/modern_black_win64.dll`) and a single `.ts3_addon` distribution package (`ModernBlack.ts3_addon`).
- **Version Control with `.env`:** Versioning and auto-update endpoints are dynamically managed via `.env` and synchronized automatically by `build.py`.
