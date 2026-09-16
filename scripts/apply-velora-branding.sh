#!/usr/bin/env bash
# Apply Velora Studio Branding & Presets to an OBS Studio source tree

set -e

OBS_ROOT="${1:-../obs-studio}"

if [ ! -d "$OBS_ROOT" ]; then
    echo "Error: OBS source directory not found at $OBS_ROOT"
    exit 1
fi

echo "Applying Velora Studio branding to: $OBS_ROOT"

# Copy Theme
cp "$(dirname "$0")/../theme/VeloraDark.qss" "$OBS_ROOT/UI/data/themes/"

# Copy Services Config
cp "$(dirname "$0")/../config/services.json" "$OBS_ROOT/plugins/obs-outputs/data/"

echo "Velora Studio customization applied successfully!"
