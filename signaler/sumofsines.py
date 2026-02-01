import wave
import numpy as np
import math

import matplotlib.pyplot as plt
from main import print_signal

def generate_sin(num):
    signal = []

    for i in range(0, 255):
        signal.append( int( 255*math.sin(math.pi*num*(i >> 0)/128.0)))

    return np.array(signal)


def write_wav(signal, filename):
    samplerate = 44100

    maxv = max(signal)

    # A note on the left channel for 1 second.
    t = np.linspace(0, 1, samplerate)
    audio =  (1/maxv) * np.array(signal)

    # Put the channels together with shape (2, 44100).

    # Convert to (little-endian) 16 bit integers.
    audio = (audio * (2 ** 15 - 1)).astype("<h")

    with wave.open(filename, "w") as f:

        f.setnchannels(1)
        # 2 bytes per sample.
        f.setsampwidth(2)
        f.setframerate(samplerate)
        f.writeframes(audio.tobytes())



def generate_sin_sum(harmonics):
    signal = np.zeros(255)
    for h in harmonics:
        signal += generate_sin(h)[:255]

    return signal/len(harmonics)

def generate_long_sine():
    signal = []
    for i in range(0, 4096):
        signal.append( int( 127*math.sin(math.pi*2*(i >> 0)/4096.0) + 128))
    return signal

def generate_long_saw():
    signal = []
    for i in range(0, 4096):
        signal.append( i >> 4)

    return signal

def sample_signal(insignal, step):
    signal = []
    for i in range(0, len(insignal), step):
        signal.append(insignal[i])
    return signal


def read_wav(filename):
    pass
    file = wave.open(filename, 'r')
    wav = file.readframes(file.getnframes())

    print (file.getnframes())

    signal = np.frombuffer(wav, dtype='int16')

    #print(test3)

    return signal

def samplify_signal(insignal):
    signal = insignal.copy()
    maxv = max(insignal)
    minv = min(insignal)
    diff = maxv-minv

    signal = 255*(signal/diff)
    return signal+abs(min(signal))



signal =  samplify_signal( read_wav("mjau_16k.wav") / 100.0 )

#signal = generate_long_sine()

print(max(signal))

print(min(signal))



print_signal(signal)
print(len(signal))

plt.plot(signal)
plt.show()
