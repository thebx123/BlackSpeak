# Modern Black TeamSpeak 3 Suite
Designed and developed by **Coretify Studio**

---

## 📦 All-In-One Distribution Package

All theme elements, Windows 11 dark titlebar support, live accent customizer, and background auto-update subsystem are unified into a single distribution package:

### 📥 **[`ModernBlack.ts3_addon`](file:///g:/Projects/TS3-ModernBlack/ModernBlack.ts3_addon)**
Double-clicking this package automatically installs and activates the complete suite in TeamSpeak 3:
1. **Obsidian Dark Theme:** Base background color `#060709`, glowing gradient accent line below the menu bar, slim rounded pill scrollbars, and customized chat & channel panels.
2. **Windows 11 Native Dark TitleBar:** Hooks into Windows 11 DWM to render native dark titlebars and sleek dark frames across all dialogs and popup windows.
3. **Live Accent & Theme Customizer:** Instant color palette switching and real-time stylesheet updates without requiring a client restart.
4. **Auto-Updater Engine:** Non-blocking background update checks against GitHub releases with one-click direct update installation.

---

## ⚙️ Configuration & Auto-Update (`.env`)

Version number and update server endpoint are easily managed in the root [`.env`](file:///g:/Projects/TS3-ModernBlack/.env) file:

```env
# Suite & Plugin Version (SemVer format)
PLUGIN_VERSION=1.0.0

# Update Metadata JSON URL (GitHub Raw URL or dedicated server)
UPDATE_URL=https://raw.githubusercontent.com/thebx123/BlackSpeak/main/version.json
```

### Server `version.json` Metadata Schema:
```json
{
  "version": "1.0.0",
  "download_url": "https://github.com/thebx123/BlackSpeak/releases/download/v1.0.0/ModernBlack.ts3_addon",
  "changelog": "Initial release of BlackSpeak: Sleek Obsidian Dark Theme, Windows 11 Dark Titlebars, Live Accent Customizer, and Auto-Update support in 1 package."
}
```

---

## 🛠️ Automated Build & Packaging System

To compile the C++ plugin and generate the unified `ModernBlack.ts3_addon` package:

1. **Run Python build script:**
   ```bash
   python build.py
   ```
2. **Or double-click the Windows shortcut [`build.bat`](file:///g:/Projects/TS3-ModernBlack/build.bat)**.

---

## 📁 Repository Directory Structure:
```
├── .agents/                            # Coding standards, rules, and customization configs
│   ├── AGENTS.md                       # Core repository rules (English-only standard)
│   └── rules/language.md               # Language and localization constraints
├── .env                                # Local environment config (Version & Update URL)
├── .env.example                        # Template environment config
├── version.json                        # Server update metadata template
├── package.ini                         # TeamSpeak 3 Addon manifest
├── build.py                            # Unified compiler & packaging pipeline
├── build.bat                           # 1-Click build launcher for Windows
├── ModernBlack.ts3_addon               # Distribution package for TeamSpeak 3
├── styles/                             # QSS stylesheets and TPL templates
└── plugins/                            # Unified C++ plugin source and binary
    ├── modern_black.cpp                # Combined C++ source (TitleBar + Customizer + Updater)
    ├── modern_black_win64.dll          # 64-bit compiled DLL
    ├── qss_template.h                  # Embedded QSS stylesheet template
    ├── version_config.h                # Header generated from .env
    └── build_plugin.bat                # Standalone plugin build script
```
