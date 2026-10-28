#include "../../part1/src/libppm.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using Clock = std::chrono::steady_clock;

struct PixelMessage {
    uint64_t sequence;
    uint8_t original[3];
    uint8_t processed[3];
};

static bool write_all(int fd, const void* data, size_t size) {
    const char* p = static_cast<const char*>(data);
    while (size > 0) {
        ssize_t n = write(fd, p, size);
        if (n <= 0) return false;
        p += n;
        size -= static_cast<size_t>(n);
    }
    return true;
}

static bool read_all(int fd, void* data, size_t size) {
    char* p = static_cast<char*>(data);
    while (size > 0) {
        ssize_t n = read(fd, p, size);
        if (n == 0) return false;
        if (n < 0) return false;
        p += n;
        size -= static_cast<size_t>(n);
    }
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

static int stage1(image_t* image, int fd, int variant, int iterations) {
    uint64_t sequence = 0;

    for (int run = 1; run <= iterations; ++run) {
        for (int y = 0; y < image->height; ++y) {
            for (int x = 0; x < image->width; ++x) {
                if (variant == 2 && run % 100 != 0) {
                    // Variant 2 only sends every 100th S1 result.
                    (void)smooth_pixel(image, x, y);
                    continue;
                }

                PixelMessage msg{};
                msg.sequence = sequence++;

                auto original = get_pixel(image, x, y);
                auto smooth = smooth_pixel(image, x, y);

                for (int c = 0; c < 3; ++c) {
                    msg.original[c] = original[c];
                    msg.processed[c] = smooth[c];
                }

                if (!write_all(fd, &msg, sizeof(msg)))
                    return 1;
            }
        }
    }

    close(fd);
    return 0;
}

static int stage2(int input_fd, int output_fd, int variant,
                  size_t pixel_count) {
    PixelMessage msg{};
    uint64_t expected = 0;

    while (read_all(input_fd, &msg, sizeof(msg))) {
        if (msg.sequence != expected++) {
            std::cerr << "S2 sequence error\n";
            return 1;
        }

        for (int c = 0; c < 3; ++c) {
            int detail = std::max(
                0, static_cast<int>(msg.original[c]) -
                   static_cast<int>(msg.processed[c]));

            // Variant 2 repeats S2 100 times per received pixel.
            int repetitions = (variant == 2) ? 100 : 1;
            for (int r = 1; r < repetitions; ++r)
                detail = std::max(
                    0, static_cast<int>(msg.original[c]) -
                       static_cast<int>(msg.processed[c]));

            msg.processed[c] = clamp(detail);
        }

        if (!write_all(output_fd, &msg, sizeof(msg)))
            return 1;
    }

    (void)pixel_count;
    close(input_fd);
    close(output_fd);
    return 0;
}

static int stage3(int input_fd, image_t* image, const char* output_path,
                  int variant, size_t pixel_count) {
    std::vector<std::array<uint8_t, 3>> output(pixel_count);
    PixelMessage msg{};
    uint64_t expected = 0;

    auto processing_start = Clock::now();

    while (read_all(input_fd, &msg, sizeof(msg))) {
        if (msg.sequence != expected++) {
            std::cerr << "S3 sequence error\n";
            return 1;
        }

        size_t index = msg.sequence % pixel_count;

        for (int c = 0; c < 3; ++c) {
            int sharpened = static_cast<int>(msg.original[c]) +
                            static_cast<int>(msg.processed[c]);

            // Variant 2 repeats S3 100 times per received pixel.
            int repetitions = (variant == 2) ? 100 : 1;
            for (int r = 1; r < repetitions; ++r)
                sharpened = static_cast<int>(msg.original[c]) +
                            static_cast<int>(msg.processed[c]);

            output[index][c] = clamp(sharpened);
        }
    }

    close(input_fd);

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

    auto processing_ms =
        std::chrono::duration<double, std::milli>(
            processing_end - processing_start).count();

    auto write_ms =
        std::chrono::duration<double, std::milli>(
            write_end - write_start).count();

    std::cout << "S3 processing time: " << processing_ms << " ms\n"
              << "File write time: " << write_ms << " ms\n";

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

    int iterations = (variant == 2) ? 1000 :
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

    int pipe1[2], pipe2[2];

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("pipe");
        return 1;
    }

    auto start = Clock::now();

    pid_t s1 = fork();

    if (s1 == 0) {
        close(pipe1[0]);
        close(pipe2[0]);
        close(pipe2[1]);

        int result = stage1(image, pipe1[1], variant, iterations);
        _exit(result);
    }

    pid_t s2 = fork();

    if (s2 == 0) {
        close(pipe1[1]);
        close(pipe2[0]);

        int result = stage2(pipe1[0], pipe2[1], variant, pixel_count);
        _exit(result);
    }

    pid_t s3 = fork();

    if (s3 == 0) {
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[1]);

        int result = stage3(
            pipe2[0], image, argv[2], variant, pixel_count);
        _exit(result);
    }

    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);

    int status1 = 0, status2 = 0, status3 = 0;
    waitpid(s1, &status1, 0);
    waitpid(s2, &status2, 0);
    waitpid(s3, &status3, 0);

    auto end = Clock::now();

    double total_ms =
        std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Total pipeline time: " << total_ms << " ms\n";

    if (!WIFEXITED(status1) || WEXITSTATUS(status1) != 0 ||
        !WIFEXITED(status2) || WEXITSTATUS(status2) != 0 ||
        !WIFEXITED(status3) || WEXITSTATUS(status3) != 0) {
        std::cerr << "One or more pipeline processes failed\n";
        return 1;
    }

    return 0;
}
