
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "../../part1/src/libppm.h"

using Pixel = std::uint8_t;
using ImageData = std::vector<Pixel>;


// ------------------------------------------------------------
// Image helper functions
// ------------------------------------------------------------

// Convert libppm's 3D pixel structure into a flat vector.
//
// image_pixels[y][x][channel]
//          |
//          v
// data[(y * width + x) * 3 + channel]
ImageData flatten_image(const image_t* image)
{
    const int width = image->width;
    const int height = image->height;

    const std::size_t image_size =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height) * 3;

    ImageData data(image_size, 0);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int channel = 0; channel < 3; ++channel)
            {
                const std::size_t index =
                    (static_cast<std::size_t>(y) * width + x) * 3
                    + channel;

                data[index] =
                    image->image_pixels[y][x][channel];
            }
        }
    }

    return data;
}


// Copy a flat vector back into libppm's 3D pixel structure.
void copy_to_image(const ImageData& data, image_t* image)
{
    const int width = image->width;
    const int height = image->height;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            for (int channel = 0; channel < 3; ++channel)
            {
                const std::size_t index =
                    (static_cast<std::size_t>(y) * width + x) * 3
                    + channel;

                image->image_pixels[y][x][channel] =
                    data[index];
            }
        }
    }
}


// Release memory allocated by read_ppm_file().
void destroy_image(image_t* image)
{
    if (image == nullptr)
    {
        return;
    }

    if (image->image_pixels != nullptr)
    {
        for (int y = 0; y < image->height; ++y)
        {
            if (image->image_pixels[y] != nullptr)
            {
                for (int x = 0; x < image->width; ++x)
                {
                    delete[] image->image_pixels[y][x];
                }

                delete[] image->image_pixels[y];
            }
        }

        delete[] image->image_pixels;
    }

    delete image;
}


// ------------------------------------------------------------
// Stage 1: Smooth
// ------------------------------------------------------------

// Apply a 3x3 smoothing filter to one pixel.
//
// Only valid neighbors are considered.
// Each RGB channel is averaged independently.
void smooth_pixel(
    const ImageData& original,
    ImageData& smoothened,
    int width,
    int height,
    std::size_t pixel_index,
    std::size_t output_offset = 0
)
{
    const int x =
        static_cast<int>(pixel_index % static_cast<std::size_t>(width));

    const int y =
        static_cast<int>(pixel_index / static_cast<std::size_t>(width));

    for (int channel = 0; channel < 3; ++channel)
    {
        int sum = 0;
        int count = 0;

        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int neighbor_x = x + dx;
                const int neighbor_y = y + dy;

                if (neighbor_x < 0 || neighbor_x >= width ||
                    neighbor_y < 0 || neighbor_y >= height)
                {
                    continue;
                }

                const std::size_t neighbor_index =
                    (static_cast<std::size_t>(neighbor_y) * width
                     + neighbor_x) * 3
                    + channel;

                sum += original[neighbor_index];
                ++count;
            }
        }

        const std::size_t output_index =
            output_offset + pixel_index * 3 + channel;

        smoothened[output_index] =
            static_cast<Pixel>(sum / count);
    }
}


// ------------------------------------------------------------
// Stage 2: Details
// ------------------------------------------------------------

// Details = max(0, original - smoothened)
void detail_pixel(
    const ImageData& original,
    const ImageData& smoothened,
    ImageData& details,
    std::size_t pixel_index,
    std::size_t smooth_offset = 0,
    std::size_t details_offset = 0
)
{
    for (int channel = 0; channel < 3; ++channel)
    {
        const std::size_t original_index =
            pixel_index * 3 + channel;

        const std::size_t smooth_index =
            smooth_offset + pixel_index * 3 + channel;

        const std::size_t details_index =
            details_offset + pixel_index * 3 + channel;

        const int difference =
            static_cast<int>(original[original_index]) -
            static_cast<int>(smoothened[smooth_index]);

        details[details_index] =
            static_cast<Pixel>(difference > 0 ? difference : 0);
    }
}


// ------------------------------------------------------------
// Stage 3: Sharpen
// ------------------------------------------------------------

// Sharpened = min(255, original + details)
void sharpen_pixel(
    const ImageData& original,
    const ImageData& details,
    ImageData& sharpened,
    std::size_t pixel_index,
    std::size_t details_offset = 0
)
{
    for (int channel = 0; channel < 3; ++channel)
    {
        const std::size_t index =
            pixel_index * 3 + channel;

        const std::size_t details_index =
            details_offset + pixel_index * 3 + channel;

        const int value =
            static_cast<int>(original[index]) +
            static_cast<int>(details[details_index]);

        sharpened[index] =
            static_cast<Pixel>(value > 255 ? 255 : value);
    }
}


// ------------------------------------------------------------
// Synchronization helper
// ------------------------------------------------------------

// Wait until an atomic counter reaches a required value.
void wait_until_ready(
    const std::atomic<std::size_t>& counter,
    std::size_t required_value
)
{
    while (counter.load(std::memory_order_acquire) < required_value)
    {
        std::this_thread::yield();
    }
}


// ------------------------------------------------------------
// Part II.1 - Variant 1
// ------------------------------------------------------------

// Three threads execute S1, S2, and S3.
//
// Each stage processes pixels in sequence.
// Two buffer slots are used so that a stage can work on one
// slot while another stage works on the other slot.
//
// Atomic counters coordinate buffer production and consumption.
void run_variant1(
    const ImageData& original,
    ImageData& output,
    int width,
    int height,
    std::size_t iterations
)
{
    const std::size_t pixel_count =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height);

    const std::size_t image_size =
        pixel_count * 3;

    const std::size_t total_pixels =
        iterations * pixel_count;

    // Two slots for intermediate data.
    ImageData smooth_buffer(image_size * 2, 0);
    ImageData details_buffer(image_size * 2, 0);

    // Sequence counters.
    std::atomic<std::size_t> s1_produced{0};
    std::atomic<std::size_t> s2_consumed{0};

    std::atomic<std::size_t> s2_produced{0};
    std::atomic<std::size_t> s3_consumed{0};


    // ---------------- S1 Thread ----------------
    std::thread s1_thread(
        [&]()
        {
            for (std::size_t sequence = 0;
                 sequence < total_pixels;
                 ++sequence)
            {
                const std::size_t slot =
                    sequence % 2;

                const std::size_t pixel_index =
                    sequence % pixel_count;

                // Wait until S2 has finished reading this slot
                // before S1 reuses it.
                if (sequence >= 2)
                {
                    wait_until_ready(
                        s2_consumed,
                        sequence - 1
                    );
                }

                smooth_pixel(
                    original,
                    smooth_buffer,
                    width,
                    height,
                    pixel_index,
                    slot * image_size
                );

                // Publish this pixel to S2.
                s1_produced.store(
                    sequence + 1,
                    std::memory_order_release
                );
            }
        }
    );


    // ---------------- S2 Thread ----------------
    std::thread s2_thread(
        [&]()
        {
            for (std::size_t sequence = 0;
                 sequence < total_pixels;
                 ++sequence)
            {
                const std::size_t slot =
                    sequence % 2;

                const std::size_t pixel_index =
                    sequence % pixel_count;

                // Wait until S1 has produced this pixel.
                wait_until_ready(
                    s1_produced,
                    sequence + 1
                );

                // Wait until S3 has finished reading this slot
                // before S2 reuses it.
                if (sequence >= 2)
                {
                    wait_until_ready(
                        s3_consumed,
                        sequence - 1
                    );
                }

                detail_pixel(
                    original,
                    smooth_buffer,
                    details_buffer,
                    pixel_index,
                    slot * image_size,
                    slot * image_size
                );

                // S2 has finished reading S1's slot.
                s2_consumed.store(
                    sequence + 1,
                    std::memory_order_release
                );

                // Publish details to S3.
                s2_produced.store(
                    sequence + 1,
                    std::memory_order_release
                );
            }
        }
    );


    // ---------------- S3 Thread ----------------
    std::thread s3_thread(
        [&]()
        {
            for (std::size_t sequence = 0;
                 sequence < total_pixels;
                 ++sequence)
            {
                const std::size_t slot =
                    sequence % 2;

                const std::size_t pixel_index =
                    sequence % pixel_count;

                // Wait until S2 has produced this pixel.
                wait_until_ready(
                    s2_produced,
                    sequence + 1
                );

                sharpen_pixel(
                    original,
                    details_buffer,
                    output,
                    pixel_index,
                    slot * image_size
                );

                // S3 has finished reading this slot.
                s3_consumed.store(
                    sequence + 1,
                    std::memory_order_release
                );
            }
        }
    );


    // Wait for all three stages to finish.
    s1_thread.join();
    s2_thread.join();
    s3_thread.join();
}


// ------------------------------------------------------------
// Part II.1 - Variant 2
// ------------------------------------------------------------

// Variant 2 uses the fixed repetition counts:
//
// S1: 1000 complete image runs.
// Every 100th S1 result is passed to S2.
//
// S2: For each received S1 image, perform 100 complete runs.
// The 100th result is passed to S3.
//
// S3: For each received S2 image, perform 100 complete runs.
//
// The output is the result of the final S3 run.
void run_variant2(
    const ImageData& original,
    ImageData& output,
    int width,
    int height
)
{
    constexpr std::size_t S1_RUNS = 1000;
    constexpr std::size_t S1_HANDOFF_INTERVAL = 100;
    constexpr std::size_t S2_RUNS_PER_HANDOFF = 100;
    constexpr std::size_t S3_RUNS_PER_HANDOFF = 100;

    constexpr std::size_t HANDOFF_COUNT =
        S1_RUNS / S1_HANDOFF_INTERVAL;

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height);

    const std::size_t image_size =
        pixel_count * 3;


    // Mailboxes between stages.
    ImageData s1_mailbox(image_size, 0);
    ImageData s2_mailbox(image_size, 0);

    // Work buffers used by S2 and S3.
    ImageData s1_work(image_size, 0);
    ImageData s2_work(image_size, 0);


    // Atomic handoff counters.
    std::atomic<std::size_t> s1_published{0};
    std::atomic<std::size_t> s1_consumed{0};

    std::atomic<std::size_t> s2_published{0};
    std::atomic<std::size_t> s2_consumed{0};


    // ---------------- S1 Thread ----------------
    std::thread s1_thread(
        [&]()
        {
            for (std::size_t run = 1;
                 run <= S1_RUNS;
                 ++run)
            {
                // Perform one complete smoothing run.
                for (std::size_t pixel_index = 0;
                     pixel_index < pixel_count;
                     ++pixel_index)
                {
                    smooth_pixel(
                        original,
                        s1_work,
                        width,
                        height,
                        pixel_index
                    );
                }

                // Every 100th S1 result is handed to S2.
                if (run % S1_HANDOFF_INTERVAL == 0)
                {
                    const std::size_t handoff =
                        run / S1_HANDOFF_INTERVAL;

                    // Wait until S2 has consumed the previous
                    // mailbox contents.
                    if (handoff > 1)
                    {
                        wait_until_ready(
                            s1_consumed,
                            handoff - 1
                        );
                    }

                    s1_mailbox = s1_work;

                    s1_published.store(
                        handoff,
                        std::memory_order_release
                    );
                }
            }
        }
    );


    // ---------------- S2 Thread ----------------
    std::thread s2_thread(
        [&]()
        {
            for (std::size_t handoff = 1;
                 handoff <= HANDOFF_COUNT;
                 ++handoff)
            {
                // Wait for S1's next handoff.
                wait_until_ready(
                    s1_published,
                    handoff
                );

                // Perform 100 complete detail runs.
                for (std::size_t run = 0;
                     run < S2_RUNS_PER_HANDOFF;
                     ++run)
                {
                    for (std::size_t pixel_index = 0;
                         pixel_index < pixel_count;
                         ++pixel_index)
                    {
                        detail_pixel(
                            original,
                            s1_mailbox,
                            s2_work,
                            pixel_index
                        );
                    }
                }

                // Wait until S3 has consumed the previous
                // S2 mailbox contents.
                if (handoff > 1)
                {
                    wait_until_ready(
                        s2_consumed,
                        handoff - 1
                    );
                }

                s2_mailbox = s2_work;

                s2_published.store(
                    handoff,
                    std::memory_order_release
                );

                // S2 has finished reading S1's mailbox.
                s1_consumed.store(
                    handoff,
                    std::memory_order_release
                );
            }
        }
    );


    // ---------------- S3 Thread ----------------
    std::thread s3_thread(
        [&]()
        {
            for (std::size_t handoff = 1;
                 handoff <= HANDOFF_COUNT;
                 ++handoff)
            {
                // Wait for S2's next handoff.
                wait_until_ready(
                    s2_published,
                    handoff
                );

                // Perform 100 complete sharpening runs.
                for (std::size_t run = 0;
                     run < S3_RUNS_PER_HANDOFF;
                     ++run)
                {
                    for (std::size_t pixel_index = 0;
                         pixel_index < pixel_count;
                         ++pixel_index)
                    {
                        sharpen_pixel(
                            original,
                            s2_mailbox,
                            output,
                            pixel_index
                        );
                    }
                }

                // S3 has finished reading S2's mailbox.
                s2_consumed.store(
                    handoff,
                    std::memory_order_release
                );
            }
        }
    );


    // Wait for all three stages to finish.
    s1_thread.join();
    s2_thread.join();
    s3_thread.join();
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char* argv[])
{
    if (argc < 4 || argc > 5)
    {
        std::cerr
            << "Usage: " << argv[0]
            << " <input.ppm> <output.ppm> <variant> [iterations]\n"
            << "Variant 1: optional iteration count (default 1000)\n"
            << "Variant 2: uses fixed repetition counts\n";

        return 1;
    }


    const int variant =
        std::atoi(argv[3]);

    if (variant != 1 && variant != 2)
    {
        std::cerr
            << "Invalid variant. Use 1 or 2.\n";

        return 1;
    }


    std::size_t iterations = 1000;

    if (variant == 1 && argc == 5)
    {
        char* end_pointer = nullptr;

        const long parsed =
            std::strtol(argv[4], &end_pointer, 10);

        if (end_pointer == argv[4] ||
            *end_pointer != '\0' ||
            parsed <= 0)
        {
            std::cerr
                << "Invalid iteration count. "
                << "It must be a positive integer.\n";

            return 1;
        }

        iterations =
            static_cast<std::size_t>(parsed);
    }
    else if (variant == 2 && argc == 5)
    {
        std::cerr
            << "Variant 2 uses the fixed repetition counts; "
            << "do not provide an iteration argument.\n";

        return 1;
    }


    // Read the input image.
    image_t* input_image =
        read_ppm_file(argv[1]);

    if (input_image == nullptr)
    {
        std::cerr
            << "Could not read input image: "
            << argv[1] << '\n';

        return 1;
    }


    const int width =
        input_image->width;

    const int height =
        input_image->height;

    if (width <= 0 || height <= 0)
    {
        std::cerr
            << "Invalid image dimensions\n";

        destroy_image(input_image);

        return 1;
    }


    // Convert the input image into a flat vector.
    ImageData original =
        flatten_image(input_image);

    const std::size_t image_size =
        original.size();

    ImageData output(image_size, 0);


    // --------------------------------------------------------
    // Measure processing time.
    // Input reading and output writing are excluded.
    // --------------------------------------------------------

    const auto processing_start =
        std::chrono::steady_clock::now();


    if (variant == 1)
    {
        run_variant1(
            original,
            output,
            width,
            height,
            iterations
        );
    }
    else
    {
        run_variant2(
            original,
            output,
            width,
            height
        );
    }


    const auto processing_end =
        std::chrono::steady_clock::now();


    // Copy the result back into libppm's image structure.
    copy_to_image(
        output,
        input_image
    );


    // --------------------------------------------------------
    // Measure output writing time separately.
    // --------------------------------------------------------

    const auto write_start =
        std::chrono::steady_clock::now();

    write_ppm_file(
        argv[2],
        input_image
    );

    const auto write_end =
        std::chrono::steady_clock::now();


    const double processing_ms =
        std::chrono::duration<double, std::milli>(
            processing_end - processing_start
        ).count();

    const double write_ms =
        std::chrono::duration<double, std::milli>(
            write_end - write_start
        ).count();


    // --------------------------------------------------------
    // Print results.
    // --------------------------------------------------------

    std::cout
        << "Variant: " << variant << '\n';

    if (variant == 1)
    {
        std::cout
            << "Iterations: " << iterations << '\n';
    }
    else
    {
        std::cout
            << "S1 runs: 1000\n"
            << "S1 handoffs: 10\n"
            << "S2 runs per handoff: 100\n"
            << "S3 runs per handoff: 100\n";
    }

    std::cout
        << "Image dimensions: "
        << width << " x " << height << '\n';

    std::cout
        << "Processing time: "
        << processing_ms << " ms\n";

    std::cout
        << "Output writing time: "
        << write_ms << " ms\n";


    // Release the input image.
    destroy_image(input_image);

    return 0;
}
