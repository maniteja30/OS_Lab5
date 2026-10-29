CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread

PART1_SRC = part1/src/image_sharpener.cpp part1/src/libppm.cpp
PART1_BIN = part1/sharpen

PART2_1_SRC = part2_1/src/thread_pipeline.cpp part1/src/libppm.cpp
PART2_1_BIN = part2_1/thread_pipeline

PART2_2_SRC = part2_2/src/process_pipeline.cpp part1/src/libppm.cpp
PART2_2_BIN = part2_2/process_pipeline

PART2_3_SRC = part2_3/src/shared_memory_pipeline.cpp part1/src/libppm.cpp
PART2_3_BIN = part2_3/shared_memory_pipeline

.PHONY: all part1 \
        part2_1_compile part2_1_variant1 part2_1_variant2 \
        part2_2_compile part2_2_variant1 part2_2_variant2 \
        part2_3_compile part2_3_variant1 part2_3_variant2 \
        clean

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

part2_1_variant1: $(PART2_1_BIN)
	./$(PART2_1_BIN) images/1.ppm output_part2_1_variant1.ppm 1

part2_1_variant2: $(PART2_1_BIN)
	./$(PART2_1_BIN) images/1.ppm output_part2_1_variant2.ppm 2


# ------------------------------------------------------------
# Part II.2: Three processes communicating using pipes
# ------------------------------------------------------------

part2_2_compile: $(PART2_2_BIN)

$(PART2_2_BIN): $(PART2_2_SRC) part1/src/libppm.h
	$(CXX) $(CXXFLAGS) $(PART2_2_SRC) -o $(PART2_2_BIN)

part2_2_variant1: $(PART2_2_BIN)
	./$(PART2_2_BIN) images/1.ppm output_part2_2_variant1.ppm 1

part2_2_variant2: $(PART2_2_BIN)
	./$(PART2_2_BIN) images/1.ppm output_part2_2_variant2.ppm 2


# ------------------------------------------------------------
# Part II.3: Three processes using shared memory and semaphores
# ------------------------------------------------------------

part2_3_compile: $(PART2_3_BIN)

$(PART2_3_BIN): $(PART2_3_SRC) part1/src/libppm.h
	$(CXX) $(CXXFLAGS) $(PART2_3_SRC) -o $(PART2_3_BIN)

part2_3_variant1: $(PART2_3_BIN)
	./$(PART2_3_BIN) images/1.ppm output_part2_3_variant1.ppm 1

part2_3_variant2: $(PART2_3_BIN)
	./$(PART2_3_BIN) images/1.ppm output_part2_3_variant2.ppm 2


# ------------------------------------------------------------
# Clean generated files
# ------------------------------------------------------------

clean:
	rm -f $(PART1_BIN) $(PART2_1_BIN) $(PART2_2_BIN) $(PART2_3_BIN)
	rm -f output_part1.ppm output_part2_1_variant*.ppm
	rm -f output_part2_2_variant*.ppm output_part2_3_variant*.ppm