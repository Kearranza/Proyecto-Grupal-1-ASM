#!/usr/bin/env bash
# Convierte cada diagramas/*.mmd en ../resultados/figuras/d_<nombre>.png
# Requiere Mermaid CLI:  npm install -g @mermaid-js/mermaid-cli
# Alternativa sin instalar nada: pegar el .mmd en https://mermaid.live y exportar PNG.
set -e
cd "$(dirname "$0")"
mkdir -p ../resultados/figuras
for f in diagramas/*.mmd; do
  nombre=$(basename "$f" .mmd)
  mmdc -i "$f" -o "../resultados/figuras/d_${nombre}.png" -b white -s 2
  echo "ok  $f"
done
