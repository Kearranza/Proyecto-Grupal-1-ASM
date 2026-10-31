"""verificar_emisor.py -- Taller semana 12, bloque C (emisor en la Pico).

Comprueba con la FFT que lo que sale de la Pico (PWM + RC) contiene las
frecuencias y amplitudes de cada modo. La Pico lee su propia salida con el ADC
(GPIO26) y manda 1024 muestras por serial; aqui se calcula el espectro.

Uso (desde la raiz del repo):
    python taller_semana12/c_emisor/verificar_emisor.py --puerto /dev/ttyACM0
    python taller_semana12/c_emisor/verificar_emisor.py --simular     # sin placa

--simular reproduce en Python el mismo algoritmo entero del firmware (DDS Q15,
PWM de 8 bits), le aplica el filtro RC y cuantiza a 12 bits. Sirve para probar
este script antes de tener la placa y como referencia de lo que se espera medir.

Lee parametros de taller_semana12/parametros.json y metadatos/configuraciones.json.
No escribe numeros de modos a mano: si el JSON cambia, esto cambia con el.
"""
import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

AQUI = Path(__file__).resolve().parent
RAIZ = AQUI.parents[1]

# ---- hardware del montaje (editar si se cambian los componentes) ----------
R_OHM, C_F = 470.0, 10e-9        # cada etapa del RC; dos etapas iguales en cascada
VREF_V, BITS_ADC, BITS_PWM = 3.3, 12, 8
# ---- criterios de aceptacion locales (el emisor; los del receptor los fija D)
MARGEN_BINS_POR_DEFECTO = 1
MARGEN_DB_POR_DEFECTO = 1.5


# ------------------------------------------------------------- parametros
def _buscar(d, clave):
    """Busca una clave (escalar no nulo) en un JSON anidado."""
    if isinstance(d, dict):
        v = d.get(clave)
        if v is not None and not isinstance(v, (dict, list)):
            return v
        for x in d.values():
            r = _buscar(x, clave)
            if r is not None:
                return r
    elif isinstance(d, list):
        for x in d:
            r = _buscar(x, clave)
            if r is not None:
                return r
    return None


def cargar_parametros():
    ruta_p = RAIZ / "taller_semana12" / "parametros.json"
    ruta_c = RAIZ / "metadatos" / "configuraciones.json"
    if not ruta_p.exists() or not ruta_c.exists():
        sys.exit(f"No se encuentran {ruta_p} o {ruta_c}: ejecuta desde el repo.")
    p = json.loads(ruta_p.read_text(encoding="utf-8"))
    c = json.loads(ruta_c.read_text(encoding="utf-8"))
    em = p["emisor"]
    fs = float(em.get("fs_hz") or _buscar(c, "fs_hz"))
    n = int(_buscar(c, "n_bloque"))
    val = p.get("validacion", {})
    return {
        "fs": fs, "N": n,
        "nivel": float(em["nivel_pico"]),
        "baudios": int(em["baudios"]),
        "modos": {m["id"]: m for m in em["modos"]},
        "margen_bins": val.get("margen_frecuencia_bins") or MARGEN_BINS_POR_DEFECTO,
        "margen_db": val.get("margen_amplitud_db") or MARGEN_DB_POR_DEFECTO,
    }


# ------------------------------------------------------------- filtro RC
def h_rc(f):
    """Respuesta de dos RC iguales en cascada, con la carga de la segunda etapa:
    H = 1 / (1 - x^2 + 3jx),  x = 2*pi*f*R*C."""
    x = 2 * np.pi * np.asarray(f, float) * R_OHM * C_F
    return 1.0 / (1.0 - x ** 2 + 3j * x)


# -------------------------------------------------- replica del firmware
def duty_firmware(frecs, amps, n, fs, nivel):
    """Mismo algoritmo entero que on_muestra() en emisor_pico.ino."""
    seno = np.round(32767 * np.sin(2 * np.pi * np.arange(1024) / 1024)).astype(np.int64)
    suma = float(sum(amps))
    amp = [int(round(32767 * nivel * a / suma)) for a in amps]
    inc = [int(f * 2 ** 32 / fs + 0.5) for f in frecs]
    fase = [0] * len(frecs)
    duty = np.empty(n, dtype=np.int64)
    for k in range(n):
        acc = 0
        for i in range(len(frecs)):
            fase[i] = (fase[i] + inc[i]) & 0xFFFFFFFF
            acc += (amp[i] * int(seno[fase[i] >> 22])) >> 15
        duty[k] = min(255, max(0, 128 + (acc >> 8)))
    return duty


def simular_captura(modo, prm):
    n, fs = prm["N"], prm["fs"]
    L = 4 * n
    duty = duty_firmware(modo["frecuencias_hz"], modo["amplitudes"], L, fs, prm["nivel"])
    v = duty / 2 ** BITS_PWM * VREF_V                    # tension media por muestra
    V = np.fft.rfft(v) * h_rc(np.fft.rfftfreq(L, 1 / fs))
    v = np.fft.irfft(V, L)[n:2 * n]                      # tramo central: sin bordes
    return np.clip(np.round(v / VREF_V * 2 ** BITS_ADC), 0, 2 ** BITS_ADC - 1)


# --------------------------------------------------------------- captura
def capturar_serial(puerto, modo_id, prm):
    try:
        import serial
    except ImportError:
        sys.exit("Falta pyserial: pip install pyserial")
    with serial.Serial(puerto, prm["baudios"], timeout=3) as s:
        s.reset_input_buffer()
        s.write(f"MODO {modo_id}\n".encode())
        resp = s.readline().decode(errors="ignore").strip()
        if resp != f"OK {modo_id}":
            sys.exit(f"La Pico respondio '{resp}' al comando MODO {modo_id}")
        time.sleep(0.3)                                   # deja pasar el transitorio del RC
        s.reset_input_buffer()
        s.write(b"CAPTURA\n")
        t0 = time.time()
        while s.readline().decode(errors="ignore").strip() != "CAP_INICIO":
            if time.time() - t0 > 5:
                sys.exit("Sin CAP_INICIO: revisa el puerto y el firmware")
        datos = []
        while len(datos) < prm["N"]:
            linea = s.readline().decode(errors="ignore").strip()
            if linea == "CAP_FIN" or not linea:
                break
            datos.append(int(linea))
        if len(datos) != prm["N"]:
            sys.exit(f"Se esperaban {prm['N']} muestras y llegaron {len(datos)}")
        return np.array(datos, float)


# -------------------------------------------------------------- analisis
def analizar(x, modo, prm):
    fs, N = prm["fs"], len(x)
    x = x - x.mean()
    w = 0.5 - 0.5 * np.cos(2 * np.pi * np.arange(N) / N)          # Hann periodica
    X = np.fft.rfft(x * w)
    P = np.abs(X) ** 2
    sw2 = float(np.sum(w ** 2))
    df = fs / N

    def amplitud(k0, mitad=2):
        # amplitud de un tono (cuentas de ADC) sumando su lobulo principal:
        # no depende de que el tono caiga justo en un bin (perdida de escalopeo).
        S = P[max(k0 - mitad, 0):k0 + mitad + 1].sum()
        return 2 * np.sqrt(S / (N * sw2))

    frecs, amps = modo["frecuencias_hz"], modo["amplitudes"]
    suma = float(sum(amps))
    filas, enmascarar = [], np.zeros(len(P), bool)
    enmascarar[:7] = True
    for f, a in zip(frecs, amps):
        k_esp = int(round(f / df))
        lo, hi = max(k_esp - prm["margen_bins"], 1), k_esp + prm["margen_bins"] + 1
        k = lo + int(np.argmax(P[lo:hi]))
        # frecuencia medida: interpolacion parabolica sobre el log del espectro
        l0, l1, l2 = np.log(P[k - 1:k + 2] + 1e-30)
        delta = 0.5 * (l0 - l2) / (l0 - 2 * l1 + l2)
        f_med = (k + delta) * df
        a_esp = 2048 * prm["nivel"] * a / suma * abs(h_rc(f))     # cuentas de ADC
        a_med = amplitud(k)
        err_db = 20 * np.log10(a_med / a_esp)
        ok_f = abs(k - k_esp) <= prm["margen_bins"]
        ok_a = abs(err_db) <= prm["margen_db"]
        filas.append(dict(f_esp=f, f_med=f_med, bin_esp=k_esp, bin_med=k,
                          a_esp=a_esp, a_med=a_med, err_db=err_db, ok=ok_f and ok_a))
        enmascarar[max(k_esp - 6, 0):k_esp + 7] = True

    P_fuera = np.where(enmascarar, 0.0, P)
    k_s = int(np.argmax(P_fuera))
    a_esp_min = min(r["a_esp"] for r in filas)
    a_espurio = 2 * np.abs(X[k_s]) / np.sum(w)          # un solo bin: no incluye faldas de tonos
    sfdr = 20 * np.log10(a_esp_min / max(a_espurio, 1e-12))
    res = dict(filas=filas, espectro=X, w=w, espurio_hz=k_s * df, sfdr_db=sfdr)

    if len(frecs) == 4:                                            # modo con interferencia
        a_t = np.sqrt(sum(r["a_med"] ** 2 for r in filas[:3]))
        a_te = np.sqrt(sum(r["a_esp"] ** 2 for r in filas[:3]))
        res["snr_med"] = 20 * np.log10(a_t / filas[3]["a_med"])
        res["snr_esp"] = 20 * np.log10(a_te / filas[3]["a_esp"])
        res["snr_sin_rc"] = 20 * np.log10(
            np.sqrt(sum(a ** 2 for a in amps[:3])) / amps[3])
        res["ok_snr"] = abs(res["snr_med"] - res["snr_esp"]) <= prm["margen_db"]
    return res


def graficar(x, res, modo, prm, ruta, etiqueta):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fs, N = prm["fs"], len(x)
    fig, ax = plt.subplots(2, 1, figsize=(8, 6))
    n_ver = int(0.01 * fs)
    ax[0].plot(np.arange(n_ver) / fs * 1e3, x[:n_ver])
    ax[0].set(xlabel="t (ms)", ylabel="ADC (cuentas)",
              title=f"Modo {modo['id']}: {modo['nombre']} {etiqueta}")
    mag = 20 * np.log10(2 * np.abs(res["espectro"]) / np.sum(res["w"]) + 1e-12)
    f = np.fft.rfftfreq(N, 1 / fs)
    ax[1].plot(f, mag)
    for r in res["filas"]:
        ax[1].axvline(r["f_esp"], color="g", ls=":", lw=1)
    ax[1].set(xlabel="f (Hz)", ylabel="amplitud (dB re 1 cuenta)", xlim=(0, fs / 2))
    for a_ in ax:
        a_.grid(alpha=.3)
    fig.tight_layout()
    ruta.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(ruta, dpi=150)
    plt.close(fig)


# ------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--puerto", help="p. ej. /dev/ttyACM0 o COM5")
    ap.add_argument("--simular", action="store_true", help="sin placa")
    ap.add_argument("--modos", type=int, nargs="*", default=[1, 2, 3])
    args = ap.parse_args()
    if not args.simular and not args.puerto:
        ap.error("indica --puerto o usa --simular")

    prm = cargar_parametros()
    etiqueta = "[SIMULADO]" if args.simular else "[MEDIDO]"
    sufijo = "_sim" if args.simular else ""
    print(f"fs = {prm['fs']:.0f} Hz | N = {prm['N']} | resolucion = {prm['fs']/prm['N']:.3f} Hz | "
          f"margenes: {prm['margen_bins']} bin, {prm['margen_db']} dB")
    fc = 1 / (2 * np.pi * R_OHM * C_F)
    print(f"RC: R = {R_OHM:.0f} ohm, C = {C_F*1e9:.1f} nF por etapa (f0 = {fc/1e3:.1f} kHz)\n")

    todo_ok = True
    for mid in args.modos:
        modo = prm["modos"][mid]
        x = simular_captura(modo, prm) if args.simular else capturar_serial(args.puerto, mid, prm)
        (AQUI / "capturas").mkdir(exist_ok=True)
        np.savetxt(AQUI / "capturas" / f"modo_{mid}{sufijo}.csv", x, fmt="%d")
        res = analizar(x, modo, prm)
        graficar(x, res, modo, prm, AQUI / "figuras" / f"fig_c_modo_{mid}{sufijo}.png", etiqueta)

        print(f"Modo {mid}: {modo['nombre']} {etiqueta}")
        print(f"  {'f esp':>7s} {'f med':>9s} {'bin':>7s} {'A esp':>8s} {'A med':>8s} {'error':>9s}")
        for r in res["filas"]:
            print(f"  {r['f_esp']:7.0f} {r['f_med']:9.1f} {r['bin_med']:3d}/{r['bin_esp']:<3d} "
                  f"{r['a_esp']:8.1f} {r['a_med']:8.1f} {r['err_db']:+7.2f} dB  "
                  f"{'[ok]' if r['ok'] else '[FALLA]'}")
        print(f"  espurio mayor: {res['espurio_hz']:.0f} Hz | tono mas debil / espurio = {res['sfdr_db']:.1f} dB")
        if "snr_med" in res:
            print(f"  SNR tonos/interferencia: medida {res['snr_med']:.2f} dB | esperada con RC "
                  f"{res['snr_esp']:.2f} dB | sin RC {res['snr_sin_rc']:.2f} dB  "
                  f"{'[ok]' if res['ok_snr'] else '[FALLA]'}")
        print()
        todo_ok &= all(r["ok"] for r in res["filas"]) and res.get("ok_snr", True)

    print("[ok] los modos verificados cumplen" if todo_ok else "[FALLA] hay modos fuera de margen")
    return 0 if todo_ok else 1


if __name__ == "__main__":
    sys.exit(main())
