#Implementacion de la DFT y de la FFT radix-2 

import numpy as np


def es_potencia_de_dos(N):
    return N > 0 and (N & (N - 1)) == 0


def siguiente_potencia_de_dos(N):
    #Menor potencia de dos mayor o igual que N
    potencia = 1
    while potencia < N:
        potencia <<= 1
    return potencia


def rellenar_con_ceros(x):
    #Extiende x con ceros hasta la potencia de dos siguiente
    x = np.asarray(x, dtype=complex)
    N = x.size
    if es_potencia_de_dos(N):
        return x
    # El relleno interpola el espectro, no mejora la resolucion frecuencial.
    return np.concatenate([x, np.zeros(siguiente_potencia_de_dos(N) - N,
                                       dtype=complex)])


def matriz_dft(N):
    #Matriz de Fourier
    n = np.arange(N)
    k = n.reshape((N, 1))
    return np.exp(-2j * np.pi * k * n / N)


def dft(x):
    #DFT por definicion, con los dos lazos explicitos
    x = np.asarray(x, dtype=complex)
    N = x.size
    X = np.zeros(N, dtype=complex)
    for k in range(N):
        acumulador = 0.0 + 0.0j
        for n in range(N):
            acumulador += x[n] * np.exp(-2j * np.pi * k * n / N)
        X[k] = acumulador
    return X


def dft_matricial(x):
    #Misma DFT O(N^2) como producto matriz-vector X = W x
    x = np.asarray(x, dtype=complex)
    return matriz_dft(x.size) @ x


def idft(X):
    #DFT inversa por definicion.
    X = np.asarray(X, dtype=complex)
    N = X.size
    return (np.conj(matriz_dft(N)) @ X) / N


def fft_recursiva(x):
    #FFT radix-2 recursiva
    x = np.asarray(x, dtype=complex)
    N = x.size
    if not es_potencia_de_dos(N):
        raise ValueError("fft_recursiva requiere que N sea potencia de dos")
    if N == 1:
        return x.copy()

    pares = fft_recursiva(x[0::2])
    impares = fft_recursiva(x[1::2])
    factores_de_giro = np.exp(-2j * np.pi * np.arange(N // 2) / N)
    termino = factores_de_giro * impares
    return np.concatenate([pares + termino, pares - termino])


def _permutacion_bit_reverso(N):
    #Indices en bit-reverso
    bits = N.bit_length() - 1
    indices = np.arange(N)
    reverso = np.zeros(N, dtype=int)
    for posicion in range(bits):
        reverso |= ((indices >> posicion) & 1) << (bits - 1 - posicion)
    return reverso


def fft(x):
    #FFT radix-2 iterativa
    X = rellenar_con_ceros(x)
    N = X.size
    X = X[_permutacion_bit_reverso(N)].astype(complex)

    tamano_bloque = 2
    while tamano_bloque <= N:
        mitad = tamano_bloque // 2
        factores_de_giro = np.exp(-2j * np.pi * np.arange(mitad) / tamano_bloque)

        for inicio in range(0, N, tamano_bloque):
            superior = X[inicio:inicio + mitad].copy()
            inferior = X[inicio + mitad:inicio + tamano_bloque] * factores_de_giro
            X[inicio:inicio + mitad] = superior + inferior
            X[inicio + mitad:inicio + tamano_bloque] = superior - inferior

        tamano_bloque <<= 1

    return X


def ifft(X):
    #FFT inversa reutilizando la directa.
    X = np.asarray(X, dtype=complex)
    return np.conj(fft(np.conj(X))) / X.size
