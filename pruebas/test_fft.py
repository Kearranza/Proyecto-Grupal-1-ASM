from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

FS = 8192  # Hz
N = 1024
K = 64
F_BIN = K * FS / N
F_INTER = (K + 0.5) * FS / N
BASE = Path(__file__).resolve().parents[1]
FIG = BASE / 'resultados' / 'figuras'
FIG.mkdir(parents=True, exist_ok=True)


def espectro(x, ventana):
    w = np.ones(N) if ventana == 'rectangular' else np.hanning(N + 1)[:-1]
    X = np.fft.fft(x * w)
    magnitud = 2 * np.abs(X[:N // 2 + 1]) / np.sum(w)
    magnitud[0] /= 2
    magnitud[-1] /= 2
    fase = np.angle(X[:N // 2 + 1])
    freqs = np.fft.rfftfreq(N, 1 / FS)
    return freqs, magnitud, fase, X


def graficar(x, etiqueta, archivo):
    fig, ejes = plt.subplots(2, 2, figsize=(11, 6), sharex='col', layout='constrained')
    for col, ventana in enumerate(('rectangular', 'hann')):
        f, mag, fase, _ = espectro(x, ventana)
        db = 20 * np.log10(np.maximum(mag, 1e-12))
        ejes[0, col].plot(f, db, lw=1.1)
        ejes[0, col].set_xlim(400, 640)
        ejes[0, col].set_ylim(-105, 5)
        ejes[0, col].set_ylabel('Magnitud (dB re 1)')
        ejes[0, col].set_title(f'{etiqueta} — {ventana}')
        ejes[0, col].grid(True, alpha=.25)
        # La fase solo tiene sentido donde la componente es apreciable
        visible = mag > np.max(mag) * 0.01
        ejes[1, col].scatter(f[visible], fase[visible], s=14)
        ejes[1, col].set_xlim(400, 640)
        ejes[1, col].set_ylim(-3.5, 3.5)
        ejes[1, col].set_yticks([-np.pi, 0, np.pi], ['-π', '0', 'π'])
        ejes[1, col].set_xlabel('Frecuencia (Hz)')
        ejes[1, col].set_ylabel('Fase (rad)')
        ejes[1, col].grid(True, alpha=.25)
    fig.savefig(FIG / archivo, dpi=170)
    plt.close(fig)


def main():
    n = np.arange(N)
    x_bin = np.cos(2 * np.pi * F_BIN * n / FS)
    x_inter = np.cos(2 * np.pi * F_INTER * n / FS)
    assert N / FS == 0.125
    assert FS / N == 8.0
    for x in (x_bin, x_inter):
        assert np.allclose(np.fft.ifft(np.fft.fft(x)).real, x, atol=1e-12)
    graficar(x_bin, 'Tono alineado: 512 Hz', 'fig_21_bin.png')
    graficar(x_inter, 'Tono intermedio: 516 Hz', 'fig_21_entre_bins.png')

    X = np.fft.fft(x_inter)
    indices = np.arange(1, N // 2)
    error = np.max(np.abs(X[indices] - np.conj(X[-indices])))
    assert error < 1e-9, f'Simetria conjugada fallida: {error}'

    # Rechazo simetrico de los bins 63..65 y sus correspondientes negativos
    Y = X.copy()
    bins_rechazados = np.array([63, 64, 65])
    Y[bins_rechazados] = 0
    Y[-bins_rechazados] = 0
    y = np.fft.ifft(Y)
    assert np.max(np.abs(y.imag)) < 1e-12
    assert np.max(np.abs(Y[indices] - np.conj(Y[-indices]))) < 1e-9

    fig, ejes = plt.subplots(2, 1, figsize=(10, 6), layout='constrained')
    f = np.fft.fftfreq(N, 1 / FS)
    orden = np.argsort(f)
    ejes[0].plot(f[orden], np.abs(X[orden]), label='FFT original')
    ejes[0].plot(f[orden], np.abs(Y[orden]), '--', label='FFT modificada')
    ejes[0].set_xlim(-650, 650)
    ejes[0].set_ylabel('|X[k]|')
    ejes[0].legend()
    ejes[0].grid(True, alpha=.3)
    ejes[1].plot(f[orden], np.angle(X[orden]), '.', ms=2, label='Original')
    sel = np.abs(Y[orden]) > 1e-8
    ejes[1].plot(f[orden][sel], np.angle(Y[orden][sel]), '.', ms=2, label='Modificada')
    ejes[1].set_xlim(-650, 650)
    ejes[1].set_xlabel('Frecuencia (Hz)')
    ejes[1].set_ylabel('Fase (rad)')
    ejes[1].grid(True, alpha=.3)
    fig.savefig(FIG / 'fig_21_simetria.png', dpi=170)
    plt.close(fig)

    print(f'fs={FS} Hz, N={N}, T={N/FS:.3f} s, delta_f={FS/N:.1f} Hz')
    print(f'Tono alineado={F_BIN:.1f} Hz; tono entre bins={F_INTER:.1f} Hz')
    print(f'Error maximo de simetria conjugada={error:.3e}')
    print(f'Parte imaginaria maxima tras IFFT simetrica={np.max(np.abs(y.imag)):.3e}')
    print('Se generaron 3 figuras en resultados/figuras/')


if __name__ == '__main__':
    main()
