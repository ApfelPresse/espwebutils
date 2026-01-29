#!/bin/bash

# Release script for espwebutils
# Usage: ./release.sh [version]
# If no version is provided, the patch version is automatically incremented
# Example: ./release.sh 0.6.2
# Example: ./release.sh  (auto-increments from current version)

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# If no version provided, auto-increment patch version
if [ -z "$1" ]; then
  CURRENT_VERSION=$(grep '"version"' "$PROJECT_DIR/library.json" | head -1 | sed 's/.*"\([^"]*\)".*/\1/')
  
  # Extract major, minor, patch
  MAJOR=$(echo "$CURRENT_VERSION" | cut -d. -f1)
  MINOR=$(echo "$CURRENT_VERSION" | cut -d. -f2)
  PATCH=$(echo "$CURRENT_VERSION" | cut -d. -f3)
  
  # Increment patch version
  PATCH=$((PATCH + 1))
  VERSION="$MAJOR.$MINOR.$PATCH"
  
  echo "📌 No version specified. Auto-incrementing patch version."
  echo "   Current: $CURRENT_VERSION → New: $VERSION"
else
  VERSION="$1"
fi

echo "🚀 Starting release process for v$VERSION..."

# Check if version already exists as tag
if git rev-parse "v$VERSION" >/dev/null 2>&1; then
  echo "❌ Tag v$VERSION already exists!"
  exit 1
fi

# Update library.json version
echo "📝 Updating library.json version to $VERSION..."
sed -i.bak "s/\"version\": \"[^\"]*\"/\"version\": \"$VERSION\"/" "$PROJECT_DIR/library.json"
rm -f "$PROJECT_DIR/library.json.bak"

# Update build_info.h version
echo "📝 Updating build_info.h version to $VERSION..."
sed -i.bak "s/#define ESPWEBUTILS_LIBRARY_VERSION \"[^\"]*\"/#define ESPWEBUTILS_LIBRARY_VERSION \"$VERSION\"/" "$PROJECT_DIR/src/build_info.h"
rm -f "$PROJECT_DIR/src/build_info.h.bak"

# Build the project
echo "🔨 Building project..."
cd "$PROJECT_DIR"
if ! pio run -e esp32s3; then
  echo "❌ Build failed!"
  exit 1
fi

# Commit changes
echo "💾 Committing changes..."
git add -A
git commit -m "Release v$VERSION"

# Tag the release
echo "🏷️  Tagging release..."
git tag -a "v$VERSION" -m "Release v$VERSION"

# Push to remote
echo "📤 Pushing to remote..."
git push origin main
git push origin "v$VERSION"

echo "✅ Release v$VERSION completed successfully!"
echo "📦 Tag: v$VERSION"
echo "🔗 Push: origin main + v$VERSION"
