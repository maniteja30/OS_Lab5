# OS Lab 5 — Part II.2: Multi-Process Pipeline Using Pipes

## 1. Objective

Implement a three-process image sharpening pipeline using pipes for inter-process communication.

## 2. Pipeline Architecture

The pipeline consists of three processes:

- S1: Smoothing
- S2: Detail extraction
- S3: Sharpening

The processes communicate through two pipes:

S1 → Pipe 1 → S2 → Pipe 2 → S3

## 3. Implementation Details

- Used `fork()` to create separate processes for S1, S2, and S3.
- Used two pipes to transfer pixel records between the processes.
- Each pixel record contains a sequence number, original pixel values, and processed pixel values.
- S2 and S3 verify sequence numbers to detect out-of-order data.
- S3 writes the final sharpened image.

## 4. Variant 1 Results

| Measurement | Result |
|---|---:|
| Iterations | 5 |
| S3 processing time | 190.977 ms |
| File write time | 1.42414 ms |
| Total pipeline time | 194.765 ms |
| Output comparison | Identical to Lab 1 reference |

## 5. Variant 2 Results

| Measurement | Result |
|---|---:|
| Workload | Assignment-defined repetition counts |
| S3 processing time | 416.696 ms |
| File write time | 1.30292 ms |
| Total pipeline time | 420.096 ms |
| Output comparison | Identical to Lab 1 reference |

## 6. Correctness Verification

The output images were compared against the Lab 1 reference using:

```bash
cmp part2_2/output_test.ppm part2_1/lab1_reference.ppm
cmp part2_2/output_variant2_test.ppm part2_1/lab1_reference.ppm