#!/usr/bin/env bash
# ==============================================================================
# Script: rebrand-prism-to-velora.sh
# Purpose: Non-destructive theme and brand customization for PRISM Live Studio.
#          Preserves internal C++ macros/symbols while updating user-facing
#          branding, themes, services, and color schemes.
# ==============================================================================

set -euo pipefail

TARGET_DIR="${1:-.}"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Target directory $TARGET_DIR does not exist."
    exit 1
fi

echo "======================================================================"
echo " Applying Safe Velora Studio Customization to: $TARGET_DIR"
echo "======================================================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# 1. Inject Velora Services (WHIP + RTMP)
echo "[1/4] Injecting Velora Service Definitions..."
find "$TARGET_DIR" -type d -path "*/plugins/obs-outputs/data" | while read -r dest; do
    echo "Copying services.json to $dest"
    cp "$ROOT_DIR/config/services.json" "$dest/"
done

# 2. Inject Velora Theme & Styles
echo "[2/4] Injecting Velora Dark & Gold Themes..."
find "$TARGET_DIR" -type d -path "*/UI/data/themes" | while read -r dest; do
    echo "Copying themes to $dest"
    cp "$ROOT_DIR/theme/VeloraDark.qss" "$dest/"
    cp "$ROOT_DIR/theme/VeloraDark.ovt" "$dest/"
done

# 3. Update Colors in Existing QSS/CSS Themes (Yellow -> Velora Gold, Dark -> Obsidian)
echo "[3/4] Overriding UI Theme Palettes..."
find "$TARGET_DIR" -type f \( -name "*.qss" -o -name "*.css" \) ! -path "*/.git/*" | while read -r file; do
    # Replace default PRISM neon yellow (#FEE500, #F3E000) with Velora Gold (#D4AF37)
    sed -i 's/#FEE500/#D4AF37/gi' "$file" 2>/dev/null || true
    sed -i 's/#F3E000/#D4AF37/gi' "$file" 2>/dev/null || true
    sed -i 's/#FFDD00/#F3D062/gi' "$file" 2>/dev/null || true
    
    # Replace base dark gray backgrounds with Velora Cosmic Dark
    sed -i 's/#1E1E1E/#040817/gi' "$file" 2>/dev/null || true
    sed -i 's/#252525/#0B1026/gi' "$file" 2>/dev/null || true
    sed -i 's/#2B2B2B/#131B3A/gi' "$file" 2>/dev/null || true
done

# 4. Update User-Facing Locale Strings (Translations & UI Display Strings Only)
echo "[4/4] Updating User-Facing Display Strings..."
find "$TARGET_DIR" -type f \( -name "*.ini" -o -name "*.ts" \) -path "*/locale/*" | while read -r file; do
    sed -i 's/PRISM Live Studio/Velora Studio/g' "$file" 2>/dev/null || true
    sed -i 's/PRISM Live/Velora Studio/g' "$file" 2>/dev/null || true
    sed -i 's/PRISMLiveStudio/VeloraStudio/g' "$file" 2>/dev/null || true
done

echo "======================================================================"
echo " Velora Customization Complete (Safe & Build-Ready)!"
echo "======================================================================"
