#!/bin/bash
# Installe des binaires Mac déjà compilés (ex. téléchargés depuis GitHub Actions).
# Place ce script à côté de AristoteThinker.vst3 et AristoteThinker.component, puis :
#   bash install_mac_prebuilt.sh
set -euo pipefail
cd "$(dirname "$0")"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

for item in AristoteThinker.vst3:"$VST3_DIR" AristoteThinker.component:"$AU_DIR"; do
    name="${item%%:*}"; dest="${item#*:}"
    if [ -e "$name" ]; then
        rm -rf "$dest/$name"
        cp -R "$name" "$dest/"
        # Plugin non notarisé : on retire la quarantaine et on signe localement
        xattr -dr com.apple.quarantine "$dest/$name" 2>/dev/null || true
        codesign --force --deep -s - "$dest/$name" 2>/dev/null || true
        echo "Installé : $dest/$name"
    fi
done

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Relance ton DAW et fais un nouveau scan des plugins."
