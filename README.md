# About

fft external in C for the Windows version of Max 8 (Max/MSP). *fl_blur~* is an external that works in pfft~, it receives magnitude and phase differences and accumulate the values from each bin to its neighbors, for the specified range, as a weighted sum.

The weights are determined by an internal window. The shape of the window can be selected as a blur mode, there are 5 available: Gaussian, Triangular, Cosine, Half-Sine, and Power of 4.

# Notes / To do list

- Technically there's no need of a delayed frame, it can be removed at the cost of more processing power, because the frame vector should be traversed twice with the present accumulation method. To do: try addressing neighbors for each index in the output vector.
- A buffer can be made to add an horizontal blur, it should spend a lot of memory and the result shouldn't be that different from the result of a fedbacked delay on the time domain before being processed by this object.
