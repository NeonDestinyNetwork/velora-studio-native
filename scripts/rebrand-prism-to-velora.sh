#!/usr/bin/env bash
# ==============================================================================
# Script: rebrand-prism-to-velora.sh
# Purpose: Rebrands PRISM Live Studio to "Velora Studio" with custom Velora styling,
#          color palettes (Cosmic Dark & Velora Gold), pre-configured WHIP/RTMP
#          services, and 2.0s GOP keyframe defaults.
# ==============================================================================

set -euo pipefail

TARGET_DIR="${1:-.}"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Target directory $TARGET_DIR does not exist."
    exit 1
fi

echo "======================================================================"
echo " Starting Velora Studio Rebranding Transformation on: $TARGET_DIR"
echo "======================================================================"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# 1. Inject Velora Services (WHIP + RTMP)
echo "[1/4] Injecting Velora Service Definitions..."
if [ -d "$TARGET_DIR/src/prism-live-studio/plugins/obs-outputs/data" ]; then
    cp "$ROOT_DIR/config/services.json" "$TARGET_DIR/src/prism-live-studio/plugins/obs-outputs/data/"
fi
if [ -d "$TARGET_DIR/plugins/obs-outputs/data" ]; then
    cp "$ROOT_DIR/config/services.json" "$TARGET_DIR/plugins/obs-outputs/data/"
fi

# 2. Inject Velora Theme & Styles
echo "[2/4] Injecting Velora Dark & Gold Themes..."
THEME_DEST=""
if [ -d "$TARGET_DIR/src/prism-live-studio/UI/data/themes" ]; then
    THEME_DEST="$TARGET_DIR/src/prism-live-studio/UI/data/themes"
elif [ -d "$TARGET_DIR/UI/data/themes" ]; then
    THEME_DEST="$TARGET_DIR/UI/data/themes"
fi

if [ -n "$THEME_DEST" ]; then
    cp "$ROOT_DIR/theme/VeloraDark.qss" "$THEME_DEST/"
    cp "$ROOT_DIR/theme/VeloraDark.ovt" "$THEME_DEST/"
fi

# 3. String & Brand Replacements across UI / Resource files
echo "[3/4] Updating Brand Strings (PRISM Live Studio -> Velora Studio)..."
find "$TARGET_DIR" -type f \( -name "*.h" -o -name "*.cpp" -o -name "*.rc" -o -name "*.json" -o -name "*.qss" -o -name "*.ui" \) ! -path "*/.git/*" | while read -r file; do
    # Replace application display names
    sed -i 's/PRISM Live Studio/Velora Studio/g' "$file" 2>/dev/null || true
    sed -i 's/PRISMLiveStudio/VeloraStudio/g' "$file" 2>/dev/null || true
    sed -i 's/PRISM Live/Velora/g' "$file" 2>/dev/null || true
    sed -i 's/PRISM/Velora/g' "$file" 2>/dev/null || true
    
    # Replace default PRISM neon yellow (#F3E000, #FEE500, etc.) with Velora Gold #D4AF37 / #F3D062
    sed -i 's/#FEE500/#D4AF37/g' "$file" 2>/dev/null || true
    sed -i 's/#fee500/#d4af37/g' "$file" 2>/dev/null || true
    sed -i 's/#F3E000/#D4AF37/g' "$file" 2>/dev/null || true
    sed -i 's/#f3e000/#d4af37/g' "$file" 2>/dev/null || true
    sed -i 's/#FFDD00/#F3D062/g' "$file" 2>/dev/null || true
    sed -i 's/#ffdd00/#f3d062/g' "$file" 2>/dev/null || true
    
    # Replace PRISM backgrounds with Velora Cosmic Dark
    sed -i 's/#1E1E1E/#040817/g' "$file" 2>/dev/null || true
    sed -i 's/#252525/#0B1026/g' "$file" 2>/dev/null || true
    sed -i 's/#2B2B2B/#131B3A/g' "$file" 2>/dev/null || true
done

# 4. Summary Output
echo "[4/4] Velora Studio Rebranding Complete!"
echo "======================================================================"
