"""Ejecuta la suite y pruebas con sanitizadores y guarda evidencia reproducible."""
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
from datetime import datetime, timezone

BASE = Path(__file__).resolve().parent
PROYECTO = BASE.parent
SALIDA = PROYECTO / "resultados" / "validacion_nodo3"
SRC = PROYECTO / "firmware" / "libraries" / "asm_comun" / "src"


def main():
    SALIDA.mkdir(parents=True, exist_ok=True)
    binarios = BASE / "bin" / "sanitizadores"
    binarios.mkdir(parents=True, exist_ok=True)
    ejecuciones = []

    def ejecutar(nombre, comando, entorno=None):
        resultado = subprocess.run(comando, cwd=BASE, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, check=False, env=entorno)
        (SALIDA / (nombre + ".log")).write_text(resultado.stdout, encoding="utf-8")
        ejecuciones.append({"nombre": nombre, "comando": comando, "retorno": resultado.returncode})
        print(f"{nombre}: retorno {resultado.returncode}")
        return resultado.returncode == 0

    bien = ejecutar("suite", ["make", "test"])
    compilador = shlex.split(os.environ.get("CC", "cc"))
    ejecutar("compilador", compilador + ["--version"])
    for prueba in ("test_fft", "test_espectral", "test_metricas", "test_nodo3", "test_regresion_nodo3"):
        destino = binarios / prueba
        comando = compilador + ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                   "-D_DEFAULT_SOURCE", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                   "-fno-omit-frame-pointer", "-I" + str(SRC), str(BASE / (prueba + ".c"))]
        comando += [str(p) for p in sorted(SRC.glob("*.c"))] + ["-lm", "-o", str(destino)]
        compilado = ejecutar(prueba + "_compilacion", comando)
        bien &= compilado
        if compilado:
            # LeakSanitizer no funciona bajo ptrace. ASan y UBSan siguen activos.
            entorno = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
            bien &= ejecutar(prueba + "_sanitizadores", [str(destino)], entorno)
    archivos = list(SRC.glob("*.[ch]")) + list(BASE.glob("test_*.*"))
    archivos += [BASE / "Makefile", Path(__file__).resolve(), BASE / "soporte_arduino" / "Arduino.h",
                 PROYECTO / "firmware" / "nodos23_esp32" / "nodos23_esp32.ino"]
    reporte = {"fecha_utc": datetime.now(timezone.utc).isoformat(), "plataforma": platform.platform(),
               "correcto": bool(bien), "ASAN_OPTIONS": "detect_leaks=0", "ejecuciones": ejecuciones,
               "sha256": {str(p.relative_to(PROYECTO)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in sorted(archivos)},
               "alcance": "Escritorio; no valida hardware. La suite puede informar modulos ajenos pendientes."}
    (SALIDA / "reporte.json").write_text(json.dumps(reporte, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0 if bien else 1


if __name__ == "__main__":
    raise SystemExit(main())
