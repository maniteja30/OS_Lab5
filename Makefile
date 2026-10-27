CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread

PART1_SRC = part1/src/image_sharpener.cpp part1/src/libppm.cpp
PART1_BIN = part1/sharpen

PART2_1_SRC = part2_1/src/thread_pipeline.cpp part1/src/libppm.cpp
PART2_1_BIN = part2_1/thread_pipeline

.PHONY: all part1 part2_1_compile part2_1_variant1 part2_1_variant2 clean

all: part1


# ------------------------------------------------------------
# Part I: Sequential baseline
# ------------------------------------------------------------

part1: $(PART1_BIN)
	./$(PART1_BIN) images/1.ppm output_part1.ppm

$(PART1_BIN): $(PART1_SRC) part1/src/libppm.h
	$(CXX) $(CXXFLAGS) $(PART1_SRC) -o $(PART1_BIN)


# ------------------------------------------------------------
# Part II.1: Three threads synchronized using atomics
# ------------------------------------------------------------

part2_1_compile: $(PART2_1_BIN)

$(PART2_1_BIN): $(PART2_1_SRC) part1/src/libppm.h
	$(CXX) $(CXXFLAGS) $(PART2_1_SRC) -o $(PART2_1_BIN)


# Variant 1: 1000 iterations
part2_1_variant1: $(PART2_1_BIN)
	./$(PART2_1_BIN) images/1.ppm output_part2_2_variant1.ppm 1


# Variant 2: Revised repetition counts
part2_1_variant2: $(PART2_1_BIN)
	./$(PART2_1_BIN) images/1.ppm output_part2_2_variant2.ppm 2


# ------------------------------------------------------------
# Clean generated files
# ------------------------------------------------------------

clean:
	rm -f $(PART1_BIN) $(PART2_1_BIN)
	rm -f output_part1.ppm output_part2_2_variant*.ppm