#!/usr/bin/env python3
"""
Modern Black Suite - Unified Build & Packaging System
Reads settings from .env, synchronizes version and update URLs,
compiles the unified C++ plugin with MSVC x64, and generates ModernBlack.ts3_addon.
"""

import os
import re
import sys
import zipfile
import subprocess

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))

def parse_env(env_path):
    config = {
        'PLUGIN_VERSION': '1.0.0',
        'UPDATE_URL': 'https://raw.githubusercontent.com/TheBx123/BlackSpeak/main/version.json'
    }
    target_env = env_path if os.path.exists(env_path) else os.path.join(PROJECT_ROOT, '.env.example')
    if os.path.exists(target_env):
        with open(target_env, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if '=' in line:
                    k, v = line.split('=', 1)
                    config[k.strip()] = v.strip().strip('"').strip("'")
    return config

def write_version_header(file_path, version, update_url):
    content = f"""#pragma once

// Auto-generated from .env by build.py - DO NOT EDIT DIRECTLY
#define PLUGIN_VERSION_STR "{version}"
#define UPDATE_CHECK_URL "{update_url}"
"""
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"[*] Updated header: {os.path.relpath(file_path, PROJECT_ROOT)}")

def update_ini_version(ini_path, version):
    if not os.path.exists(ini_path):
        return
    with open(ini_path, 'r', encoding='utf-8') as f:
        content = f.read()
    new_content = re.sub(r'Version\s*=\s*[^\r\n]+', f'Version = {version}', content)
    with open(ini_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(f"[*] Synchronized {os.path.relpath(ini_path, PROJECT_ROOT)} -> Version {version}")

def find_vcvars64():
    candidates = [
        r"D:\Visual Studio\Main\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None

def compile_plugin(vcvars_bat):
    print("\n--- Compiling Unified C++ Plugin (MSVC 64-bit) ---")
    plugins_dir = os.path.join(PROJECT_ROOT, 'plugins')
    src_file = os.path.join(plugins_dir, 'modern_black.cpp')
    out_dll = os.path.join(plugins_dir, 'modern_black_win64.dll')

    cmd = f'call "{vcvars_bat}" && cl.exe /O2 /MT /GS- /W3 /D_WIN32_WINNT=0x0601 /EHa /LD /Fe:"{out_dll}" "{src_file}" /link user32.lib gdi32.lib comctl32.lib shell32.lib dwmapi.lib wininet.lib /OPT:REF /OPT:ICF'
    res = subprocess.run(cmd, shell=True, cwd=plugins_dir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if res.returncode != 0:
        print("[!] Plugin compilation failed:")
        print(res.stdout)
        return False
    print("[+] Modern Black plugin compiled successfully.")

    # Clean intermediate build files
    for ext in ['.obj', '.exp', '.lib']:
        p = os.path.join(plugins_dir, 'modern_black' + ext)
        if os.path.exists(p): os.remove(p)
        p2 = os.path.join(plugins_dir, 'modern_black_win64' + ext)
        if os.path.exists(p2): os.remove(p2)

    return True

def package_addon(version):
    print("\n--- Packaging Unified ModernBlack.ts3_addon ---")
    addon_out = os.path.join(PROJECT_ROOT, 'ModernBlack.ts3_addon')
    if os.path.exists(addon_out):
        os.remove(addon_out)

    ini_path = os.path.join(PROJECT_ROOT, 'package.ini')
    dll_path = os.path.join(PROJECT_ROOT, 'plugins', 'modern_black_win64.dll')
    styles_dir = os.path.join(PROJECT_ROOT, 'styles')

    with zipfile.ZipFile(addon_out, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        # 1. Manifest
        zf.write(ini_path, arcname='package.ini')
        print("  Added: package.ini")

        # 2. Unified Plugin DLL
        if os.path.exists(dll_path):
            zf.write(dll_path, arcname='plugins/modern_black_win64.dll')
            print("  Added: plugins/modern_black_win64.dll")
        else:
            print("[!] Warning: Plugin DLL not found!")

        # 3. Styles & Templates
        for root, _, files in os.walk(styles_dir):
            for file in files:
                fp = os.path.join(root, file)
                rel = os.path.relpath(fp, PROJECT_ROOT).replace('\\', '/')
                zf.write(fp, arcname=rel)
                print(f"  Added: {rel}")

    print(f"\n[+] Successfully created Unified Addon: {addon_out} ({os.path.getsize(addon_out)} bytes)")

def main():
    print("===================================================")
    print("  Modern Black Suite - Unified Build System")
    print("===================================================")

    env_path = os.path.join(PROJECT_ROOT, '.env')
    config = parse_env(env_path)
    version = config['PLUGIN_VERSION']
    update_url = config['UPDATE_URL']

    print(f"[*] Target Version : {version}")
    print(f"[*] Update URL     : {update_url}")

    # 1. Update Version Config Header
    write_version_header(os.path.join(PROJECT_ROOT, 'plugins', 'version_config.h'), version, update_url)

    # 2. Synchronize INI manifest
    update_ini_version(os.path.join(PROJECT_ROOT, 'package.ini'), version)

    # 3. Compile C++ plugin
    vcvars = find_vcvars64()
    if not vcvars:
        print("[!] Error: Visual Studio vcvars64.bat not found!")
        sys.exit(1)

    if not compile_plugin(vcvars):
        print("[!] Build failed during compilation.")
        sys.exit(1)

    # 4. Package unified addon
    package_addon(version)

    print("\n===================================================")
    print("  Unified build completed successfully!")
    print("===================================================")

if __name__ == '__main__':
    main()
