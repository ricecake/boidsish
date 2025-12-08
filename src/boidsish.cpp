#include "boidsish.h"

#include <chrono>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <shader.h>

namespace Boidsish {

	Dot::Dot(int id, float x, float y, float z, float size, float r, float g, float b, float a, int trail_length) {
		this->id = id;
		this->x = x;
		this->y = y;
		this->z = z;
		this->size = size;
		this->r = r;
		this->g = g;
		this->b = b;
		this->a = a;
		this->trail_length = trail_length;
	}

	void Dot::render() const {
		float radius = size * 0.01f;

		GLfloat material_ambient[] = {r * 0.2f, g * 0.2f, b * 0.2f, a};
		GLfloat material_diffuse[] = {r, g, b, a};
		GLfloat material_specular[] = {0.5f, 0.5f, 0.5f, a};
		GLfloat material_shininess[] = {32.0f};

		glMaterialfv(GL_FRONT, GL_AMBIENT, material_ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, material_diffuse);
		glMaterialfv(GL_FRONT, GL_SPECULAR, material_specular);
		glMaterialfv(GL_FRONT, GL_SHININESS, material_shininess);

		const int longitude_segments = 12;
		const int latitude_segments = 8;

		glBegin(GL_TRIANGLES);

		for (int lat = 0; lat < latitude_segments; ++lat) {
			float lat0 = M_PI * (-0.5f + (float)lat / latitude_segments);
			float lat1 = M_PI * (-0.5f + (float)(lat + 1) / latitude_segments);

			float y0 = sin(lat0) * radius;
			float y1 = sin(lat1) * radius;
			float r0 = cos(lat0) * radius;
			float r1 = cos(lat1) * radius;

			for (int lon = 0; lon < longitude_segments; ++lon) {
				float lon0 = 2 * M_PI * (float)lon / longitude_segments;
				float lon1 = 2 * M_PI * (float)(lon + 1) / longitude_segments;

				float x0 = cos(lon0);
				float z0 = sin(lon0);
				float x1 = cos(lon1);
				float z1 = sin(lon1);

				glNormal3f(x0 * cos(lat0), sin(lat0), z0 * cos(lat0));
				glVertex3f(x0 * r0, y0, z0 * r0);

				glNormal3f(x1 * cos(lat0), sin(lat0), z1 * cos(lat0));
				glVertex3f(x1 * r0, y0, z1 * r0);

				glNormal3f(x1 * cos(lat1), sin(lat1), z1 * cos(lat1));
				glVertex3f(x1 * r1, y1, z1 * r1);

				glNormal3f(x0 * cos(lat0), sin(lat0), z0 * cos(lat0));
				glVertex3f(x0 * r0, y0, z0 * r0);

				glNormal3f(x1 * cos(lat1), sin(lat1), z1 * cos(lat1));
				glVertex3f(x1 * r1, y1, z1 * r1);

				glNormal3f(x0 * cos(lat1), sin(lat1), z0 * cos(lat1));
				glVertex3f(x0 * r1, y1, z0 * r1);
			}
		}

		glEnd();
	}

	// Trail data for each dot
	struct Trail {
		std::deque<std::tuple<float, float, float, float>> positions; // x, y, z, alpha
		int                                                max_length;

		Trail(int length = 250): max_length(length) {} // Increased default length

		void AddPosition(float x, float y, float z) {
			positions.push_back({x, y, z, 1.0f});
			if (positions.size() > static_cast<size_t>(max_length)) {
				positions.pop_front();
			}

			// Improved alpha fade - more gradual and vibrant
			for (size_t i = 0; i < positions.size(); ++i) {
				float t = static_cast<float>(i) / static_cast<float>(positions.size());
				// Use a power curve for more vibrant trails
				float alpha = pow(t, 0.7f); // Keeps more of the trail visible
				std::get<3>(positions[i]) = alpha;
			}
		}
	};

	// Implementation details hidden from header
	struct Visualizer::VisualizerImpl {
		GLFWwindow*   window;
		int           width, height;
		Camera        camera;
		ShapeFunction shape_function;
		Shader*       background_shader;
		GLuint        background_vao;
		Shader*       dot_shader;
		GLuint        sphere_vao;
		int           sphere_indices;
		glm::vec3     grid_color;

		std::map<int, Trail> trails;            // Trail for each dot (using dot ID as key)
		std::map<int, float> trail_last_update; // Track when each trail was last updated for cleanup

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
		int	 tracked_dot_index;

		// Coordinate wrapping
		bool  coordinate_wrapping_enabled;
		float wrap_range;

		// Timing
		std::chrono::high_resolution_clock::time_point start_time;

		VisualizerImpl(int w, int h, const char* title):
			window(nullptr),
			width(w),
			height(h),
			camera(),
			shape_function(nullptr),
			background_shader(nullptr),
			background_vao(0),
			dot_shader(nullptr),
			sphere_vao(0),
			sphere_indices(0),
			grid_color(0.0f, 1.0f, 1.0f),
			last_mouse_x(0.0),
			last_mouse_y(0.0),
			first_mouse(true),
			auto_camera_mode(true),
			auto_camera_time(0.0f),
			auto_camera_angle(0.0f),
			auto_camera_height_offset(0.0f),
			auto_camera_distance(10.0f),
			single_track_mode(false),
			tracked_dot_index(0),
			coordinate_wrapping_enabled(false),
			wrap_range(20.0f) {
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

			// Use more flexible OpenGL settings for better compatibility
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

			// For macOS - try without forward compatibility first
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

			window = glfwCreateWindow(width, height, title, nullptr, nullptr);
			if (!window) {
				std::cerr << "Failed to create window with OpenGL 2.1, trying with default settings..." << std::endl;

				// Reset hints and try with default OpenGL
				glfwDefaultWindowHints();
				window = glfwCreateWindow(width, height, title, nullptr, nullptr);

				if (!window) {
					std::cerr << "Failed to create window with default settings" << std::endl;
					glfwTerminate();
					throw std::runtime_error("Failed to create GLFW window - check OpenGL drivers");
				}
			}

			glfwMakeContextCurrent(window);
			glewExperimental = true;

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

			glEnable(GL_LIGHTING);
			glEnable(GL_LIGHT0);
			glEnable(GL_NORMALIZE);

			GLfloat light_pos[] = {1.0f, 1.0f, 1.0f, 0.0f};
			glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

			// Check OpenGL version
			const GLubyte* version = glGetString(GL_VERSION);
			std::cout << "OpenGL Version: " << version << std::endl;

			// Set background color (black for holodeck effect)
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			float quad_vertices[] = {
				-1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,

				-1.0f, 1.0f, 0.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  0.0f,
			};

			// screen quad VAO
			unsigned int quadVBO;
			glGenVertexArrays(1, &background_vao);
			glGenBuffers(1, &quadVBO);
			glBindVertexArray(background_vao);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

			// Generate sphere mesh
			std::vector<float> vertices;
			std::vector<int>   indices;
			const int          longitude_segments = 12;
			const int          latitude_segments = 8;
			for (int lat = 0; lat <= latitude_segments; ++lat) {
				for (int lon = 0; lon <= longitude_segments; ++lon) {
					float theta = lon * 2.0 * M_PI / longitude_segments;
					float phi = lat * M_PI / latitude_segments;
					float x = cos(theta) * sin(phi);
					float y = cos(phi);
					float z = sin(theta) * sin(phi);
					vertices.push_back(x);
					vertices.push_back(y);
					vertices.push_back(z);
					vertices.push_back(x); // Normal
					vertices.push_back(y);
					vertices.push_back(z);
				}
			}
			for (int lat = 0; lat < latitude_segments; ++lat) {
				for (int lon = 0; lon < longitude_segments; ++lon) {
					int first = (lat * (longitude_segments + 1)) + lon;
					int second = first + longitude_segments + 1;
					indices.push_back(first);
					indices.push_back(second);
					indices.push_back(first + 1);
					indices.push_back(second);
					indices.push_back(second + 1);
					indices.push_back(first + 1);
				}
			}
			sphere_indices = indices.size();

			unsigned int sphere_vbo, sphere_ebo;
			glGenVertexArrays(1, &sphere_vao);
			glGenBuffers(1, &sphere_vbo);
			glGenBuffers(1, &sphere_ebo);

			glBindVertexArray(sphere_vao);
			glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
			glBufferData(
				GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(int), &indices[0], GL_STATIC_DRAW
			);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

			background_shader = new Shader("shaders/background.vs", "shaders/background.fs");
			dot_shader = new Shader("shaders/dot.vs", "shaders/dot.fs");
		}

		~VisualizerImpl() {
			glDeleteVertexArrays(1, &background_vao);
			glDeleteVertexArrays(1, &sphere_vao);
			delete background_shader;
			delete dot_shader;

			if (window) {
				glfwDestroyWindow(window);
			}
			glfwTerminate();
		}

		void SetupPerspective() {
			// Deprecated
		}

		void SetupCamera() {
			// Deprecated
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
			// right = forward x up = (forward_x, forward_y, forward_z) x (0, 1, 0)
			// right = (forward_z * 0 - forward_y * 1, forward_x * 0 - forward_z * 0, forward_x * 1 - forward_y * 0)
			// right = (-forward_y, 0, forward_x)
			// But for horizontal movement only, we normalize in the XZ plane:
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

		void RenderGrid() {
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_LIGHTING); // Disable lighting for grid
			glLineWidth(1.0f);

			// Holodeck-style grid on z=0 plane
			glBegin(GL_LINES);

			int   grid_size = 25;
			float grid_spacing = 1.0f;

			// Main grid lines (brighter cyan)
			glColor4f(0.0f, 0.8f, 1.0f, 0.6f);

			for (int i = -grid_size; i <= grid_size; ++i) {
				// Every 5th line is brighter
				if (i % 5 == 0) {
					glColor4f(0.0f, 0.9f, 1.0f, 0.8f);
				} else {
					glColor4f(0.0f, 0.6f, 0.8f, 0.4f);
				}

				// Lines parallel to X axis (on z=0 plane)
				glVertex3f(-grid_size * grid_spacing, 0.0f, i * grid_spacing);
				glVertex3f(grid_size * grid_spacing, 0.0f, i * grid_spacing);

				// Lines parallel to Z axis (on z=0 plane)
				glVertex3f(i * grid_spacing, 0.0f, -grid_size * grid_spacing);
				glVertex3f(i * grid_spacing, 0.0f, grid_size * grid_spacing);
			}

			// Add subtle border/frame effect
			glColor4f(0.0f, 1.0f, 1.0f, 0.9f);
			glVertex3f(-grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);
			glVertex3f(grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);

			glVertex3f(grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);
			glVertex3f(grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);

			glVertex3f(grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);
			glVertex3f(-grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);

			glVertex3f(-grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);
			glVertex3f(-grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);

			glEnd();
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_LIGHTING); // Re-enable lighting after grid
		}

		void RenderTrail(const Trail& trail, float r, float g, float b) {
			if (trail.positions.size() < 2)
				return;

			glDisable(GL_LIGHTING); // Disable lighting for trails
			glLineWidth(2.0f);      // Thicker lines for better visibility

			// Render trail with gradient effect
			glBegin(GL_LINE_STRIP);
			for (size_t i = 0; i < trail.positions.size(); ++i) {
				const auto& pos = trail.positions[i];
				float       alpha = std::get<3>(pos);

				// Make colors more vibrant and add some brightness boost
				float brightness_boost = 1.5f;
				float trail_r = std::min(1.0f, r * brightness_boost);
				float trail_g = std::min(1.0f, g * brightness_boost);
				float trail_b = std::min(1.0f, b * brightness_boost);

				// Add a subtle glow effect for newer trail segments
				if (alpha > 0.7f) {
					brightness_boost = 2.0f;
					trail_r = std::min(1.0f, r * brightness_boost);
					trail_g = std::min(1.0f, g * brightness_boost);
					trail_b = std::min(1.0f, b * brightness_boost);
				}

				glColor4f(trail_r, trail_g, trail_b, alpha * 0.8f); // Increased base alpha
				glVertex3f(std::get<0>(pos), std::get<1>(pos), std::get<2>(pos));
			}
			glEnd();

			glLineWidth(1.0f);     // Reset line width
			glEnable(GL_LIGHTING); // Re-enable lighting
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

			// Toggle coordinate wrapping with B key (for "Boundary")
			if (key == GLFW_KEY_B && action == GLFW_PRESS) {
				impl->coordinate_wrapping_enabled = !impl->coordinate_wrapping_enabled;
				if (impl->coordinate_wrapping_enabled) {
					std::cout << "Coordinate wrapping enabled (range: ±" << impl->wrap_range << ")" << std::endl;
				} else {
					std::cout << "Coordinate wrapping disabled" << std::endl;
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

		void RenderBackgroundAndGrid() {
			glDisable(GL_DEPTH_TEST);
			background_shader->use();

			glm::mat4 projection = glm::perspective(
				glm::radians(camera.fov), (float)width / (float)height, 0.1f, 1000.0f
			);
			glm::vec3 camera_pos = glm::vec3(camera.x, camera.y, camera.z);
			glm::vec3 camera_front = glm::vec3(
				cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw)),
				sin(glm::radians(camera.pitch)),
				-cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw))
			);
			glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);

			background_shader->setMat4("inverseViewMatrix", inverse(projection * view));
			background_shader->setVec3("cameraPosition", camera_pos);
			background_shader->setVec3("gridColor", grid_color);

			glBindVertexArray(background_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glEnable(GL_DEPTH_TEST);
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
		glClear(GL_DEPTH_BUFFER_BIT);

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

		// Render grid
		impl->RenderBackgroundAndGrid();

		// Render shapes and trails
		if (!shapes.empty()) {
			// Clean up old trails first
			impl->CleanupOldTrails(time, shapes);

			// Track which shapes we've seen this frame
			std::set<int> current_shape_ids;

			for (const auto& shape : shapes) {
				current_shape_ids.insert(shape->id);

				// Create or update trail using shape ID
				auto& trail = impl->trails[shape->id];
				trail.max_length = shape->trail_length;
				trail.AddPosition(shape->x, shape->y, shape->z);

				// Update last seen time for this trail
				impl->trail_last_update[shape->id] = time;

				// Render trail
				impl->RenderTrail(trail, shape->r, shape->g, shape->b);
			}

			impl->dot_shader->use();

			glm::mat4 projection = glm::perspective(
				glm::radians(impl->camera.fov),
				(float)impl->width / (float)impl->height,
				0.1f,
				1000.0f
			);
			glm::vec3 camera_pos = glm::vec3(impl->camera.x, impl->camera.y, impl->camera.z);
			glm::vec3 camera_front = glm::vec3(
				cos(glm::radians(impl->camera.pitch)) * sin(glm::radians(impl->camera.yaw)),
				sin(glm::radians(impl->camera.pitch)),
				-cos(glm::radians(impl->camera.pitch)) * cos(glm::radians(impl->camera.yaw))
			);
			glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::mat4 view = glm::lookAt(camera_pos, camera_pos + camera_front, camera_up);

			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(glm::value_ptr(projection));
			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(glm::value_ptr(view));

			impl->dot_shader->setMat4("projection", projection);
			impl->dot_shader->setMat4("view", view);
			impl->dot_shader->setVec3("lightPos", camera_pos);
			impl->dot_shader->setVec3("viewPos", camera_pos);

			glBindVertexArray(impl->sphere_vao);

			// Reflections & Main Render
			for (const auto& shape : shapes) {
				std::shared_ptr<Dot> dot = std::dynamic_pointer_cast<Dot>(shape);
				if (dot) {
					glDisable(GL_LIGHTING);
					impl->dot_shader->use();
					glBindVertexArray(impl->sphere_vao);

					float x = dot->x, y = dot->y, z = dot->z;
					if (impl->coordinate_wrapping_enabled && impl->wrap_range > 0) {
						x = fmod(x + impl->wrap_range, 2.0f * impl->wrap_range) - impl->wrap_range;
						y = fmod(y + impl->wrap_range, 2.0f * impl->wrap_range) - impl->wrap_range;
						z = fmod(z + impl->wrap_range, 2.0f * impl->wrap_range) - impl->wrap_range;
					}

					// Reflection
					glm::mat4 model = glm::mat4(1.0f);
					model = glm::translate(model, glm::vec3(x, -y, z));
					model = glm::scale(model, glm::vec3(dot->size * 0.01f));
					impl->dot_shader->setMat4("model", model);
					impl->dot_shader->setVec3("objectColor", glm::vec3(dot->r, dot->g, dot->b));
					impl->dot_shader->setBool("isReflection", true);
					glDrawElements(GL_TRIANGLES, impl->sphere_indices, GL_UNSIGNED_INT, 0);

					// Main
					model = glm::mat4(1.0f);
					model = glm::translate(model, glm::vec3(x, y, z));
					model = glm::scale(model, glm::vec3(dot->size * 0.01f));
					impl->dot_shader->setMat4("model", model);
					impl->dot_shader->setBool("isReflection", false);
					glDrawElements(GL_TRIANGLES, impl->sphere_indices, GL_UNSIGNED_INT, 0);

					glUseProgram(0);
				} else {
					glEnable(GL_LIGHTING);
					glPushMatrix();
					shape->render();
					glPopMatrix();
				}
			}

			glEnable(GL_LIGHTING);
			glEnable(GL_LIGHTING);
			// Render trails for dots that are no longer active but still fading
			for (auto& trail_pair : impl->trails) {
				int    trail_id = trail_pair.first;
				Trail& trail = trail_pair.second;

				// If this trail is for a dot we didn't see this frame, just render the trail
				if (current_shape_ids.find(trail_id) == current_shape_ids.end() && !trail.positions.empty()) {
					// Use a default color for orphaned trails (white/gray)
					impl->RenderTrail(trail, 0.7f, 0.7f, 0.7f);
				}
			}
			glDisable(GL_LIGHTING);
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

	// EntityHandler implementation
	std::vector<std::shared_ptr<Shape>> EntityHandler::operator()(float time) {
		float delta_time = 0.016f; // Default 60 FPS
		if (last_time_ >= 0.0f) {
			delta_time = time - last_time_;
		}
		last_time_ = time;

		// Call pre-timestep hook
		PreTimestep(time, delta_time);

		// Get entities
		std::vector<std::shared_ptr<Entity>> entities;
		std::transform(entities_.begin(), entities_.end(), std::back_inserter(entities), [](const auto& pair) {
			return pair.second;
		});

		// Update all entities
		for (auto& entity : entities) {
			entity->UpdateEntity(*this, time, delta_time);
		}

		// Call post-timestep hook
		PostTimestep(time, delta_time);

		// Generate shapes from entity states
		std::vector<std::shared_ptr<Shape>> shapes;
		shapes.reserve(entities_.size());

		for (auto& entity : entities) {
			// Update entity position using its velocity
			Vector3 new_position = entity->GetPosition() + entity->GetVelocity() * delta_time;
			entity->SetPosition(new_position);

			// Get visual properties
			float r, g, b, a;
			entity->GetColor(r, g, b, a);

			// Create dot at entity's position
			shapes.emplace_back(
				std::make_shared<Dot>(
					entity->GetId(),
					entity->GetXPos(),
					entity->GetYPos(),
					entity->GetZPos(),
					entity->GetSize(),
					r,
					g,
					b,
					a,
					entity->GetTrailLength()
				)
			);
		}

		return shapes;
	}

} // namespace Boidsish