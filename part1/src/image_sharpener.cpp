#include <iostream>
#include "libppm.h"
#include <cstdint>
#include <chrono>

using namespace std;

struct image_t* S1_smoothen(struct image_t *input_image)
{
	// TODO
	// remember to allocate space for smoothened_image. See read_ppm_file() in libppm.c for some help.
	struct image_t* smoothened_image = (struct image_t*)malloc(sizeof(struct image_t));
	smoothened_image->width = input_image->width;
	smoothened_image->height = input_image->height;
	smoothened_image->image_pixels = (uint8_t***)malloc(smoothened_image->height * sizeof(uint8_t**));
	for (int y = 0; y < smoothened_image->height; ++y) {
        smoothened_image->image_pixels[y] = (uint8_t**)malloc(smoothened_image->width * sizeof(uint8_t*));
        for (int x = 0; x < smoothened_image->width; ++x) {
            smoothened_image->image_pixels[y][x] = (uint8_t*)malloc(3 * sizeof(uint8_t));
        }
    }
	for(int y = 0; y < smoothened_image->height; y++){
		for(int x = 0; x < smoothened_image->width; x++){
			for(int k = 0; k < 3; k++){
				int sum = 0;
				int count = 0;
				for(int ky = -1; ky <= 1; ky++){
					for(int kx = -1; kx <= 1; kx++){
						int ny = y + ky;
						int nx = x + kx;
						if(ny >= 0 && ny < input_image->height && nx >= 0 && nx < input_image->width){
							sum += input_image->image_pixels[ny][nx][k];
							count++;
						}
					}
				}

				smoothened_image->image_pixels[y][x][k] = (uint8_t)(sum/count);
			}
		}
	}
	return smoothened_image;
}

struct image_t* S2_find_details(struct image_t *input_image, struct image_t *smoothened_image)
{
	struct image_t* detailed_image = (struct image_t*)malloc(sizeof(struct image_t));
	detailed_image->width = input_image->width;
	detailed_image->height = input_image->height;
	detailed_image->image_pixels = (uint8_t***)malloc(smoothened_image->height * sizeof(uint8_t**));
	for (int y = 0; y < smoothened_image->height; ++y) {
        detailed_image->image_pixels[y] = (uint8_t**)malloc(smoothened_image->width * sizeof(uint8_t*));
        for (int x = 0; x < smoothened_image->width; ++x) {
            detailed_image->image_pixels[y][x] = (uint8_t*)malloc(3 * sizeof(uint8_t));
        }
    }
	for(int y = 0; y < detailed_image->height; y++){
		for(int x = 0; x < detailed_image->width; x++){
			for(int k = 0; k < 3; k++){
				int diff = (int)input_image->image_pixels[y][x][k] - (int)smoothened_image->image_pixels[y][x][k];
                if (diff < 0) {
                    diff = 0;
                }
                detailed_image->image_pixels[y][x][k] = (uint8_t)diff;
			}
		}
	}
	// TODO
	return detailed_image;
}

struct image_t* S3_sharpen(struct image_t *input_image, struct image_t *details_image){
    if (input_image == nullptr || details_image == nullptr) {
        return nullptr;
    }
    struct image_t* sharpened_image = (struct image_t*)malloc(sizeof(struct image_t));
    sharpened_image->width = input_image->width;
    sharpened_image->height = input_image->height;
    sharpened_image->image_pixels = (uint8_t***)malloc(sharpened_image->height * sizeof(uint8_t**));
    for (int y = 0; y < sharpened_image->height; ++y) {
        sharpened_image->image_pixels[y] = (uint8_t**)malloc(sharpened_image->width * sizeof(uint8_t*));
        for (int x = 0; x < sharpened_image->width; ++x) {
            sharpened_image->image_pixels[y][x] = (uint8_t*)malloc(3 * sizeof(uint8_t));
        }
    }
    for (int y = 0; y < sharpened_image->height; ++y) {
        for (int x = 0; x < sharpened_image->width; ++x) {
            for (int k = 0; k < 3; ++k) {
                int sum = (int)input_image->image_pixels[y][x][k] + (int)details_image->image_pixels[y][x][k];
                if (sum > 255) {
                    sum = 255;
                }
                sharpened_image->image_pixels[y][x][k] = (uint8_t)sum;
            }
        }
    }
    return sharpened_image;
}

int main(int argc, char **argv)
{
    if(argc != 3)
    {
        cout << "usage: ./a.out <path-to-original-image> <path-to-transformed-image>\n\n";
        exit(0);
    }

    using namespace chrono;

    // ---------------- File Read ----------------
    auto start = high_resolution_clock::now();

    struct image_t *input_image = read_ppm_file(argv[1]);

    if (input_image == nullptr) {
        cerr << "Could not read input image.\n";
        return 1;
    }

    auto end = high_resolution_clock::now();

    auto read_time = duration_cast<microseconds>(end - start).count();


    // ---------------- S1 ----------------
    start = high_resolution_clock::now();

    struct image_t *smoothened_image = S1_smoothen(input_image);

    end = high_resolution_clock::now();

    auto s1_time = duration_cast<microseconds>(end - start).count();


    // ---------------- S2 ----------------
    start = high_resolution_clock::now();

    struct image_t *details_image =
        S2_find_details(input_image, smoothened_image);

    end = high_resolution_clock::now();

    auto s2_time = duration_cast<microseconds>(end - start).count();


    // ---------------- S3 ----------------
    start = high_resolution_clock::now();

    struct image_t *sharpened_image =
        S3_sharpen(input_image, details_image);

    end = high_resolution_clock::now();

    auto s3_time = duration_cast<microseconds>(end - start).count();


    // ---------------- File Write ----------------
    start = high_resolution_clock::now();

    write_ppm_file(argv[2], sharpened_image);

    end = high_resolution_clock::now();

    auto write_time = duration_cast<microseconds>(end - start).count();


    // ---------------- Print Results ----------------
    cout << "File Read  : " << read_time << " microseconds\n";
    cout << "S1         : " << s1_time << " microseconds\n";
    cout << "S2         : " << s2_time << " microseconds\n";
    cout << "S3         : " << s3_time << " microseconds\n";
    cout << "File Write : " << write_time << " microseconds\n";

    return 0;
}
