#!/bin/bash
set -e

# Sourcer ESP-IDF si idf.py n'est pas dans le PATH
if ! command -v idf.py &>/dev/null; then
    source ~/esp/esp-idf/export.sh
fi

if grep -q "^CONFIG_IRRI_WOKWI_MODE=y" sdkconfig 2>/dev/null; then
    echo "ERREUR : CONFIG_IRRI_WOKWI_MODE=y dans sdkconfig — build bloqué."
    echo "Désactiver avec : idf.py menuconfig  (Irrifrance → Wokwi simulation mode)"
    exit 1
fi

VERSION=$(grep 'define IRRI_VERSION ' main/version.h | grep -oP '"[^"]+"' | tr -d '"')

if grep -q "^CONFIG_IRRI_PROD=y" sdkconfig 2>/dev/null; then
    echo "Build PROD v${VERSION} (deux passes)..."
    idf.py build
    idf.py reconfigure
    idf.py build
    echo "Binaire : $(ls build/irrifrance_*.bin | grep -v dev)"
else
    echo "Build DEV v${VERSION}..."
    idf.py build
    echo "Binaire : build/irrifrance_dev.bin"
fi
