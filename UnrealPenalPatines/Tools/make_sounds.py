"""Genera los efectos de sonido (WAV 44,1 kHz, mono) que usa el juego, sintetizados con numpy.
Hit, rebote, valla, palo, red, atajada, silbato, hinchada, queja y ambiente de estadio (loop).
Uso: python Tools/make_sounds.py"""
import numpy as np, wave, os

SR = 44100
OUT = os.path.join(os.path.dirname(__file__), "..", "SourceAssets", "Audio")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(3)


def t(d):
    return np.arange(int(SR * d)) / SR


def lowpass(x, f):
    a = np.exp(-2 * np.pi * f / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):          # filtro de un polo (suficiente para ruido)
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def bandpass(x, lo, hi):
    return lowpass(x, hi) - lowpass(x, lo)


def env(n, atk=0.003, dec=0.1):
    e = np.exp(-np.arange(n) / (SR * dec))
    k = int(SR * atk)
    if k > 0:
        e[:k] *= np.linspace(0, 1, k)
    return e


def save(name, x, peak=0.9):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype(np.int16).tobytes())
    print("ok", name)


# golpe de palo a la bocha: chasquido seco + golpe grave de madera
n = int(SR * 0.18); tt = t(0.18)
hit = bandpass(rng.standard_normal(n), 900, 4500) * env(n, .001, .018) + 0.8 * np.sin(2 * np.pi * 150 * tt) * env(n, .001, .05)
save("S_Hit", hit)

# rebote de la bocha en el parquet
n = int(SR * 0.12); tt = t(0.12)
save("S_Bounce", np.sin(2 * np.pi * 210 * tt * (1 - tt * 2)) * env(n, .001, .03) + 0.2 * bandpass(rng.standard_normal(n), 600, 2500) * env(n, .001, .01))

# golpe contra la valla
n = int(SR * 0.3); tt = t(0.3)
save("S_Board", 0.9 * np.sin(2 * np.pi * 95 * tt) * env(n, .001, .07) + bandpass(rng.standard_normal(n), 150, 900) * env(n, .001, .06))

# palo / travesaño: metal que resuena
n = int(SR * 0.9); tt = t(0.9)
post = sum(a * np.sin(2 * np.pi * f * tt) * np.exp(-tt * d) for f, a, d in ((880, 1, 5), (1340, .7, 7), (2110, .5, 9), (3050, .3, 12)))
save("S_Post", post + 0.5 * bandpass(rng.standard_normal(n), 2000, 7000) * env(n, .001, .01))

# red: roce suave
n = int(SR * 0.6)
save("S_Net", lowpass(rng.standard_normal(n), 2500) * np.sin(np.pi * np.arange(n) / n) ** 0.7, .7)

# atajada: golpe sordo de guante / polaina
n = int(SR * 0.22); tt = t(0.22)
save("S_Save", 0.8 * np.sin(2 * np.pi * 120 * tt) * env(n, .001, .05) + lowpass(rng.standard_normal(n), 1800) * env(n, .001, .04))

# silbato (dos pitidos)
tt = t(0.9)
w = np.zeros_like(tt)
for s, e in ((0.0, 0.32), (0.45, 0.85)):
    m = (tt >= s) & (tt < e)
    tm = tt[m] - s
    w[m] = np.sin(2 * np.pi * (2900 + 60 * np.sin(2 * np.pi * 30 * tm)) * tm) * np.minimum(1, tm * 60) * np.minimum(1, (e - s - tm) * 60)
save("S_Whistle", w, .6)

# hinchada que festeja: ruido filtrado que crece y se apaga
d = 3.2; n = int(SR * d)
crowd = bandpass(rng.standard_normal(n), 300, 2400)
a = np.minimum(1, np.arange(n) / (SR * 0.5)) * np.exp(-np.maximum(0, np.arange(n) / SR - 1.2) * 0.9)
mod = 1 + .3 * np.sin(2 * np.pi * 7 * t(d)) + .2 * np.sin(2 * np.pi * 3.1 * t(d))
save("S_Cheer", crowd * a * mod, .9)

# "uuuh": queja de la tribuna, grave y descendente
d = 1.4; n = int(SR * d); tt = t(d)
groan = lowpass(rng.standard_normal(n), 700) * (0.6 + 0.4 * np.sin(2 * np.pi * 5 * tt)) + 0.4 * np.sin(2 * np.pi * (180 - 60 * tt) * tt)
save("S_Groan", groan * np.sin(np.pi * np.arange(n) / n) ** 0.6, .8)

# ambiente de estadio: murmullo continuo, con fundido circular para que el loop no se corte
d = 12.0; n = int(SR * d)
amb = lowpass(rng.standard_normal(n), 900) * (0.55 + 0.45 * np.sin(2 * np.pi * 0.17 * t(d) + 1.0)) + 0.5 * bandpass(rng.standard_normal(n), 1200, 3500) * 0.25
k = int(SR * 1.0)
fade = np.linspace(0, 1, k)
amb[:k] = amb[:k] * fade + amb[-k:] * (1 - fade)
amb = amb[:-k]
save("S_Ambiente", amb, .5)
