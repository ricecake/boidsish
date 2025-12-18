# Root Makefile wrapper
BUILD_DIR = build
CONFIG = Release

all:
	cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)
	cmake --build $(BUILD_DIR) --parallel

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean