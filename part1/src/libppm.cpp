
#include "libppm.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using namespace std;

// Reads one PPM header token, skipping whitespace and comments
// before the token. It consumes the whitespace delimiter after it.
static bool read_token(ifstream& stream, string& token)
{
    token.clear();

    while (true) {
        int c = stream.peek();

        if (c == EOF) {
            return false;
        }

        if (isspace(static_cast<unsigned char>(c))) {
            stream.get();
            continue;
        }

        if (c == '#') {
            string ignored;
            getline(stream, ignored);
            continue;
        }

        break;
    }

    while (true) {
        int c = stream.peek();

        if (c == EOF) {
            break;
        }

        if (isspace(static_cast<unsigned char>(c))) {
            stream.get();

            // Treat Windows-style CRLF as one header delimiter.
            if (c == '\r' && stream.peek() == '\n') {
                stream.get();
            }

            break;
        }

        if (c == '#') {
            break;
        }

        token.push_back(static_cast<char>(stream.get()));
    }

    return !token.empty();
}

struct image_t* read_ppm_file(char* path_to_input_file)
{
    ifstream stream(path_to_input_file, ios::binary);

    if (!stream.is_open()) {
        cerr << "Failed to open input file: "
             << path_to_input_file << '\n';
        return nullptr;
    }

    string magic, width_token, height_token, maxval_token;

    if (!read_token(stream, magic) ||
        !read_token(stream, width_token) ||
        !read_token(stream, height_token) ||
        !read_token(stream, maxval_token)) {
        cerr << "Invalid or incomplete PPM header\n";
        return nullptr;
    }

    if (magic != "P6") {
        cerr << "Unsupported PPM format: expected binary P6\n";
        return nullptr;
    }

    int width, height, maxval;

    try {
        size_t used = 0;

        width = stoi(width_token, &used);
        if (used != width_token.size()) {
            throw invalid_argument("invalid width");
        }

        height = stoi(height_token, &used);
        if (used != height_token.size()) {
            throw invalid_argument("invalid height");
        }

        maxval = stoi(maxval_token, &used);
        if (used != maxval_token.size()) {
            throw invalid_argument("invalid maxval");
        }
    } catch (...) {
        cerr << "Invalid numeric value in PPM header\n";
        return nullptr;
    }

    if (width <= 0 || height <= 0 || maxval != 255) {
        cerr << "Expected positive dimensions and maxval 255\n";
        return nullptr;
    }

    if (static_cast<size_t>(width) >
        numeric_limits<size_t>::max() / static_cast<size_t>(height) / 3) {
        cerr << "Image dimensions are too large\n";
        return nullptr;
    }

    image_t* image = new image_t{};
    image->width = width;
    image->height = height;
    image->image_pixels = new uint8_t**[height]();

    for (int y = 0; y < height; ++y) {
        image->image_pixels[y] = new uint8_t*[width]();

        for (int x = 0; x < width; ++x) {
            image->image_pixels[y][x] = new uint8_t[3];

            for (int k = 0; k < 3; ++k) {
                char byte;

                if (!stream.get(byte)) {
                    cerr << "Incomplete PPM pixel data\n";

                    for (int row = 0; row <= y; ++row) {
                        int limit = (row == y) ? x + 1 : width;

                        for (int col = 0; col < limit; ++col) {
                            delete[] image->image_pixels[row][col];
                        }

                        delete[] image->image_pixels[row];
                    }

                    delete[] image->image_pixels;
                    delete image;
                    return nullptr;
                }

                image->image_pixels[y][x][k] =
                    static_cast<uint8_t>(
                        static_cast<unsigned char>(byte));
            }
        }
    }

    return image;
}

void write_ppm_file(char* path_to_output_file, image_t* image)
{
    if (image == nullptr) {
        cerr << "Cannot write a null image\n";
        return;
    }

    ofstream stream(path_to_output_file, ios::binary);

    if (!stream.is_open()) {
        cerr << "Failed to open output file: "
             << path_to_output_file << '\n';
        return;
    }

    stream << "P6\n"
           << image->width << ' '
           << image->height << "\n255\n";

    for (int y = 0; y < image->height; ++y) {
        for (int x = 0; x < image->width; ++x) {
            for (int k = 0; k < 3; ++k) {
                stream.put(
                    static_cast<char>(image->image_pixels[y][x][k]));
            }
        }
    }

    if (!stream) {
        cerr << "Error while writing output image\n";
    }
}
