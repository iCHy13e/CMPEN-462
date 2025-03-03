# Name: Justin Ngo
# PSU Email: jvn5439
# PSUID: 933902868
# Class: CMPEN 462
# Assignment: Homework 2
# Date: 02.17.2025

import numpy as np
import matplotlib.pyplot as plt

'''
References: 
https://numpy.org/doc/stable/reference/routines.fft.html
https://www.geeksforgeeks.org/plotting-a-square-wave-using-matplotlib-numpy-and-scipy/
https://stackoverflow.com/questions/66000468/plot-square-wave-in-python
https://psu.instructure.com/courses/2378968/files/174067072?module_item_id=44321606 (Sines_FFT_example.m)
https://psu.instructure.com/courses/2378968/files/174066175?module_item_id=44321556 (pulse_trains.m)
https://psu.instructure.com/courses/2378968/files/174066916?module_item_id=44321597 (Example_tone_FFT.m)
'''


# Signal parameters
fo = 240e3      # Fundamental frequency (240 KHz)
fs = 4096 * fo  # Sample frequenct (~983MHz)
t = 250e-3      # Duration in seconds (250 ms)
A_max = 2.5     # Max Amplitude (2.5V)
A_min = 0.0     # Min Amplitude (0V)


# Time vector
t = np.arange(0, t, 1/fs)
samples = int(fs/fo) # samples per period

# Generate square pulse train
signal = A_max * (np.sign(np.sin(2 * np.pi * fo * t)) > 0)

# Calculate FFT
n = len(signal)  # Number of points
freqs = np.fft.fftfreq(n, d=1/fs)  # Frequency vector
fft_signal = np.fft.fft(signal)  # Compute FFT

# Apply fftshift and normalize FFT
freqs_shifted = np.fft.fftshift(freqs)
fft_signal_shifted = np.fft.fftshift(fft_signal)
fft_signal_normalized = fft_signal_shifted / n  # Remove FFT gain by dividing by n

pulses = 4
t_display = t[:int(pulses * fs / fo)] # Time vector for 4 pulses
signal_display = signal[:int(pulses * fs / fo)] # Signal vector for 4 pulses

# Graphs 
plt.figure(figsize=(15, 8))

# Time domain plot
plt.subplot(2, 1, 1)
plt.plot(t_display * 1e6, signal_display, 'b-', linewidth=2)
plt.grid(True)
plt.xlabel('Time (microseconds)')
plt.ylabel('Amplitude (V)')
plt.title('Square Pulse Train (Time Domain)')
plt.ylim([A_min - 0.2, A_max + 0.2])

# Frequency domain plot
plt.subplot(2, 1, 2)
plt.plot(freqs_shifted/1e6, np.abs(fft_signal_normalized), 'r-')
plt.grid(True)
plt.xlabel('Frequency (MHz)')
plt.ylabel('Magnitude')
plt.title('Frequency Spectrum (with FFT Shift)')
# Adjust x-axis limits to show frequencies centered around 0
plt.xlim([-8*fo/1e6, 8*fo/1e6])  # Show up to +-5th harmonic

plt.tight_layout()
plt.show()