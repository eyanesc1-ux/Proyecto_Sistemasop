#!/bin/sh
# ejecutar.sh - Ejecuta el simulador en Linux (para CLOUD-LIN02, APP-LIN01)
cd "$(dirname "$0")"
if [ -x ./simplanif ]; then BIN=./simplanif; else BIN=./bin/linux/simplanif; fi
"$BIN" "$@"
echo
echo "Resultados en: $(pwd)/salida"
