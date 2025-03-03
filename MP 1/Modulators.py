'''
Name: Justin Ngo
PSU Email: jvn5439
PSUID: 933902868
Class: CMPEN 462
Assignment: Mini Project 1
Part: Modulator file for different schemes so that the main code file looks cleaner and is easier to read
Date: 03.01.25
'''
import numpy as np


# BPSK 1 bit per symbol
def bpsk(input_bits):
    symbols = []
    for bit in input_bits:
        if bit == '0':
            symbols.append(1 + 0j)  # Map to 1
        else:
            symbols.append(-1 + 0j) # Map to -1
    return np.array(symbols)


# pi/2-BPSK 1 bit per symbol
def pi2_bpsk(input_bits):
    symbols = []

    for i, bit in enumerate(input_bits):
        if bit == '0':
            # Apply rotation of (π/2) * n to the normal BPSK constellation
            rotation = (np.pi/2) * i
            symbols.append(np.exp(1j * (np.pi + rotation)))
        else:
            rotation = (np.pi/2) * i
            symbols.append(np.exp(1j * rotation))
    return np.array(symbols)


# QPSK 2 bits per symbol
def qpsk(input_bits):
    symbols = []
    for i in range(0, len(input_bits), 2):
        # Get pair of bits
        if i+1 < len(input_bits):  
            if input_bits[i:i+2] == '00':
                symbols.append((1/np.sqrt(2)) * (1 + 1j))  # upper right
            
            elif input_bits[i:i+2] == '01':
                symbols.append((1/np.sqrt(2)) * (1 - 1j))  # lower right
            
            elif input_bits[i:i+2] == '10':
                symbols.append((1/np.sqrt(2)) * (-1 + 1j)) # upper left 
            
            elif input_bits[i:i+2] == '11':
                symbols.append((1/np.sqrt(2)) * (-1 - 1j)) # lower left
    return np.array(symbols)


# 64QAM 6 bits per symbol
def qam64(input_bits):
    symbols = []
    for i in range(0, len(input_bits), 6):
        if i+5 < len(input_bits):  # Ensure we have 6 bits
            
            # Extract the 6 bits
            b_0 = int(input_bits[i])
            b_1 = int(input_bits[i+1])
            b_2 = int(input_bits[i+2])
            b_3 = int(input_bits[i+3])
            b_4 = int(input_bits[i+4])
            b_5 = int(input_bits[i+5])

            # Calculate real and imaginary
            real = (1-2*b_0)*(4-(1-2*b_2)*(2-(1-2*b_4)))
            imag = (1-2*b_1)*(4-(1-2*b_3)*(2-(1-2*b_5)))
            
            # Normalize by √42
            symbol = complex(real, imag) / np.sqrt(42)
            symbols.append(symbol)
    return np.array(symbols)