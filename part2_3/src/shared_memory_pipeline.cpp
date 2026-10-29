#include "../../part1/src/libppm.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using Clock = std::chrono::steady_clock;

struct PixelMessage {
    uint64_t sequence;
    uint8_t original[3];
    uint8_t processed[3];
    bool done;
};

struct SharedBuffer {
    sem_t empty;
    sem_t full;
    PixelMessage message;
};

struct SharedPipeline {
    SharedBuffer buffer1;
    SharedBuffer buffer2;
};

static bool semaphore_wait(sem_t* semaphore) {
    while (sem_wait(semaphore) == -1) {
        if (errno != EINTR)
            return false;
    }
    return true;
}

static bool send_message(SharedBuffer* buffer,
                         const PixelMessage& message) {
    if (!semaphore_wait(&buffer->empty))
        return false;

    buffer->message = message;

    if (sem_post(&buffer->full) == -1)
        return false;

    return true;
}

static bool receive_message(SharedBuffer* buffer,
                            PixelMessage& message) {
    if (!semaphore_wait(&buffer->full))
        return false;

    message = buffer->message;

    if (sem_post(&buffer->empty) == -1)
        return false;

    return true;
}

static uint8_t clamp(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

static std::array<uint8_t, 3> get_pixel(
    image_t* image, int x, int y) {

    std::array<uint8_t, 3> pixel{};

    for (int c = 0; c < 3; ++c)
        pixel[c] = image->image_pixels[y][x][c];

    return pixel;
}

static std::array<uint8_t, 3> smooth_pixel(
    image_t* image, int x, int y) {

    int sum[3] = {0, 0, 0};
    int count = 0;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx;
            int ny = y + dy;

            if (nx < 0 || nx >= image->width ||
                ny < 0 || ny >= image->height)
                continue;

            for (int c = 0; c < 3; ++c)
                sum[c] += image->image_pixels[ny][nx][c];

            ++count;
        }
    }

    std::array<uint8_t, 3> result{};

    for (int c = 0; c < 3; ++c)
        result[c] = static_cast<uint8_t>(sum[c] / count);

    return result;
}

// Process 1: Smoothing
static int stage1(image_t* image, SharedBuffer* output,
                  int variant, int iterations) {
    uint64_t sequence = 0;

    for (int run = 1; run <= iterations; ++run) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                auto original = get_pixel(image, x, y);
                auto smooth = smooth_pixel(image, x, y);

                // Variant 2 sends every 100th S1 result.
                if (variant == 2 && run % 100 != 0)
                    continue;

                PixelMessage message{};
                message.sequence = sequence++;
                message.done = false;

                for (int c = 0; c < 3; ++c) {
                    message.original[c] = original[c];
                    message.processed[c] = smooth[c];
                }

                if (!send_message(output, message)) {
                    std::cerr << "S1 failed to send a message\n";
                    return 1;
                }
            }
        }
    }

    PixelMessage end_message{};
    end_message.done = true;

    if (!send_message(output, end_message)) {
        std::cerr << "S1 failed to send termination message\n";
        return 1;
    }

    return 0;
}

// Process 2: Detail extraction
static int stage2(SharedBuffer* input, SharedBuffer* output,
                  int variant) {
    PixelMessage message{};
    uint64_t expected_sequence = 0;

    while (true) {
        if (!receive_message(input, message)) {
            std::cerr << "S2 failed to receive a message\n";
            return 1;
        }

        if (message.done)
            break;

        if (message.sequence != expected_sequence++) {
            std::cerr << "S2 sequence error\n";
            return 1;
        }

        for (int c = 0; c < 3; ++c) {
            int detail = std::max(
                0,
                static_cast<int>(message.original[c]) -
                static_cast<int>(message.processed[c]));

            int repetitions = (variant == 2) ? 100 : 1;

            for (int r = 1; r < repetitions; ++r) {
                detail = std::max(
                    0,
                    static_cast<int>(message.original[c]) -
                    static_cast<int>(message.processed[c]));
            }

            message.processed[c] = clamp(detail);
        }

        if (!send_message(output, message)) {
            std::cerr << "S2 failed to send a message\n";
            return 1;
        }
    }

    PixelMessage end_message{};
    end_message.done = true;

    if (!send_message(output, end_message)) {
        std::cerr << "S2 failed to send termination message\n";
        return 1;
    }

    return 0;
}

// Process 3: Sharpening and output
static int stage3(SharedBuffer* input, image_t* image,
                  const char* output_path, int variant,
                  size_t pixel_count) {
    std::vector<std::array<uint8_t, 3>> output(pixel_count);

    PixelMessage message{};
    uint64_t expected_sequence = 0;

    auto processing_start = Clock::now();

    while (true) {
        if (!receive_message(input, message)) {
            std::cerr << "S3 failed to receive a message\n";
            return 1;
        }

        if (message.done)
            break;

        if (message.sequence != expected_sequence++) {
            std::cerr << "S3 sequence error\n";
            return 1;
        }

        size_t index = message.sequence % pixel_count;

        for (int c = 0; c < 3; ++c) {
            int sharpened =
                static_cast<int>(message.original[c]) +
                static_cast<int>(message.processed[c]);

            int repetitions = (variant == 2) ? 100 : 1;

            for (int r = 1; r < repetitions; ++r) {
                sharpened =
                    static_cast<int>(message.original[c]) +
                    static_cast<int>(message.processed[c]);
            }

            output[index][c] = clamp(sharpened);
        }
    }

    auto processing_end = Clock::now();

    for (size_t i = 0; i < pixel_count; ++i) {
        int y = static_cast<int>(i / image->width);
        int x = static_cast<int>(i % image->width);

        for (int c = 0; c < 3; ++c)
            image->image_pixels[y][x][c] = output[i][c];
    }

    auto write_start = Clock::now();

    write_ppm_file(const_cast<char*>(output_path), image);

    auto write_end = Clock::now();

    double processing_ms =
        std::chrono::duration<double, std::milli>(
            processing_end - processing_start).count();

    double write_ms =
        std::chrono::duration<double, std::milli>(
            write_end - write_start).count();

    std::cout << "S3 processing time: "
              << processing_ms << " ms\n"
              << "File write time: "
              << write_ms << " ms\n"
              << std::flush;

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr
            << "Usage: " << argv[0]
            << " <input.ppm> <output.ppm> <variant:1|2> [iterations]\n";
        return 1;
    }

    int variant = std::atoi(argv[3]);

    if (variant != 1 && variant != 2) {
        std::cerr << "Variant must be 1 or 2\n";
        return 1;
    }

    int iterations =
        (variant == 2) ? 1000 :
        (argc >= 5 ? std::atoi(argv[4]) : 1000);

    if (iterations <= 0) {
        std::cerr << "Iterations must be positive\n";
        return 1;
    }

    image_t* image = read_ppm_file(argv[1]);

    if (!image) {
        std::cerr << "Could not read input image\n";
        return 1;
    }

    size_t pixel_count =
        static_cast<size_t>(image->width) * image->height;

    auto* shared = static_cast<SharedPipeline*>(
        mmap(nullptr, sizeof(SharedPipeline),
             PROT_READ | PROT_WRITE,
             MAP_SHARED | MAP_ANONYMOUS, -1, 0));

    if (shared == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    if (sem_init(&shared->buffer1.empty, 1, 1) == -1 ||
        sem_init(&shared->buffer1.full, 1, 0) == -1 ||
        sem_init(&shared->buffer2.empty, 1, 1) == -1 ||
        sem_init(&shared->buffer2.full, 1, 0) == -1) {
        perror("sem_init");
        return 1;
    }

    auto start = Clock::now();

    pid_t s1 = fork();

    if (s1 == 0) {
        int result = stage1(
            image, &shared->buffer1, variant, iterations);
        _exit(result);
    }

    if (s1 < 0) {
        perror("fork S1");
        return 1;
    }

    pid_t s2 = fork();

    if (s2 == 0) {
        int result = stage2(
            &shared->buffer1, &shared->buffer2, variant);
        _exit(result);
    }

    if (s2 < 0) {
        perror("fork S2");
        return 1;
    }

    pid_t s3 = fork();

    if (s3 == 0) {
        int result = stage3(
            &shared->buffer2, image, argv[2],
            variant, pixel_count);
        _exit(result);
    }

    if (s3 < 0) {
        perror("fork S3");
        return 1;
    }

    int status1 = 0;
    int status2 = 0;
    int status3 = 0;

    waitpid(s1, &status1, 0);
    waitpid(s2, &status2, 0);
    waitpid(s3, &status3, 0);

    auto end = Clock::now();

    double total_ms =
        std::chrono::duration<double, std::milli>(
            end - start).count();

    std::cout << "Total pipeline time: "
              << total_ms << " ms\n";

    sem_destroy(&shared->buffer1.empty);
    sem_destroy(&shared->buffer1.full);
    sem_destroy(&shared->buffer2.empty);
    sem_destroy(&shared->buffer2.full);

    munmap(shared, sizeof(SharedPipeline));

    if (!WIFEXITED(status1) || WEXITSTATUS(status1) != 0 ||
        !WIFEXITED(status2) || WEXITSTATUS(status2) != 0 ||
        !WIFEXITED(status3) || WEXITSTATUS(status3) != 0) {
        std::cerr << "One or more pipeline processes failed\n";
        return 1;
    }

    return 0;
}
