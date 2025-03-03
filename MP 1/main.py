'''
Name: Justin Ngo
PSU Email: jvn5439
PSUID: 933902868
Class: CMPEN 462
Assignment: Mini Project 1
Part: main file to run OFDM
Date: 03.01.25
'''
import scipy.io as sio
import numpy as np
from OFDM import OFDM


# General Variables
fft_size = 4096 
symbols_per_slot = 14


#1. Bit Stream Generator
phrase = 'WirelessCommunicationSystemsandSecurityJustinNgo'
bit_stream = ''.join(format(ord(char), '08b') for char in phrase)

# FFT size * bits per 64QAM symbol * symbols per slot
required_length = fft_size * 6 * symbols_per_slot
bit_stream = (bit_stream * (required_length // len(bit_stream) + 1))[:required_length]

# Save bit stream
sio.savemat('bistream.mat', {'bit_stream': np.array(list(bit_stream), dtype=int)})

#toggle to print the bitstream, leaving it off so we dont flood the console when running the code
#print("Generated bit stream:", bit_stream)


bpskOFDM = OFDM(bit_stream, "BPSK")
bpskOFDM = OFDM(bit_stream, "Pi_by_2_BPSK")
bpskOFDM = OFDM(bit_stream, "QPSK")
bpskOFDM = OFDM(bit_stream, "64QAM")
