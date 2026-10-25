# Part I: Sequential Baseline

Input image: images/1.ppm
Compiler: g++
Optimization: -O2
Measurements: microseconds converted to milliseconds

| Run | Read | S1 | S2 | S3 | Write | Total (ms) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 5.201 | 5.003 | 3.989 | 4.780 | 4.279 | 23.252 |
| 2 | 10.277 | 10.386 | 2.698 | 2.150 | 1.287 | 26.798 |
| 3 | 6.293 | 7.929 | 2.195 | 2.118 | 2.560 | 21.095 |
| 4 | 4.123 | 4.179 | 2.213 | 1.972 | 1.293 | 13.780 |
| 5 | 3.800 | 4.740 | 1.958 | 1.845 | 1.393 | 13.736 |
| **Average** | **5.939** | **6.447** | **2.611** | **2.573** | **2.162** | **19.732** |

The generated output was confirmed to match an existing Lab 1 output
at the pixel-data level.

These are preliminary measurements for a 450 x 180 image. Significant
run-to-run variation was observed; further measurements may be needed
for reliable performance comparisons.
