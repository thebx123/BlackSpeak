---
name: publish
description: Automated publishing tool for BlackSpeak suite that compiles C++ plugin, synchronizes versions, packages .ts3_addon, and creates GitHub releases when triggered with publish:<version>.
---

# BlackSpeak Publisher Skill

This skill handles automated compilation, version synchronization, packaging, and Git/GitHub release publishing for BlackSpeak.

## Workflow

Whenever the user specifies `publish:<version>` (e.g., `publish:1.1.0`):

1. **Modify `.env`**:
   Update `PLUGIN_VERSION=<version>` in `.env`.

2. **Run Build Script**:
   Execute `python build.py` to compile `plugins/blackspeak.cpp` and package `BlackSpeak.ts3_addon`.

3. **Git Release & Tag**:
   ```bash
   git add -A
   git commit -m "Release v<version>: Automated build and release update"
   git tag -f v<version> -m "Release v<version>"
   git push origin main --tags --force
   ```
