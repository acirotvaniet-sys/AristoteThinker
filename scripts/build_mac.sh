#!/bin/bash
# Compile et installe AristoteThinker sur macOS (VST3 + AU + Standalone).
# Binaire universel : Mac Apple Silicon (M1/M2/M3/M4) et Intel.
#
# Utilisation (dans le Terminal, depuis le dossier du projet) :
#   bash scripts/build_mac.sh
set -euo pipefail

cd "$(dirname "$0")/.."

echo "== AristoteThinker : compilation macOS =="

# 1. Outils nécessaires
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Les outils de développement Apple sont nécessaires. Une fenêtre d'installation va s'ouvrir."
    xcode-select --install || true
    echo "Relance ce script une fois l'installation terminée."
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    if command -v brew >/dev/null 2>&1; then
        echo "Installation de CMake via Homebrew..."
        brew install cmake
    else
        echo "CMake est introuvable. Installe Homebrew (https://brew.sh) puis : brew install cmake"
        echo "ou télécharge CMake sur https://cmake.org/download/"
        exit 1
    fi
fi

if ! command -v git >/dev/null 2>&1; then
    echo "Git est introuvable (il est fourni avec les outils Xcode)."
    exit 1
fi

# 2. Compilation (JUCE est téléchargé automatiquement au premier lancement)
cmake -B build-mac -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build-mac --config Release --target AristoteThinker_VST3 AristoteThinker_AU AristoteThinker_Standalone

ART="build-mac/AristoteThinker_artefacts/Release"

# 3. Installation dans les dossiers de plugins de l'utilisateur
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/AristoteThinker.vst3" "$AU_DIR/AristoteThinker.component"
cp -R "$ART/VST3/AristoteThinker.vst3" "$VST3_DIR/"
cp -R "$ART/AU/AristoteThinker.component" "$AU_DIR/"

# Signature locale « ad hoc » (suffisante pour un usage personnel)
codesign --force --deep -s - "$VST3_DIR/AristoteThinker.vst3" || true
codesign --force --deep -s - "$AU_DIR/AristoteThinker.component" || true
xattr -dr com.apple.quarantine "$VST3_DIR/AristoteThinker.vst3" "$AU_DIR/AristoteThinker.component" 2>/dev/null || true

# Force macOS à redécouvrir les Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Installé :"
echo "  VST3 -> $VST3_DIR/AristoteThinker.vst3"
echo "  AU   -> $AU_DIR/AristoteThinker.component"
echo "  App  -> $ART/Standalone/AristoteThinker.app"
echo ""
echo "Relance ton DAW et fais un nouveau scan des plugins."
