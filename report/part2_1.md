# Part II.1 — Multi-Threaded Pipeline

## Implementation

The image sharpening pipeline consists of three stages:

- **S1:** Smooth the input image using a 3x3 neighborhood.
- **S2:** Compute image details by subtracting the smoothed image from the original image.
- **S3:** Sharpen the image by adding the details to the original image.

Three threads execute the stages concurrently:

- Thread 1 executes S1.
- Thread 2 executes S2.
- Thread 3 executes S3.

Atomic counters coordinate the production and consumption of intermediate results. Two buffer slots are used to allow stages to operate concurrently while preventing a stage from overwriting data that another stage has not yet consumed.

## Variant 1

Variant 1 processes pixels through the three-stage pipeline.

The iteration count can be specified as a command-line argument. The default is 1000 iterations.

### Test

Input image: `images/1.ppm`

Image dimensions: 450 x 180

Iterations: 5

| Measurement | Result |
|---|---:|
| Processing time | 202.371 ms |
| Output writing time | 2.93857 ms |

## Variant 2

Variant 2 uses the fixed repetition counts:

- S1 performs 1000 complete image runs.
- Every 100th S1 result is passed to S2, resulting in 10 handoffs.
- For each S1 handoff, S2 performs 100 complete image runs.
- For each S2 handoff, S3 performs 100 complete image runs.
- The output is the result of the final S3 run.

### Test

Input image: `images/1.ppm`

Image dimensions: 450 x 180

| Measurement | Result |
|---|---:|
| Processing time | 3716.37 ms |
| Output writing time | 2.19367 ms |

## Correctness Verification

The outputs from both variants were compared byte-for-byte against the Lab 1 reference image using `cmp`.

Both comparisons returned exit code 0, indicating that the files were identical.

- Variant 1 output: identical to the Lab 1 reference.
- Variant 2 output: identical to the Lab 1 reference.

## Conclusion

Both variants compiled and executed successfully. The output images matched the Lab 1 reference exactly.
