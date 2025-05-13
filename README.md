# About

fft external in C for the Windows version of Max 8 (Max/MSP). *fl_blur~* is an external that works in pfft~, it receives magnitude and phase differences and accumulate the values from each bin to its neighbors, for the specified range, as a weighted sum.

The weights are determined by an internal window. The shape of the window can be selected as a blur mode, there are 5 available: Gaussian, Triangular, Cosine, Half-Sine, and Power of 4.