
# Part II.3 — Shared Memory and Semaphores

## Objective

Implement a three-stage image-processing pipeline using shared memory and process-shared semaphores for synchronization.

The stages are:
- **S1:** Reads the input image and performs the first processing stage.
- **S2:** Performs the second processing stage.
- **S3:** Performs the final processing stage and writes the output image.

The processes communicate through a shared-memory buffer. Semaphores coordinate access to the buffer and ensure that a producer does not overwrite data before the consumer has processed it.

## Implementation

- Shared memory is allocated using `mmap()` with `MAP_SHARED`.
- Process-shared semaphores are initialized using `sem_init()` with `pshared = 1`.
- The `empty` semaphore indicates whether the shared buffer is available.
- The `full` semaphore indicates whether the shared buffer contains data.
- The pipeline uses sequence numbers to maintain the order of pixel messages.
- A completion message is passed through the stages to indicate the end of processing.

## Correctness Testing

The implementation was compiled successfully.

Both variants were tested using `images/1.ppm`. Their output images were compared byte-for-byte against the Lab 1 reference output using `cmp`.

| Variant | Test | Result |
|---|---|---|
| Variant 1 | 5 iterations | Output matched the reference |
| Variant 2 | Default workload | Output matched the reference |

No output from `cmp` indicates that the generated output was byte-for-byte identical to the reference image.

## Performance Results

### Variant 1

Command:

```bash
./part2_3/shared_memory_pipeline images/1.ppm part2_3/output_test.ppm 1 5
```

| Metric | Time |
|---|---:|
| S3 processing time | 1442.82 ms |
| File write time | 1.26246 ms |
| Total pipeline time | 1446.74 ms |

### Variant 2

Command:

```bash
./part2_3/shared_memory_pipeline images/1.ppm part2_3/output_variant2_test.ppm 2
```

| Metric | Time |
|---|---:|
| S3 processing time | 5027.52 ms |
| File write time | 1.4738 ms |
| Total pipeline time | 5031.38 ms |

## Observations

- Both variants produced output identical to the Lab 1 reference.
- The implementation uses shared memory to exchange pixel messages between processes.
- Semaphore synchronization introduces overhead, particularly when synchronization occurs for every pixel.
- The measured runtimes should not be directly compared because Variant 1 was tested with 5 iterations, while Variant 2 used its default workload.

## Conclusion

Part II.3 was implemented using shared memory and process-shared semaphores. Both variants passed the output correctness test against the Lab 1 reference.
