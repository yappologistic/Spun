#!/usr/bin/env python3
"""Synthesize Spun's original 32-second soundcheck; no downloaded samples."""
import array
import math
import os
import pathlib
import subprocess
import tempfile
import wave

root = pathlib.Path(__file__).resolve().parents[1]
rate, seconds = 44100, 32
chords = [(48, 55, 59, 62), (45, 52, 55, 59), (41, 48, 52, 55), (43, 50, 55, 57)]
melody = [72, 74, 79, 76, 74, 71, 67, 69, 72, 76, 79, 83, 81, 79, 74, 72]
tau = math.tau
def frequency(note):
    return 440 * 2 ** ((note - 69) / 12)

frequencies = [[frequency(n) for n in chord] for chord in chords]
notes = [frequency(n) for n in melody]
pcm = array.array('h')
for sample in range(rate * seconds):
    t = sample / rate
    segment = min(3, int(t / 8))
    local = t % 8
    fade = min(1, t / 2, (seconds-t) / 3)
    pad_envelope = min(1, local / 1.2, (8-local) / 1.2)
    left = right = 0.0
    for k, f in enumerate(frequencies[segment]):
        tone = .024 * pad_envelope * (math.sin(tau*f*t) + .24*math.sin(tau*f*2*t))
        left += tone * (.85 + .15*math.sin(t*.23+k))
        right += tone * (.85 + .15*math.cos(t*.23+k))
    step = min(15, int(t / 2))
    age = t % 2
    bell = .046 * (1-math.exp(-age*45)) * math.exp(-age*2.2) * (math.sin(tau*notes[step]*t) + .2*math.sin(tau*notes[step]*3*t))
    left += bell*.8
    right += bell
    for value in (left, right):
        pcm.append(round(max(-1, min(1, value*fade)) * 32767))

with tempfile.TemporaryDirectory(prefix='spun-soundcheck-') as temp:
    temp = pathlib.Path(temp)
    wav, art = temp / 'first-light.wav', temp / 'cover.png'
    with wave.open(str(wav), 'wb') as out:
        out.setnchannels(2); out.setsampwidth(2); out.setframerate(rate); out.writeframes(pcm.tobytes())
    subprocess.run([str(root/'build/spun'), '-platform', 'offscreen', '--export-cover', str(art)], check=True,
                   env={**os.environ, 'QT_QPA_PLATFORMTHEME': ''})
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-i', str(wav), '-i', str(art),
                    '-map', '0:a', '-map', '1:v', '-c:a', 'flac', '-c:v', 'png', '-disposition:v', 'attached_pic',
                    '-metadata', 'title=First Light', '-metadata', 'artist=Spun Sound Lab',
                    '-metadata', 'album=The first mixtape', '-metadata', 'comment=Original synthesized soundcheck. No third-party samples.',
                    str(root/'assets/First-Light.flac')], check=True)
print('Created assets/First-Light.flac')
