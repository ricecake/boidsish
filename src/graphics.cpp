#include "graphics.h"

#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "dot.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <shader.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "trail.h"

namespace Boidsish {

	// Implementation details hidden from header
	struct Visualizer::VisualizerImpl {
		GLFWwindow*          window;
		int                  width, height;
		Camera               camera;
		ShapeFunction        shape_function;
		std::map<int, std::shared_ptr<Trail>> trails;            // Trail for each dot (using dot ID as key)
		std::map<int, float> trail_last_update; // Track when each trail was last updated for cleanup

		Shader*				 shader;
		Shader*				 grid_shader;
		Shader*				 trail_shader;
		GLuint				 grid_vao;
		glm::mat4			 projection;


		// Mouse and keyboard state
		double last_mouse_x, last_mouse_y;
		bool   first_mouse;
		bool   keys[1024];

		// Auto-camera state
		bool  auto_camera_mode;
		float auto_camera_time;
		float auto_camera_angle;
		float auto_camera_height_offset;
		float auto_camera_distance;

		// Single-object tracking state
		bool single_track_mode;
		int  tracked_dot_index;

		// Timing
		std::chrono::high_resolution_clock::time_point start_time;

		VisualizerImpl(int w, int h, const char* title):
			window(nullptr),
			width(w),
			height(h),
			camera(),
			shape_function(nullptr),
			last_mouse_x(0.0),
			last_mouse_y(0.0),
			first_mouse(true),
			auto_camera_mode(true),
			auto_camera_time(0.0f),
			auto_camera_angle(0.0f),
			auto_camera_height_offset(0.0f),
			auto_camera_distance(10.0f),
			single_track_mode(false),
			tracked_dot_index(0) {
			// Initialize all keys to false
			for (int i = 0; i < 1024; ++i) {
				keys[i] = false;
			}

			start_time = std::chrono::high_resolution_clock::now();

			if (!glfwInit()) {
				throw std::runtime_error("Failed to initialize GLFW");
			}

			// Set error callback to get more info
			glfwSetErrorCallback([](int error, const char* description) {
				std::cerr << "GLFW Error " << error << ": " << description << std::endl;
			});

			// Use modern OpenGL 3.3
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

			window = glfwCreateWindow(width, height, title, nullptr, nullptr);
			if (!window) {
				std::cerr << "Failed to create GLFW window with OpenGL 3.3" << std::endl;
				glfwTerminate();
				throw std::runtime_error("Failed to create GLFW window - check OpenGL drivers");
			}

			glfwMakeContextCurrent(window);

			// Initialize GLEW (optional for basic OpenGL)
			GLenum glew_status = glewInit();
			if (glew_status != GLEW_OK) {
				std::cerr << "GLEW initialization failed: " << glewGetErrorString(glew_status) << std::endl;
				std::cerr << "Continuing without GLEW - basic OpenGL should still work" << std::endl;
			} else {
				std::cout << "GLEW initialized successfully" << std::endl;
			}

			// Set up callbacks
			glfwSetWindowUserPointer(window, this);
			glfwSetKeyCallback(window, KeyCallback);
			glfwSetCursorPosCallback(window, MouseCallback);
			glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

			// Capture cursor
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			// Enable depth testing
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// Check OpenGL version
			const GLubyte* version = glGetString(GL_VERSION);
			std::cout << "OpenGL Version: " << version << std::endl;

			// Set background color (black for holodeck effect)
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			shader = new Shader("shaders/vis.vs", "shaders/frag.fs");
			Shape::shader = shader;
			grid_shader = new Shader("shaders/grid.vs", "shaders/grid.fs");
			trail_shader = new Shader("shaders/trail.vs", "shaders/trail.fs");

			Dot::InitSphereMesh();

			float quad_vertices[] = {
				-1.0f, 0.0f, -1.0f,
				 1.0f, 0.0f, -1.0f,
				 1.0f, 0.0f,  1.0f,
				 1.0f, 0.0f,  1.0f,
				-1.0f, 0.0f,  1.0f,
				-1.0f, 0.0f, -1.0f,
			};

			glGenVertexArrays(1, &grid_vao);
			glBindVertexArray(grid_vao);

			GLuint vbo;
			glGenBuffers(1, &vbo);
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			glBindVertexArray(0);
		}

		~VisualizerImpl() {
			Dot::CleanupSphereMesh();
			delete shader;
			delete grid_shader;
			delete trail_shader;
			glDeleteVertexArrays(1, &grid_vao);
			if (window) {
				glfwDestroyWindow(window);
			}
			glfwTerminate();
		}

		glm::mat4 SetupMatrices() {
			projection = glm::perspective(glm::radians(camera.fov), (float)width / (float)height, 0.1f, 1000.0f);

			glm::vec3 cameraPos = glm::vec3(camera.x, camera.y, camera.z);

			float yaw_rad = glm::radians(camera.yaw);
			float pitch_rad = glm::radians(camera.pitch);

			glm::vec3 front;
			front.x = cos(pitch_rad) * sin(yaw_rad);
			front.y = sin(pitch_rad);
			front.z = -cos(pitch_rad) * cos(yaw_rad);
			front = glm::normalize(front);

			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));

			shader->use();
			shader->setMat4("projection", projection);
			shader->setMat4("view", view);

			return view;
		}

		void RenderGrid(const glm::mat4& view, const glm::mat4& projection) {
			glDisable(GL_DEPTH_TEST);
			grid_shader->use();

			glm::mat4 model = glm::mat4(1.0f);
			model = glm::scale(model, glm::vec3(100.0f));

			grid_shader->setMat4("model", model);
			grid_shader->setMat4("view", view);
			grid_shader->setMat4("projection", projection);
			grid_shader->setFloat("far_plane", 1000.0f);

			glBindVertexArray(grid_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
			glEnable(GL_DEPTH_TEST);
		}

		void ProcessInput(float delta_time) {
			// Skip manual camera controls if auto-camera is active
			if (auto_camera_mode) {
				return;
			}

			float camera_speed = 5.0f * delta_time;

			// Calculate camera direction vectors using standard FPS math
			float yaw_rad = camera.yaw * M_PI / 180.0f;
			float pitch_rad = camera.pitch * M_PI / 180.0f;

			// Forward vector - where the camera is looking
			float forward_x = cos(pitch_rad) * sin(yaw_rad);
			float forward_y = sin(pitch_rad);
			float forward_z = -cos(pitch_rad) * cos(yaw_rad);

			// Right vector - cross product of forward and world up (0,1,0)
			float right_x = -forward_z;
			float right_z = forward_x;

			// Movement
			if (keys[GLFW_KEY_W]) {
				camera.x += forward_x * camera_speed;
				camera.y += forward_y * camera_speed;
				camera.z += forward_z * camera_speed;
			}
			if (keys[GLFW_KEY_S]) {
				camera.x -= forward_x * camera_speed;
				camera.y -= forward_y * camera_speed;
				camera.z -= forward_z * camera_speed;
			}
			if (keys[GLFW_KEY_A]) {
				camera.x -= right_x * camera_speed;
				camera.z -= right_z * camera_speed;
			}
			if (keys[GLFW_KEY_D]) {
				camera.x += right_x * camera_speed;
				camera.z += right_z * camera_speed;
			}
			if (keys[GLFW_KEY_SPACE]) {
				camera.y += camera_speed;
			}
			if (keys[GLFW_KEY_LEFT_SHIFT]) {
				camera.y -= camera_speed;
			}
		}

		void UpdateAutoCamera(float delta_time, const std::vector<std::shared_ptr<Shape>>& shapes) {
			if (!auto_camera_mode || shapes.empty()) {
				return;
			}

			auto_camera_time += delta_time;

			// Calculate mean position of all shapes
			float mean_x = 0.0f, mean_y = 0.0f, mean_z = 0.0f;
			float min_x = shapes[0]->x, max_x = shapes[0]->x;
			float min_y = shapes[0]->y, max_y = shapes[0]->y;
			float min_z = shapes[0]->z, max_z = shapes[0]->z;

			for (const auto& shape : shapes) {
				mean_x += shape->x;
				mean_y += shape->y;
				mean_z += shape->z;

				min_x = std::min(min_x, shape->x);
				max_x = std::max(max_x, shape->x);
				min_y = std::min(min_y, shape->y);
				max_y = std::max(max_y, shape->y);
				min_z = std::min(min_z, shape->z);
				max_z = std::max(max_z, shape->z);
			}

			mean_x /= shapes.size();
			mean_y /= shapes.size();
			mean_z /= shapes.size();

			// Calculate bounding box size to determine appropriate distance
			float extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
			float target_distance = extent * 2.0f + 5.0f; // Ensure all points are visible

			// Smoothly adjust distance to keep points in center 2/3 of screen
			auto_camera_distance += (target_distance - auto_camera_distance) * delta_time * 0.5f;

			// Sinusoidal height variation (staying above floor, max 30 degrees)
			float height_cycle = sin(auto_camera_time * 0.3f) * 0.5f + 0.5f; // 0 to 1
			float camera_height = mean_y + 1.0f + height_cycle * auto_camera_distance * 0.5f;

			// Gentle rotation around mean point with varying speed
			float rotation_speed = 0.2f + 0.1f * sin(auto_camera_time * 0.15f); // Variable speed
			auto_camera_angle += rotation_speed * delta_time;

			// Gradual direction changes
			float direction_modifier = sin(auto_camera_time * 0.08f) * 0.3f; // Gentle direction variation
			float effective_angle = auto_camera_angle + direction_modifier;

			// Position camera in a circle around the mean point
			camera.x = mean_x + cos(effective_angle) * auto_camera_distance;
			camera.z = mean_z + sin(effective_angle) * auto_camera_distance;
			camera.y = camera_height;

			// Look at the mean point
			float dx = mean_x - camera.x;
			float dy = mean_y - camera.y;
			float dz = mean_z - camera.z;

			float distance_xz = sqrt(dx * dx + dz * dz);

			// Calculate yaw and pitch to look at mean point
			camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
			camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;

			// Constrain pitch to stay below 30 degrees above horizontal
			camera.pitch = std::max(-89.0f, std::min(30.0f, camera.pitch));
		}

		void UpdateSingleTrackCamera(float delta_time, const std::vector<Dot>& dots) {
			if (!single_track_mode || dots.empty()) {
				return;
			}

			// Ensure tracked_dot_index is within bounds
			if (tracked_dot_index >= static_cast<int>(dots.size())) {
				tracked_dot_index = 0;
			}

			const Dot& target_dot = dots[tracked_dot_index];

			float target_x = target_dot.x;
			float target_y = target_dot.y;
			float target_z = target_dot.z;

			// Desired distance from the target
			float distance = 15.0f;
			float camera_height_offset = 5.0f;

			// Smoothly move the camera towards the target
			float pan_speed = 2.0f * delta_time;
			camera.x += (target_x - camera.x) * pan_speed;
			camera.y += (target_y + camera_height_offset - camera.y) * pan_speed;
			camera.z += (target_z - distance - camera.z) * pan_speed;

			// Look at the target
			float dx = target_x - camera.x;
			float dy = target_y - camera.y;
			float dz = target_z - camera.z;

			float distance_xz = sqrt(dx * dx + dz * dz);

			camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
			camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;

			// Constrain pitch
			camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));
		}

		void UpdateSingleTrackCamera(float delta_time, const std::vector<std::shared_ptr<Shape>>& shapes) {
			if (!single_track_mode || shapes.empty()) {
				return;
			}

			// Ensure tracked_dot_index is within bounds
			if (tracked_dot_index >= static_cast<int>(shapes.size())) {
				tracked_dot_index = 0;
			}

			const auto& target_dot = shapes[tracked_dot_index];

			float target_x = target_dot->x;
			float target_y = target_dot->y;
			float target_z = target_dot->z;

			// Desired distance from the target
			float distance = 15.0f;
			float camera_height_offset = 5.0f;

			// Smoothly move the camera towards the target
			float pan_speed = 2.0f * delta_time;
			camera.x += (target_x - camera.x) * pan_speed;
			camera.y += (target_y + camera_height_offset - camera.y) * pan_speed;
			camera.z += (target_z - distance - camera.z) * pan_speed;

			// Look at the target
			float dx = target_x - camera.x;
			float dy = target_y - camera.y;
			float dz = target_z - camera.z;

			float distance_xz = sqrt(dx * dx + dz * dz);

			camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
			camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;

			// Constrain pitch
			camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));
		}

		void CleanupOldTrails(float current_time, const std::vector<std::shared_ptr<Shape>>& active_shapes) {
			// Create set of active dot IDs
			std::set<int> active_ids;
			for (const auto& shape : active_shapes) {
				active_ids.insert(shape->id);
			}

			// Remove trails for shapes that haven't been updated in a while
			auto trail_it = trails.begin();
			while (trail_it != trails.end()) {
				int trail_id = trail_it->first;

				// If dot is not active and trail hasn't been updated recently, remove it
				if (active_ids.find(trail_id) == active_ids.end()) {
					auto update_it = trail_last_update.find(trail_id);
					if (update_it != trail_last_update.end() &&
					    (current_time - update_it->second) > 2.0f) { // 2 second timeout
						trail_it = trails.erase(trail_it);
						trail_last_update.erase(trail_id);
						continue;
					}
				}
				++trail_it;
			}
		}

		static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
			(void)scancode;
			(void)mods; // Suppress unused parameter warnings
			VisualizerImpl* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(window));

			if (key >= 0 && key < 1024) {
				if (action == GLFW_PRESS) {
					impl->keys[key] = true;
				} else if (action == GLFW_RELEASE) {
					impl->keys[key] = false;
				}
			}

			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
				glfwSetWindowShouldClose(window, true);
			}

			// Toggle auto-camera mode with 0 key
			if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
				impl->auto_camera_mode = !impl->auto_camera_mode;
				if (impl->auto_camera_mode) {
					std::cout << "Auto-camera mode enabled" << std::endl;
					// Reset cursor capture when switching to auto mode
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				} else {
					std::cout << "Auto-camera mode disabled" << std::endl;
					// Re-enable cursor capture for manual mode
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
					impl->first_mouse = true; // Reset mouse to avoid jumps
				}
			}

			if (key == GLFW_KEY_9 && action == GLFW_PRESS) {
				impl->single_track_mode = true;
				impl->auto_camera_mode = false;
				impl->tracked_dot_index++;
				std::cout << "Single-track camera enabled" << std::endl;
			}

			if (key == GLFW_KEY_8 && action == GLFW_PRESS) {
				impl->single_track_mode = false;
				std::cout << "Single-track camera disabled" << std::endl;
			}
		}

		static void MouseCallback(GLFWwindow* window, double xpos, double ypos) {
			VisualizerImpl* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(window));

			// Skip mouse input if auto-camera is active
			if (impl->auto_camera_mode) {
				return;
			}

			if (impl->first_mouse) {
				impl->last_mouse_x = xpos;
				impl->last_mouse_y = ypos;
				impl->first_mouse = false;
			}

			double xoffset = xpos - impl->last_mouse_x;
			double yoffset = ypos - impl->last_mouse_y; // Fixed: don't reverse Y

			impl->last_mouse_x = xpos;
			impl->last_mouse_y = ypos;

			float sensitivity = 0.1f;
			xoffset *= sensitivity;
			yoffset *= sensitivity;

			impl->camera.yaw += static_cast<float>(xoffset);   // Fixed: add for correct direction
			impl->camera.pitch -= static_cast<float>(yoffset); // Subtract for natural mouse feel

			// Constrain pitch
			if (impl->camera.pitch > 89.0f) {
				impl->camera.pitch = 89.0f;
			}
			if (impl->camera.pitch < -89.0f) {
				impl->camera.pitch = -89.0f;
			}
		}

		static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
			VisualizerImpl* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(window));
			impl->width = width;
			impl->height = height;
			glViewport(0, 0, width, height);
		}
	};

	// Visualizer public interface implementation
	Visualizer::Visualizer(int width, int height, const char* title) {
		impl = new VisualizerImpl(width, height, title);
	}

	Visualizer::~Visualizer() {
		delete impl;
	}

	void Visualizer::SetShapeHandler(ShapeFunction func) {
		impl->shape_function = func;
	}

	bool Visualizer::ShouldClose() const {
		return glfwWindowShouldClose(impl->window);
	}

	void Visualizer::Update() {
		glfwPollEvents();

		static auto last_frame = std::chrono::high_resolution_clock::now();
		auto        current_frame = std::chrono::high_resolution_clock::now();
		float       delta_time = std::chrono::duration<float>(current_frame - last_frame).count();
		last_frame = current_frame;

		impl->ProcessInput(delta_time);
	}

	void Visualizer::Render() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Get current time for dot function
		auto  current_time = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float>(current_time - impl->start_time).count();

		// Get shapes first so we can use them for auto-camera
		std::vector<std::shared_ptr<Shape>> shapes;
		if (impl->shape_function) {
			shapes = impl->shape_function(time);

			static bool first_render = true;
			if (first_render) {
				std::cout << "Rendering " << shapes.size() << " shapes" << std::endl;
				if (!shapes.empty()) {
					std::cout << "First shape: pos(" << shapes[0]->x << ", " << shapes[0]->y << ", " << shapes[0]->z
							  << ") color(" << shapes[0]->r << ", " << shapes[0]->g << ", " << shapes[0]->b << ")"
							  << std::endl;
				}
				first_render = false;
			}
		}

		// Update auto-camera before setting up camera
		static auto last_frame_time = current_time;
		float       delta_time = std::chrono::duration<float>(current_time - last_frame_time).count();
		last_frame_time = current_time;
		if (impl->single_track_mode) {
			impl->UpdateSingleTrackCamera(delta_time, shapes);
		} else {
			impl->UpdateAutoCamera(delta_time, shapes);
		}

		glm::mat4 view = impl->SetupMatrices();
		impl->RenderGrid(view, impl->projection);

		// Set lighting uniforms
		impl->shader->setVec3("lightPos", 1.0f, 1.0f, 1.0f);
		impl->shader->setVec3("viewPos", impl->camera.x, impl->camera.y, impl->camera.z);
		impl->shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);


		// Render shapes and trails
		if (!shapes.empty()) {
			// Clean up old trails first
			impl->CleanupOldTrails(time, shapes);

			// Track which shapes we've seen this frame
			std::set<int> current_shape_ids;

			for (const auto& shape : shapes) {
				current_shape_ids.insert(shape->id);

				// Create or update trail using shape ID
				if (impl->trails.find(shape->id) == impl->trails.end()) {
					impl->trails[shape->id] = std::make_shared<Trail>(shape->trail_length);
				}
				impl->trails[shape->id]->AddPosition(shape->x, shape->y, shape->z);

				// Update last seen time for this trail
				impl->trail_last_update[shape->id] = time;

				// Render shape
				shape->render();
			}

			// Render trails
			impl->trail_shader->use();
			impl->trail_shader->setMat4("view", view);
			impl->trail_shader->setMat4("projection", impl->projection);
			for (const auto& pair : impl->trails) {
				const auto& shape = std::find_if(shapes.begin(), shapes.end(), [&](const auto& s) { return s->id == pair.first; });
				if (shape != shapes.end()) {
					pair.second->Render(*impl->trail_shader, (*shape)->r, (*shape)->g, (*shape)->b, glm::vec3(impl->camera.x, impl->camera.y, impl->camera.z));
				}
			}
		}

		glfwSwapBuffers(impl->window);
	}

	void Visualizer::Run() {
		while (!ShouldClose()) {
			Update();
			Render();
		}
	}

	Camera& Visualizer::GetCamera() {
		return impl->camera;
	}

	void Visualizer::SetCamera(const Camera& camera) {
		impl->camera = camera;
	}
} // namespace Boidsish
