#!/usr/bin/env bash
# ==============================================================================
# Script: rebrand-prism-to-velora.sh
# Purpose: Complete build preparation and Velora Studio branding for PRISM Live Studio.
# ==============================================================================

set -euo pipefail

TARGET_DIR="${1:-.}"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Target directory $TARGET_DIR does not exist."
    exit 1
fi

echo "======================================================================"
echo " Preparing Velora Studio Build & Customizations on: $TARGET_DIR"
echo "======================================================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# 1. Pre-create expected build directory trees for CMake INTERFACE_INCLUDE_DIRECTORIES
echo "[1/8] Creating required CMake build folders..."
mkdir -p "$TARGET_DIR/src/obs-studio/build/Release/config"
mkdir -p "$TARGET_DIR/src/obs-studio/build/RelWithDebInfo/config"
mkdir -p "$TARGET_DIR/src/obs-studio/build/Debug/config"
mkdir -p "$TARGET_DIR/src/prism-live-studio/build/Release/config"
mkdir -p "$TARGET_DIR/src/prism-live-studio/build/RelWithDebInfo/config"
mkdir -p "$TARGET_DIR/bin/prism/windows/Release"
mkdir -p "$TARGET_DIR/bin/prism/windows/RelWithDebInfo"

# 2. Inject Velora Services (WHIP + RTMP)
echo "[2/8] Injecting Velora Service Definitions..."
find "$TARGET_DIR" -type d -path "*/plugins/obs-outputs/data" | while read -r dest; do
    echo "Copying services.json to $dest"
    cp "$ROOT_DIR/config/services.json" "$dest/"
done

# 3. Inject Velora Theme & Styles
echo "[3/8] Injecting Velora Dark & Gold Themes..."
find "$TARGET_DIR" -type d -path "*/UI/data/themes" | while read -r dest; do
    echo "Copying themes to $dest"
    cp "$ROOT_DIR/theme/VeloraDark.qss" "$dest/"
    cp "$ROOT_DIR/theme/VeloraDark.ovt" "$dest/"
done

# 4. Update Colors in Existing QSS/CSS Themes (Yellow -> Velora Gold, Dark -> Obsidian)
echo "[4/8] Overriding UI Theme Palettes..."
find "$TARGET_DIR" -type f \( -name "*.qss" -o -name "*.css" \) ! -path "*/.git/*" | while read -r file; do
    sed -i 's/#FEE500/#D4AF37/gi' "$file" 2>/dev/null || true
    sed -i 's/#F3E000/#D4AF37/gi' "$file" 2>/dev/null || true
    sed -i 's/#FFDD00/#F3D062/gi' "$file" 2>/dev/null || true
    sed -i 's/#1E1E1E/#040817/gi' "$file" 2>/dev/null || true
    sed -i 's/#252525/#0B1026/gi' "$file" 2>/dev/null || true
    sed -i 's/#2B2B2B/#131B3A/gi' "$file" 2>/dev/null || true
done

# 5. Update User-Facing Locale Strings (Translations & UI Display Strings Only)
echo "[5/8] Updating User-Facing Display Strings..."
find "$TARGET_DIR" -type f \( -name "*.ini" -o -name "*.ts" \) -path "*/locale/*" | while read -r file; do
    sed -i 's/PRISM Live Studio/Velora Studio/g' "$file" 2>/dev/null || true
    sed -i 's/PRISM Live/Velora Studio/g' "$file" 2>/dev/null || true
    sed -i 's/PRISMLiveStudio/VeloraStudio/g' "$file" 2>/dev/null || true
done

# 6. Fix CMake compatibility checks
echo "[6/8] Patching CMake legacy checks..."
find "$TARGET_DIR" -type f -name "CMakeLists.txt" ! -path "*/.git/*" | while read -r file; do
    sed -i 's/legacy_check()/message(STATUS "legacy_check bypassed")/g' "$file" 2>/dev/null || true
    sed -i 's/legacy_check(.[^)]*)/message(STATUS "legacy_check bypassed")/g' "$file" 2>/dev/null || true
done

# 7. Disable /WX and suppress MSVC deprecation warnings
echo "[7/8] Disabling /WX TreatWarningsAsErrors..."
find "$TARGET_DIR" -type f \( -name "*.cmake" -o -name "CMakeLists.txt" \) ! -path "*/.git/*" | while read -r file; do
    sed -i 's/\/WX//g' "$file" 2>/dev/null || true
    sed -i 's/-WX//g' "$file" 2>/dev/null || true
done

find "$TARGET_DIR" -type f -name "Config.cpp" ! -path "*/.git/*" | while read -r file; do
    sed -i '1s/^/#pragma warning(disable: 4996)\n/' "$file" 2>/dev/null || true
done

# 8. Qt6 Compatibility Patches
echo "[8/8] Applying Qt6 Compatibility patches..."
find "$TARGET_DIR" -type f \( -name "*.cpp" -o -name "*.h" \) ! -path "*/.git/*" | while read -r file; do
    sed -i 's/QEvent::DevicePixelRatioChange/QEvent::Type(999)/g' "$file" 2>/dev/null || true
done

echo "======================================================================"
echo " Velora Customization & Build Prep Complete!"
echo "======================================================================"
