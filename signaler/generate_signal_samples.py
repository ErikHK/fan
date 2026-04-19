import wave
import numpy as np
import math

import matplotlib.pyplot as plt
from main import print_signal

leng = 65536
top = 16383
bottom = -16383


def generate_sin():
    signal = []

    for i in range(0, leng-1):
        signal.append( (top-bottom) * math.sin(2*math.pi*i / leng) / 2)

    return np.array(signal)


def generate_soft_sin():
    signal = []
    amp = (top-bottom)/2

    for i in range(0, leng-1):
        signal.append( amp * ( math.sin(2*math.pi*i / leng) + math.sin(2*math.pi*3*i/leng) / 3 + math.sin(2*math.pi*5*i/leng) / 5 + math.sin(2*math.pi*7*i/leng) / 7 ) )

    return np.array(signal)

def generate_saw():
    signal = []
    for i in range(0, leng-1):
        signal.append( i )

    return np.array(signal) * ((top-bottom) / leng) - top

def generate_organ():
    signal = []
    amp = (top - bottom) / 2

    leng2 = leng/12

    for i in range(0, leng-1):
        signal.append( (amp/7) * ( math.sin(2*math.pi*i / leng2)
                                    + math.sin(2*math.pi*1.25*i/leng2) +
                                    math.sin(2*math.pi*1.33*i/leng2) +
                                    math.sin(2*math.pi*1.66*i/leng2) +
                                    math.sin(2*math.pi*2*i/leng2) +
                                    math.sin(2*math.pi*2.33*i/leng2) +
                                    math.sin(2*math.pi*2.5*i/leng2)) )

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

def generate_zeros(num):
    signal = []
    for i in range(num):
        signal.append(0)
    return signal


def print_signal(signal):
    stri = "const __flash uint8_t s[] = {"
    for s in signal:
        stri = stri + str(int(s)) + ","

    print(stri + "};")

#signal = generate_saw()

#signal = generate_organ()

signal = generate_zeros(10000)

print_signal(signal)

plt.plot(np.append(signal, signal))
plt.show()
print(max(signal))