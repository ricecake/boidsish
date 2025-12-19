BUILD_DIR = build
CONFIG = Release

.PHONY: all clean format run

all:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)
	@cmake --build $(BUILD_DIR) --parallel

# Bridges the wrapper to the CMake 'format' target we created earlier
format:
	@cmake -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR) --target format

# Runs a specific example (e.g., 'make run X=boid_sim')
run: all
	@./$(BUILD_DIR)/$(X)

clean:
	rm -rf $(BUILD_DIR)