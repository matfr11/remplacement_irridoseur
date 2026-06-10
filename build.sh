#!/bin/bash
set -e

VERSION="${1:-}"
if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>  (ex: $0 1.1.10)"
    exit 1
fi

echo "Build firmware v${VERSION}..."
idf.py -DPROJECT_VER="${VERSION}" build

DEST="binaire/irrifrance-esp32-${VERSION}.bin"
cp build/irrifrance-esp32.bin "${DEST}"
echo "Binaire : ${DEST}"
