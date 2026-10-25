
CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

PART1_SRC = part1/src/image_sharpener.cpp part1/src/libppm.cpp
PART1_BIN = part1/sharpen

.PHONY: all part1 clean

all: part1

part1: $(PART1_BIN)

$(PART1_BIN): $(PART1_SRC) part1/src/libppm.h
	$(CXX) $(CXXFLAGS) $(PART1_SRC) -o $(PART1_BIN)

clean:
	rm -f $(PART1_BIN) output_part1.ppm
