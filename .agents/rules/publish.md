---
trigger: manual
description: Automated publishing workflow triggered when user specifies publish:<version>
---

# BlackSpeak Automated Publishing Workflow

When the user enters `publish:<version>` (for example `publish:1.1.0`):

1. **Update `.env`**:
   - Set `PLUGIN_VERSION=<version>` in `.env`.
   
2. **Execute Unified Build**:
   - Run `python build.py` from the project root.
   - This automatically:
     - Updates `plugins/version_config.h`
     - Updates `package.ini`
     - Updates `version.json` (version, changelog, and `BlackSpeak.ts3_addon` download link)
     - Compiles `plugins/blackspeak.cpp` with MSVC x64 to `plugins/blackspeak_win64.dll`
     - Packages `BlackSpeak.ts3_addon`

3. **Stage & Commit Git Changes**:
   - Stage all modified and new files: `git add -A`
   - Commit with standard English release message: `git commit -m "Release v<version>: Automated build and release update"`
   - Create or update tag: `git tag -f v<version> -m "Release v<version>"`

4. **Push to Remote**:
   - Push commit and tags: `git push origin main --tags --force`

5. **Verify Completion**:
   - Confirm successful build and report release status to the user in English.