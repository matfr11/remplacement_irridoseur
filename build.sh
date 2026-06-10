#!/bin/bash
set -e

VERSION="${1:-}"
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>  (ex: $0 1.1.10)"
    exit 1
fi

if grep -q "^CONFIG_IRRI_WOKWI_MODE=y" sdkconfig 2>/dev/null; then
    echo "ERREUR : CONFIG_IRRI_WOKWI_MODE=y dans sdkconfig — build production bloqué."
    echo "Désactiver avec : idf.py menuconfig  (Irrifrance → Wokwi simulation mode)"
    exit 1
fi

echo "Build firmware v${VERSION}..."
idf.py -DPROJECT_VER="${VERSION}" build

DEST="binaire/irrifrance-esp32-${VERSION}.bin"
cp build/irrifrance-esp32.bin "${DEST}"
echo "Binaire : ${DEST}"
