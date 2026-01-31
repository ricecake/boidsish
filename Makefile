BUILD_DIR = build
CONFIG = Release
# CONFIG = RelWithDebInfo

.PHONY: all clean format run clean-build

all:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)
	@cmake --build $(BUILD_DIR) --parallel

# Bridges the wrapper to the CMake 'format' target we created earlier
format:
	@cmake -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR) --target format

check:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)
	@cmake --build $(BUILD_DIR) --target check

test: all
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Runs a specific example (e.g., 'make run X=boid_sim')
run: all
	@./$(BUILD_DIR)/$(X)

packages:
	sudo apt install libassimp-dev libassimp5 assimp-utils libgl1-mesa-dev libglfw3-dev libglew-dev libglm-dev xvfb imagemagick libgtest-dev

clean-build:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)
	@cmake --build $(BUILD_DIR) --parallel --clean-first

clean:
	rm -rf $(BUILD_DIR)

clean-build:
	@cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)
	@cmake --build $(BUILD_DIR) --parallel --clean-first