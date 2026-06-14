#!/bin/bash
set -e

if grep -q "^CONFIG_IRRI_WOKWI_MODE=y" sdkconfig 2>/dev/null; then
    echo "ERREUR : CONFIG_IRRI_WOKWI_MODE=y dans sdkconfig — build production bloqué."
    echo "Désactiver avec : idf.py menuconfig  (Irrifrance → Wokwi simulation mode)"
    exit 1
fi

VERSION=$(grep 'define IRRI_VERSION ' main/version.h | grep -oP '"[^"]+"' | tr -d '"')
echo "Build firmware v${VERSION}..."
idf.py build

DEST="binaire/irrifrance-esp32-${VERSION}.bin"
cp build/irrifrance-esp32.bin "${DEST}" 2>/dev/null || \
    cp build/irrifrance_dev.bin "${DEST}" 2>/dev/null || true
echo "Binaire : ${DEST}"
