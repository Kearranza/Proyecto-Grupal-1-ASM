"""

Diseño, implementación por ecuación de diferencias y análisis de filtros IIR (sección 2.3). 

Ejemplo (rechazo de banda alrededor de f0 = 1000 Hz, fs = 8000 Hz):
    sos, b, a = disenar_iir(fs=8000, wp=[900, 1100], ws=[970, 1030],
                            gpass=1.0, gstop=40.0, ftype='ellip')
Para rechazo de banda: wp = [borde_inf_paso, borde_sup_paso] EXTERIORES y
ws = banda de rechazo INTERIOR (es decir, wp abarca más que ws).

"""
import numpy as np
from scipy import signal


# ------------------------------------------------------------------ diseño
def disenar_iir(fs, wp, ws, gpass, gstop, ftype="ellip"):
    """Diseña con iirdesign. Devuelve sos, b, a y el orden total."""
    sos = signal.iirdesign(wp, ws, gpass, gstop, ftype=ftype,
                           output="sos", fs=fs)
    b, a = signal.sos2tf(sos)
    return sos, b, a


def zpk(sos):
    z, p, k = signal.sos2zpk(sos)
    return z, p, k


def es_estable(sos, margen=1e-9):
    """Causal: ROC = |z| > max|p|. Estable si max|p| < 1 (ROC contiene |z|=1)."""
    _, p, _ = zpk(sos)
    r = np.max(np.abs(p))
    return r < 1 - margen, r


# ------------------------------------------------- ecuación de diferencias
def filtrar_ecuacion_diferencias(b, a, x):
    """y[n] = (1/a0)(sum b_k x[n-k] - sum_{k>=1} a_k y[n-k]). Forma directa I.
    Úsela con b, a de orden bajo; para orden alto prefiera la versión SOS."""
    b = np.asarray(b, float) / a[0]
    a = np.asarray(a, float) / a[0]
    x = np.asarray(x, float)
    y = np.zeros_like(x)
    for n in range(len(x)):
        acc = 0.0
        for k in range(len(b)):
            if n - k >= 0:
                acc += b[k] * x[n - k]
        for k in range(1, len(a)):
            if n - k >= 0:
                acc -= a[k] * y[n - k]
        y[n] = acc
    return y


def filtrar_sos_manual(sos, x):
    """Cascada de biquads en forma directa II transpuesta (la que se llevaría
    al microcontrolador). Cada sección: b0 b1 b2 a0 a1 a2."""
    y = np.asarray(x, float).copy()
    for b0, b1, b2, a0, a1, a2 in sos:
        b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
        z1 = z2 = 0.0
        out = np.empty_like(y)
        for n, xn in enumerate(y):
            yn = b0 * xn + z1
            z1 = b1 * xn - a1 * yn + z2
            z2 = b2 * xn - a2 * yn
            out[n] = yn
        y = out
    return y


def verificar_equivalencia(sos, b, a, x, tol=1e-9):
    """Compara implementación propia vs scipy. Devuelve errores máx. y si pasa."""
    ref_sos = signal.sosfilt(sos, x)
    ref_ba = signal.lfilter(b, a, x)
    e_sos = np.max(np.abs(filtrar_sos_manual(sos, x) - ref_sos))
    e_ba = np.max(np.abs(filtrar_ecuacion_diferencias(b, a, x) - ref_ba))
    return {"err_max_sos": e_sos, "err_max_ba": e_ba,
            "tolerancia": tol, "ok": bool(max(e_sos, e_ba) < tol)}


# ---------------------------------------------------------------- respuestas
def respuestas(sos, fs, n=4096):
    """Magnitud (dB), fase (rad, desenrollada) y retardo de grupo (muestras y s)."""
    w, h = signal.sosfreqz(sos, worN=n, fs=fs)
    b, a = signal.sos2tf(sos)
    wg, gd = signal.group_delay((b, a), w=n)  # w en rad/muestra, gd en muestras
    f_gd = wg * fs / (2 * np.pi)
    return {"f": w, "mag_db": 20 * np.log10(np.maximum(np.abs(h), 1e-12)),
            "fase": np.unwrap(np.angle(h)),
            "f_gd": f_gd, "gd_muestras": gd, "gd_s": gd / fs}


# -------------------------------------------------------------- cuantización
def cuantizar_sos(sos, frac_bits=14):
    """Redondea coeficientes a punto fijo con frac_bits bits fraccionarios.
    Con Q2.14 se representan coeficientes en [-2, 2) (a1 de biquads suele
    estar en (-2, 2))."""
    q = 2.0 ** frac_bits
    s = sos.copy()
    s[:, [0, 1, 2, 4, 5]] = np.round(s[:, [0, 1, 2, 4, 5]] * q) / q
    return s


def efecto_cuantizacion(sos, fs, bits_lista=(8, 12, 14, 16)):
    """Para cada precisión: radio polar máximo, estabilidad y desviación de |H|."""
    ref = respuestas(sos, fs)["mag_db"]
    filas = []
    for fb in bits_lista:
        sq = cuantizar_sos(sos, fb)
        est, r = es_estable(sq)
        mag = respuestas(sq, fs)["mag_db"]
        filas.append({"frac_bits": fb, "estable": est, "radio_polar_max": r,
                      "desv_max_dB": float(np.max(np.abs(mag - ref)))})
    return filas


def saturacion_int16(y, escala):
    """Cuenta muestras que saturarían en int16 si y se escala por `escala`."""
    v = np.round(y * escala)
    return int(np.sum((v > 32767) | (v < -32768)))


def memoria_bytes(sos, bytes_coef=4, bytes_estado=4):
    """5 coeficientes + 2 estados por biquad."""
    s = len(sos)
    return {"secciones": s, "coef_bytes": 5 * s * bytes_coef,
            "estado_bytes": 2 * s * bytes_estado,
            "total_bytes": 5 * s * bytes_coef + 2 * s * bytes_estado,
            "MAC_por_muestra": 5 * s}


# ------------------------------------------------------------------ métricas
def snr_db(limpia, señal):
    r = np.asarray(señal) - np.asarray(limpia)
    return 10 * np.log10(np.sum(np.asarray(limpia) ** 2) / np.sum(r ** 2))