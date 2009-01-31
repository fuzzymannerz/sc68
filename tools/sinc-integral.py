#!/usr/bin/python

import math
import sys
from scipy import array, exp
from scipy.signal import firwin
from scipy.fftpack import fft, ifft

def fir_minphase(table, pad_size=8):
    table = list(table)
    # table should be a real-valued table of FIR coefficients
    convolution_size = len(table)
    table += [0] * (convolution_size * (pad_size - 1))

    # compute the real cepstrum
    # fft -> abs + ln -> ifft -> real
    cepstrum = ifft(map(lambda x: math.log(x), abs(fft(table))))
    # because the positive and negative freqs were equal, imaginary content is neglible
    # cepstrum = map(lambda x: x.real, cepstrum)

    # window the cepstrum in such a way that anticausal components become rejected
    cepstrum[1                :len(cepstrum)/2] *= 2;
    cepstrum[len(cepstrum)/2+1:len(cepstrum)  ] *= 0;

    # now cancel the previous steps:
    # fft -> exp -> ifft -> real
    cepstrum = ifft(map(exp, fft(cepstrum)))
    return map(lambda x: x.real, cepstrum[0:convolution_size])

class BiquadFilter(object):
    __slots__ = ['b0', 'b1', 'b2', 'a1', 'a2', 'x1', 'x2', 'y1', 'y2']

    def __init__(self, b0, b1, b2, a1, a2):
        self.b0 = b0
        self.b1 = b1
        self.b2 = b2
        self.a1 = a1
        self.a2 = a2
        self.reset()

    def reset(self):
        self.x1 = 0.0
        self.x2 = 0.0
        self.y1 = 0.0
        self.y2 = 0.0

    def filter(self, x0):
        y0 = self.b0*x0 + self.b1*self.x1 + self.b2*self.x2 - self.a1*self.y1 - self.a2*self.y2
        self.x2 = self.x1
        self.x1 = x0

        self.y2 = self.y1
        self.y1 = y0

        return y0

def make_rc_lopass(sample_rate, freq):
    omega = 2 * math.pi * freq / sample_rate;
    term = 1 + 1/omega;
    return BiquadFilter(1/term, 0.0, 0.0, -1.0 + 1/term, 0.0);

def quantize(x, bits, scale=False):
    x = list(x)
    fact = 2 ** bits

    # this adjusts range precisely between -65536 and 0 so that our bleps look right.
    # we should only do this if the table is integrated; in fact this code probably
    # belonged to the integration function.
    correction_factor = 1.0
    if scale:
        correction_factor = x[-1] - x[0]

    err = 0;
    for _ in range(len(x)):
        val = x[_] * fact / correction_factor - err;
        # correct rounding
        if val < 0:
            intval = int(val - 0.5)
        else:
            intval = int(val + 0.5)

        # error feedback
        err = intval - val;

        # leave scaled?
        if not scale:
            intval /= float(fact)
        x[_] = intval * -1
    return x

def lin2db(lin):
    return 20 * (math.log(lin) / math.log(10))

def print_spectrum(table, sample_rate):
    for _ in range(len(table) / 2):
        mag = lin2db(abs(table[_]))
        pha = math.atan2(table[_].real, table[_].imag)
        print "%s %s %s" % (float(_) / len(table) * sample_rate, mag, pha)

def print_fir(table, format='gnuplot'):
    if format == 'gnuplot':
        for _ in range(len(table)):
            print "%s %s" % (_, table[_])
    elif format == 'c':
        col = 0
        for _ in range(len(table)):
            col += len(str(table[_])) + 1
            if col >= 80:
                print
                col = 0
            sys.stdout.write("%s," % table[_])
        if col != 0:
            print

def integrate(table):
    total = 0
    for _ in table:
        total += _
    startval = -total
    new = []
    for _ in table:
        startval += _
        new.append(startval)
    
    return new

def run_filter(flt, table):
    flt.reset()

    # initialize filter to stable state
    for _ in range(10000):
        flt.filter(table[0])

    # now run the filter
    newtable = []
    for _ in range(len(table)):
        newtable.append(flt.filter(table[_]))

    return newtable, abs(table[-1] - flt.filter(table[-1]))

def main():
    spectrum = len(sys.argv) > 1

    # The output should be aliasingless for 48 kHz sampling frequency.
    # Sligh aliasing to 19 kHz for 44.1 kHz. SNR is only about 80 dB.
    # likely there are some sampling noise from phase errors, too...
    unfiltered = firwin(1024, 20000.0 / 2e6 * 2, window=('kaiser', 9.0))

    # move filtering effects to start to allow IIRs more time to settle
    unfiltered = fir_minphase(unfiltered)

    filter_fixed = make_rc_lopass(2e6, 10000.0)

    # apply fixed filter
    filtered, error = run_filter(filter_fixed, unfiltered)

    if not spectrum:
        # integrate to produce blep
        filtered = integrate(filtered)
    
    # quantize and scale
    filtered = quantize(filtered, bits=16, scale=(not spectrum))

    if spectrum:
        table = list(filtered);
        table += [0] * (16384 - len(table))
        print_spectrum(fft(table), sample_rate=2e6)
    else:
        print " /*"
        print "  * Table generated by contrib/sinc-integral.py."
        print "  * residual: %f dB" % lin2db(error)
        print "  */"
        print
        print "const int sine_integral[%d] = {" % len(filtered)
        print_fir(filtered, format='c')
        print "};"

if __name__ == '__main__':
    main()

