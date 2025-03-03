'''
Name: Justin Ngo
PSU Email: jvn5439
PSUID: 933902868
Class: CMPEN 462
Assignment: Mini Project 1
Part: OFDM, handles all logic and houses majority of  functions for the OFDM process
Date: 03.01.25
'''
import numpy as np
import matplotlib.pyplot as plt
import scipy.io as sio
from Modulators import bpsk, pi2_bpsk, qpsk, qam64


# General Variables
fft_size = 4096 
subcarrier_spacing = 60e3                       # 60kHz
sample_rate = fft_size * subcarrier_spacing     # should be 245.76MHz

# little confused about the timing unit, ik the book uses 480khz but not sure if thats just an example 
# or a standard for calculation, using 60khz for now to match project guidelines
timing_unit = 1/(fft_size * subcarrier_spacing) # should be about 4.07ns


# OFDM function to run the entire process from main
def OFDM(bit_stream: str, mod_type: str):
    #2. check the modulation type before running rest of code
    if mod_type == 'BPSK':
        symbols = bpsk(bit_stream)
    elif mod_type == 'Pi_by_2_BPSK':
        symbols = pi2_bpsk(bit_stream)
    elif mod_type == 'QPSK':
        symbols = qpsk(bit_stream)
    elif mod_type == '64QAM':
        symbols = qam64(bit_stream)
    else:
        raise ValueError('Invalid modulation type')
    
    # Save modulated signals as .mat files
    sio.savemat(f'{mod_type.upper()}.mat', {mod_type.upper(): symbols})

    #3. & 5. run s2p conversion
    parallel_ifft = s2p_iff(symbols)

    #6. run cyclic prefix insertion
    transmit_symbols = cyc_pref(parallel_ifft)

    #8. plot the OFDM symbols
    plot_ofdm_symbols(transmit_symbols, mod_type)


#Serial to Parallel Conversion
def s2p_iff(symbols):
    # calculate the number of OFDM symbols needed
    num_symbols = len(symbols)
    ofdm_symbols = int(np.ceil(num_symbols / fft_size))
    
    # pad the input with zeros if necessary
    padding_length = ofdm_symbols * fft_size - num_symbols
    if (padding_length > 0):
        symbols = np.append(symbols, np.zeros(padding_length, dtype=complex))
    
    # reshape into parallel groups (2d array of 4096 rows and however many columns come from fft_size)
    parallel_symbols = symbols.reshape((ofdm_symbols, fft_size))

    #5. IFFT
    # pass  parallel symbols through IFFT since we dont worry about resource element mapping in this project
    parallel_ifft = np.fft.ifft(parallel_symbols, axis=1)

    #return a serialized version of parallel ifft so that we can perform cyclic prefix insertion
    return parallel_ifft


#Cyclic Prefix Insertion
def cyc_pref(ifft_out):
    # Variables for 60khz Normal Prefix
    transmit_symbols = []
    pref_dur_samples = 2304

    for symbol in ifft_out:
        # All symbols get normal CP
        # Extract the cyclic prefix from the end of the symbol
        cp = symbol[-pref_dur_samples:]
        
        # Prepend the CP to the symbol
        trans_OFDM_sym = np.concatenate((cp, symbol))
        transmit_symbols.append(trans_OFDM_sym)

    #7. Collect the transmit symbols in a buffer
    return np.array(transmit_symbols)


#Plot
def plot_ofdm_symbols(time_domain_symbols, mod_scheme):
    pref_dur_samples = 2304  # CP duration in samples (from the slides)
    
    # Extract fist two symbols from buffer
    symbol1 = time_domain_symbols[0]
    symbol2 = time_domain_symbols[1]
    
    # Combine the two symbols for plotting
    combined = np.concatenate((symbol1, symbol2))
    time_axis = np.arange(len(combined))
    
    # Plot real and imaginary parts
    plt.figure(figsize=(12, 6))
    plt.plot(time_axis, np.real(combined), label='Real Part')
    plt.plot(time_axis, np.imag(combined), label='Imaginary Part')
    
    # Mark the boundaries between symbols
    symbol_boundaries = [len(symbol1)]
    for boundary in symbol_boundaries:
        plt.axvline(x=boundary, color='r', linestyle='--', label='Symbol Boundary' if boundary == symbol_boundaries[0] else "")
    
    # Mark the CP regions
    cp_starts = [0, len(symbol1)]
    for start in cp_starts:
        plt.axvspan(start, start + pref_dur_samples, alpha=0.2, color='green', label='CP' if start == 0 else "")
    
    # Add labels and title
    plt.title(f'Two OFDM Symbols with Cyclic Prefix - {mod_scheme.upper()}')
    plt.xlabel('Sample Index')
    plt.ylabel('Amplitude')
    plt.legend()
    plt.grid(True)
    
    plt.show()