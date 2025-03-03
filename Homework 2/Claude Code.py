import numpy as np
import matplotlib.pyplot as plt

# Signal parameters
fo = 240e3  # Fundamental frequency (240 KHz)
fs = 4096 * fo  # Sample rate
T = 1/fo  # Period of one pulse
duration_4_pulses = 4 * T  # Duration to show 4 pulses
V_max = 2.5  # Maximum voltage
V_min = 0.0  # Minimum voltage

# Time vector for 4 pulses
t = np.arange(0, duration_4_pulses, 1/fs)
samples_per_period = int(fs/fo)
duty_cycle = 0.5  # 50% duty cycle

# Generate square pulse train
pulse_train = np.zeros(len(t))
for i in range(len(t)):
    if (i % samples_per_period) < (duty_cycle * samples_per_period):
        pulse_train[i] = V_max
    else:
        pulse_train[i] = V_min

# Calculate FFT
# For FFT, we'll use more periods to get better frequency resolution
n_periods = 100  # Using 100 periods for FFT
t_fft = np.arange(0, n_periods * T, 1/fs)
pulse_train_fft = np.zeros(len(t_fft))
for i in range(len(t_fft)):
    if (i % samples_per_period) < (duty_cycle * samples_per_period):
        pulse_train_fft[i] = V_max
    else:
        pulse_train_fft[i] = V_min

fft_result = np.fft.fft(pulse_train_fft)
freq = np.fft.fftfreq(len(t_fft), 1/fs)

# Plotting
plt.figure(figsize=(15, 8))

# Time domain plot
plt.subplot(2, 1, 1)
plt.plot(t*1e6, pulse_train, 'b-', linewidth=2)
plt.grid(True)
plt.xlabel('Time (μs)')
plt.ylabel('Amplitude (V)')
plt.title('Square Pulse Train (Time Domain)')
plt.ylim([V_min-0.2, V_max+0.2])

# Frequency domain plot
plt.subplot(2, 1, 2)
plt.plot(freq[:len(freq)//2]/1e6, np.abs(fft_result[:len(freq)//2]), 'r-')
plt.grid(True)
plt.xlabel('Frequency (MHz)')
plt.ylabel('Magnitude')
plt.title('Frequency Spectrum (No FFT Shift)')
plt.xlim([0, 5*fo/1e6])  # Show up to 5th harmonic

plt.tight_layout()
plt.show()