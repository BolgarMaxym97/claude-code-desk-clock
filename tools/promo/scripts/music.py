"""Original lo-fi track for the promo (made for this project, no third-party audio).
84 BPM, swung; FM electric piano chords (Ebmaj9 - Cm9 - Abmaj7 - Bb6sus), sine bass, soft kick / snare / hats,
a small bell motif, vinyl crackle, Schroeder reverb on the keys. Usage: music.py out.wav"""
import math, random, struct, sys, wave

SR, BPM, DUR = 44100, 84, 40.5  # = promo length (S.end / FPS)
BEAT = 60 / BPM
SWING = 0.18 * BEAT  # delay of every off-beat 8th
N = int(SR * DUR)
keys = [0.0] * N   # e-piano + bell bus (gets reverb)
dry = [0.0] * N    # bass + drums + crackle
rnd = random.Random(11)
TAU = 2 * math.pi

def hz(m):
    return 440 * 2 ** ((m - 69) / 12)

def add(bus, start, dur, fn, vol):
    a, b = int(start * SR), min(N, int((start + dur) * SR))
    for i in range(max(0, a), b):
        bus[i] += vol * fn((i - a) / SR)

def epiano(f):  # FM: carrier 1:1, modulator index falls fast -> "tine" attack, then a soft body
    def g(t):
        idx = 1.6 * math.exp(-t * 7) + 0.25
        env = (1 - math.exp(-t * 300)) * math.exp(-t * 1.6)
        trem = 1 + 0.08 * math.sin(TAU * 4.5 * t)
        return env * trem * math.sin(TAU * f * t + idx * math.sin(TAU * f * t))
    return g

def bell(f):
    def g(t):
        env = (1 - math.exp(-t * 400)) * math.exp(-t * 2.4)
        return env * math.sin(TAU * f * t + 1.2 * math.exp(-t * 3) * math.sin(TAU * f * 3.5 * t))
    return g

def bass(f):
    def g(t):
        env = (1 - math.exp(-t * 120)) * math.exp(-t * 1.1)
        x = math.sin(TAU * f * t) + 0.25 * math.sin(TAU * 2 * f * t)
        return env * math.tanh(1.4 * x)
    return g

def kick(t):
    return math.exp(-t * 14) * math.sin(TAU * (48 + 70 * math.exp(-t * 35)) * t)

def snare(t):
    return 0.6 * math.exp(-t * 22) * (rnd.random() * 2 - 1) + 0.4 * math.exp(-t * 30) * math.sin(TAU * 190 * t)

def hat(t):
    return math.exp(-t * 90) * (rnd.random() * 2 - 1)

# Ebmaj9, Cm9, Abmaj7, Bb6sus (voicings around middle C), bass roots
CHORDS = [([63, 67, 70, 74, 77], 39), ([60, 63, 67, 70, 74], 36), ([60, 63, 67, 68, 72], 44), ([62, 65, 67, 70, 72], 46)]
bar = 4 * BEAT
for b in range(int(DUR / bar) + 1):
    notes, root = CHORDS[b % 4]
    t0 = b * bar
    for hit, vel in ((0, 0.9), (1.5 * BEAT + SWING, 0.55)):  # chord on 1, ghost re-hit on the swung "and" of 2
        for k, m in enumerate(notes):
            add(keys, t0 + hit + k * 0.012, 2.6, epiano(hz(m)), 0.055 * vel)  # slight strum
    add(dry, t0, 1.8 * BEAT, bass(hz(root)), 0.30)
    add(dry, t0 + 2.5 * BEAT + SWING, 1.2 * BEAT, bass(hz(root + 7)), 0.22)
    if b >= 1:
        for q in range(4):
            if q in (0, 2) or (q == 3 and b % 2):
                add(dry, t0 + q * BEAT + (SWING if q == 3 else 0), 0.4, kick, 0.55)
            if q in (1, 3):
                add(dry, t0 + q * BEAT, 0.25, snare, 0.16)
            for e in (0, 1):
                add(dry, t0 + q * BEAT + e * (BEAT / 2 + SWING), 0.05, hat, 0.035 if e else 0.05)
# bell motif over bars 2-5, 6-9 and 10-13
MOTIF = [(0, 79), (0.5, 82), (1.5, 84), (3, 82), (4, 79), (5.5, 77), (6, 75), (8, 74), (9.5, 75), (10, 77)]
for start in (2 * bar, 6 * bar, 10 * bar):
    for beat, m in MOTIF:
        add(keys, start + beat * BEAT, 1.6, bell(hz(m)), 0.045)
# vinyl crackle + hiss
for i in range(N):
    dry[i] += 0.004 * (rnd.random() * 2 - 1)
    if rnd.random() < 0.0004:
        dry[i] += (rnd.random() * 2 - 1) * 0.12

def reverb(x):  # Schroeder: 4 parallel combs + 2 allpasses
    out = [0.0] * len(x)
    for d, g in ((1557, 0.80), (1617, 0.79), (1491, 0.81), (1422, 0.78)):
        buf = [0.0] * d
        j = 0
        for i, v in enumerate(x):
            y = buf[j]
            buf[j] = v + y * g
            out[i] += y * 0.25
            j = (j + 1) % d
    for d, g in ((225, 0.5), (556, 0.5)):
        buf = [0.0] * d
        j = 0
        for i, v in enumerate(out):
            y = buf[j]
            buf[j] = v + y * g
            out[i] = y - g * v
            j = (j + 1) % d
    return out

wet = reverb(keys)
mix = [dry[i] + keys[i] * 0.8 + wet[i] * 0.45 for i in range(N)]
lp, a = 0.0, 0.35  # one-pole low-pass: warm, lo-fi top end
for i in range(N):
    lp += a * (mix[i] - lp)
    mix[i] = lp
peak = max(abs(v) for v in mix) or 1
with wave.open(sys.argv[1], 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(b''.join(struct.pack('<h', int(32767 * 0.89 * v / peak)) for v in mix))
print('wrote', sys.argv[1], DUR, 's')
