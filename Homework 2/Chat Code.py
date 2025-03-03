import numpy as np
import matplotlib.pyplot as plt

# Given parameters
fo = 240e3  # Fundamental frequency in Hz
fs = 4096 * fo  # Sampling frequency
T = 250e-3  # Duration in seconds
A_max = 2.5  # Max amplitude
A_min = 0.0  # Min amplitude

# Time vector
t = np.arange(0, T, 1/fs)

# Square wave generation (50% duty cycle)
signal = A_max * (np.sign(np.sin(2 * np.pi * fo * t)) > 0)

# Compute FFT
N = len(signal)  # Number of points
freqs = np.fft.fftfreq(N, d=1/fs)  # Frequency vector
fft_signal = np.fft.fft(signal)  # Compute FFT

# Plot time-domain representation (showing 4 pulses only)
num_pulses = 4
t_display = t[:int(num_pulses * fs / fo)]
signal_display = signal[:int(num_pulses * fs / fo)]

plt.figure(figsize=(10, 4))
plt.plot(t_display * 1e6, signal_display, 'b')  # Convert time to microseconds
plt.xlabel('Time (μs)')
plt.ylabel('Amplitude (V)')
plt.title('Time-Domain Signal (4 Pulses Shown)')
plt.grid()
plt.show()

# Plot frequency-domain representation (linear amplitude)
plt.figure(figsize=(10, 4))
plt.plot(freqs[:N//2] / 1e6, np.abs(fft_signal[:N//2]), 'r')  # Convert Hz to MHz
plt.xlabel('Frequency (MHz)')
plt.ylabel('Amplitude')
plt.title('Frequency-Domain Representation')
plt.grid()
plt.show()
