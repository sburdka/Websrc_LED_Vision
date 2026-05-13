#!/bin/bash
# Copies the web project files into the Android app's assets/web/ directory.
# Run this from the repository root before building the APK:
#   cd /path/to/Websrc_LED_Vision
#   bash android_app/copy_web_assets.sh

set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSETS_DIR="$REPO_ROOT/android_app/app/src/main/assets/web"

echo "Copying web assets to $ASSETS_DIR ..."
mkdir -p "$ASSETS_DIR"

# Copy all HTML, JS, CSS, font and image files (exclude android_app/ and .git/)
rsync -av --exclude='android_app/' --exclude='.git/' \
      --include='*.html' \
      --include='*.js' \
      --include='*.css' \
      --include='*.ttf' \
      --include='*.jpg' \
      --include='*.png' \
      --include='*.gif' \
      --exclude='*' \
      "$REPO_ROOT/" "$ASSETS_DIR/"

echo "Done. Web assets are ready in $ASSETS_DIR"
