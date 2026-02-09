# This is a sample Python script.

# Press Shift+F10 to execute it or replace it with your code.
# Press Double Shift to search everywhere for classes, files, tool windows, actions, and settings.

import wave
import numpy as np
import math

import matplotlib.pyplot as plt

def generate_sin(num):
    signal = []

    for i in range(0, 255):
        signal.append( int( 255*math.sin(math.pi*num*(i >> 0)/128.0)))

    return signal

def generate_tri():
    pass
    signal = []
    for i in range(0, 128):
            signal.append(2* (i >> 0))

    signal2 = signal.copy()
    signal2.reverse()

    return signal + signal2

def generate_saw(num):
    signal = []
    for i in range( int(round(256 / num))):
        signal.append(num*i)

    return np.tile(signal, num)


def read_wav():
    pass
    file = wave.open("organ_sample.wav", 'r')
    wav = file.readframes(268)

    test3 = np.frombuffer(wav, dtype='int16')

    #print(test3)

    return test3


def print_signal(signal):
    stri = "const __flash uint8_t s[] = {"
    for s in signal:
        stri = stri + str(int(s)) + ","

    print(stri + "};")


def write_wav(signal, filename):
    samplerate = 44100

    # A note on the left channel for 1 second.
    t = np.linspace(0, 1, samplerate)
    audio =  (1/255.0) * np.array(signal)

    # Put the channels together with shape (2, 44100).

    # Convert to (little-endian) 16 bit integers.
    audio = (audio * (2 ** 15 - 1)).astype("<h")

    with wave.open(filename, "w") as f:

        f.setnchannels(1)
        # 2 bytes per sample.
        f.setsampwidth(2)
        f.setframerate(samplerate)
        f.writeframes(audio.tobytes())


# Press the green button in the gutter to run the script.
if __name__ == '__main__':
    signal = generate_sin(1)


    test = read_wav()


    sign = []
    nums = []

    for t in test:
        #print(str(t) + ", ")
        sign.append(t)
        nums.append(int(t/110.0) + 136+5)


    with wave.open("test.wav", "w") as f:

        f.setnchannels(1)
        # 2 bytes per sample.
        f.setsampwidth(2)
        f.setframerate(44100)
        f.writeframes(np.tile(test, 268).tobytes())


    signal = generate_sin(1)

    signal = generate_sin(3)
    print(len(signal))
    print(max(signal))

    print(signal)
    plt.plot(signal)
    plt.show()

    #write_wav(np.tile(test/10000.0, 100), "test.wav")



# See PyCharm help at https://www.jetbrains.com/help/pycharm/
