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

	Dot::Dot(int id, float x, float y, float z, float size, float r, float g, float b, float a, int trail_length):
		Shape(id, x, y, z, r, g, b, a, trail_length) {
		this->size = size;
	}

	// Trail data for each dot
	struct Trail {
		std::deque<std::tuple<float, float, float, float>> positions; // x, y, z, alpha
		int                                                max_length;
		unsigned int                                       VAO, VBO;

		Trail(int length = 250): max_length(length), VAO(0), VBO(0) {
			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);

			glBindVertexArray(VAO);
			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(GL_ARRAY_BUFFER, (max_length + 2) * 7 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

			// Position attribute
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			// Color attribute
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);

			glBindVertexArray(0);
		}

		~Trail() {
			glDeleteVertexArrays(1, &VAO);
			glDeleteBuffers(1, &VBO);
		}

		void AddPosition(float x, float y, float z, float r, float g, float b) {
			positions.push_back({x, y, z, 1.0f});
			if (positions.size() > static_cast<size_t>(max_length)) {
				positions.pop_front();
			}

			if (positions.size() < 2) {
				return;
			}

			std::vector<float> vertex_data;
			vertex_data.reserve((positions.size() + 2) * 7);

			// Adjacency vertex at the beginning
			vertex_data.push_back(std::get<0>(positions[0]));
			vertex_data.push_back(std::get<1>(positions[0]));
			vertex_data.push_back(std::get<2>(positions[0]));
			vertex_data.push_back(r);
			vertex_data.push_back(g);
			vertex_data.push_back(b);
			vertex_data.push_back(0.0f);

			for (size_t i = 0; i < positions.size(); ++i) {
				float t = static_cast<float>(i) / static_cast<float>(positions.size());
				float alpha = pow(t, 0.7f);

				vertex_data.push_back(std::get<0>(positions[i]));
				vertex_data.push_back(std::get<1>(positions[i]));
				vertex_data.push_back(std::get<2>(positions[i]));
				vertex_data.push_back(r);
				vertex_data.push_back(g);
				vertex_data.push_back(b);
				vertex_data.push_back(alpha);
			}

			// Adjacency vertex at the end
			vertex_data.push_back(std::get<0>(positions.back()));
			vertex_data.push_back(std::get<1>(positions.back()));
			vertex_data.push_back(std::get<2>(positions.back()));
			vertex_data.push_back(r);
			vertex_data.push_back(g);
			vertex_data.push_back(b);
			vertex_data.push_back(1.0f);

			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_data.size() * sizeof(float), vertex_data.data());
		}
	};

	// Implementation details hidden from header
	struct Visualizer::VisualizerImpl {
		GLFWwindow*          window;
		int                  width, height;
		Camera               camera;
		ShapeFunction        shape_function;
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
		Shader*                                      trail_shader;
		Shader*                                      grid_shader;
		Shader*                                      sphere_shader;
		Shader*                                      edge_shader;
		unsigned int                                 grid_VAO, grid_VBO;
		unsigned int                                 dot_VAO, dot_VBO;
		unsigned int                                 sphere_VAO, sphere_VBO, sphere_EBO;
		int                                          sphere_index_count;

		void CreateSphereMesh() {
			std::vector<float> vertices;
			std::vector<unsigned int> indices;
			const int longitude_segments = 12;
			const int latitude_segments = 8;
			const float radius = 1.0f;

			for (int lat = 0; lat <= latitude_segments; ++lat) {
				float lat0 = M_PI * (-0.5f + (float)lat / latitude_segments);
				float y0 = sin(lat0);
				float r0 = cos(lat0);

				for (int lon = 0; lon <= longitude_segments; ++lon) {
					float lon0 = 2 * M_PI * (float)lon / longitude_segments;
					float x0 = cos(lon0);
					float z0 = sin(lon0);

					vertices.push_back(x0 * r0 * radius);
					vertices.push_back(y0 * radius);
					vertices.push_back(z0 * r0 * radius);
					vertices.push_back(x0 * r0);
					vertices.push_back(y0);
					vertices.push_back(z0 * r0);
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

			sphere_index_count = indices.size();

			glGenVertexArrays(1, &sphere_VAO);
			glGenBuffers(1, &sphere_VBO);
			glGenBuffers(1, &sphere_EBO);

			glBindVertexArray(sphere_VAO);

			glBindBuffer(GL_ARRAY_BUFFER, sphere_VBO);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_EBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
			glEnableVertexAttribArray(1);

			glBindVertexArray(0);
		}

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
			tracked_dot_index(0),
			coordinate_wrapping_enabled(false),
			wrap_range(20.0f),
			trail_shader(nullptr),
			grid_shader(nullptr),
			sphere_shader(nullptr),
			edge_shader(nullptr),
			grid_VAO(0),
			grid_VBO(0),
			dot_VAO(0),
			dot_VBO(0),
			sphere_VAO(0),
			sphere_VBO(0),
			sphere_EBO(0),
			sphere_index_count(0) {
			// Initialize all keys to false
			for (int i = 0; i < 1024; ++i) {
				keys[i] = false;
			}

			start_time = std::chrono::high_resolution_clock::now();

			if (!glfwInit()) {
				throw std::runtime_error("Failed to initialize GLFW");
			}

			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

			window = glfwCreateWindow(width, height, title, nullptr, nullptr);
			if (!window) {
				glfwTerminate();
				throw std::runtime_error("Failed to create GLFW window");
			}

			glfwMakeContextCurrent(window);
			glewExperimental = GL_TRUE;
			if (glewInit() != GLEW_OK) {
				throw std::runtime_error("Failed to initialize GLEW");
			}

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			trail_shader = new Shader("shaders/trail.vs", "shaders/trail.fs", "shaders/trail.gs");
			grid_shader = new Shader("shaders/grid.vs", "shaders/grid.fs");
			sphere_shader = new Shader("shaders/sphere.vs", "shaders/sphere.fs");
			edge_shader = new Shader("shaders/edge.vs", "shaders/edge.fs");

			glGenVertexArrays(1, &dot_VAO);
			glGenBuffers(1, &dot_VBO);

			CreateSphereMesh();

			const GLubyte* version = glGetString(GL_VERSION);
			std::cout << "OpenGL Version: " << version << std::endl;

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			// Set up callbacks
			glfwSetWindowUserPointer(window, this);
			glfwSetKeyCallback(window, KeyCallback);
			glfwSetCursorPosCallback(window, MouseCallback);
			glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
		}

		~VisualizerImpl() {
			delete trail_shader;
			delete grid_shader;
			delete sphere_shader;
			delete edge_shader;
			glDeleteVertexArrays(1, &grid_VAO);
			glDeleteBuffers(1, &grid_VBO);
			glDeleteVertexArrays(1, &dot_VAO);
			glDeleteBuffers(1, &dot_VBO);
			glDeleteVertexArrays(1, &sphere_VAO);
			glDeleteBuffers(1, &sphere_VBO);
			glDeleteBuffers(1, &sphere_EBO);
			if (window) {
				glfwDestroyWindow(window);
			}
			glfwTerminate();
		}

		void SetupPerspective() {
			// This is now handled by the projection matrix in the shader.
		}

		void SetupCamera() {
			// This is now handled by the view matrix in the shader.
		}

		void ProcessInput(float delta_time) {
			// Skip manual camera controls if auto-camera is active
			if (auto_camera_mode) {
				return;
			}

			float camera_speed = 5.0f * delta_time;

			// Calculate camera direction vectors
			glm::vec3 front;
			front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
			front.y = sin(glm::radians(camera.pitch));
			front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
			camera.front = glm::normalize(front);
			camera.right = glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 1.0f, 0.0f)));
			camera.up = glm::normalize(glm::cross(camera.right, camera.front));

			glm::vec3 camera_pos(camera.x, camera.y, camera.z);

			// Movement
			if (keys[GLFW_KEY_W]) {
				camera_pos += camera.front * camera_speed;
			}
			if (keys[GLFW_KEY_S]) {
				camera_pos -= camera.front * camera_speed;
			}
			if (keys[GLFW_KEY_A]) {
				camera_pos -= camera.right * camera_speed;
			}
			if (keys[GLFW_KEY_D]) {
				camera_pos += camera.right * camera_speed;
			}
			if (keys[GLFW_KEY_SPACE]) {
				camera_pos.y += camera_speed;
			}
			if (keys[GLFW_KEY_LEFT_SHIFT]) {
				camera_pos.y -= camera_speed;
			}
			camera.x = camera_pos.x;
			camera.y = camera_pos.y;
			camera.z = camera_pos.z;
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

		void RenderGrid(const glm::mat4& projection, const glm::mat4& view) {
			if (grid_VAO == 0) {
				std::vector<float> grid_vertices;
				int   grid_size = 25;
				float grid_spacing = 1.0f;

				for (int i = -grid_size; i <= grid_size; ++i) {
					grid_vertices.push_back(-grid_size * grid_spacing);
					grid_vertices.push_back(0.0f);
					grid_vertices.push_back(i * grid_spacing);
					grid_vertices.push_back(grid_size * grid_spacing);
					grid_vertices.push_back(0.0f);
					grid_vertices.push_back(i * grid_spacing);

					grid_vertices.push_back(i * grid_spacing);
					grid_vertices.push_back(0.0f);
					grid_vertices.push_back(-grid_size * grid_spacing);
					grid_vertices.push_back(i * grid_spacing);
					grid_vertices.push_back(0.0f);
					grid_vertices.push_back(grid_size * grid_spacing);
				}

				glGenVertexArrays(1, &grid_VAO);
				glGenBuffers(1, &grid_VBO);

				glBindVertexArray(grid_VAO);
				glBindBuffer(GL_ARRAY_BUFFER, grid_VBO);
				glBufferData(GL_ARRAY_BUFFER, grid_vertices.size() * sizeof(float), grid_vertices.data(), GL_STATIC_DRAW);

				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
				glEnableVertexAttribArray(0);

				glBindVertexArray(0);
			}

			grid_shader->use();
			grid_shader->setVec4("color", 0.0f, 0.8f, 1.0f, 0.6f);
			grid_shader->setMat4("projection", projection);
			grid_shader->setMat4("view", view);

			glBindVertexArray(grid_VAO);
			glDrawArrays(GL_LINES, 0, 4 * (2 * 25 + 1));
			glBindVertexArray(0);
		}

		void RenderShape(const std::shared_ptr<Shape>& shape, const glm::mat4& projection, const glm::mat4& view) {
			if (auto dot = std::dynamic_pointer_cast<Dot>(shape)) {
				sphere_shader->use();
				sphere_shader->setVec3("objectColor", dot->r, dot->g, dot->b);
				sphere_shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);
				sphere_shader->setVec3("lightPos", 1.0f, 1.0f, 1.0f);
				sphere_shader->setVec3("viewPos", camera.x, camera.y, camera.z);
				sphere_shader->setMat4("projection", projection);
				sphere_shader->setMat4("view", view);

				float x = dot->x, y = dot->y, z = dot->z;
				if (coordinate_wrapping_enabled && wrap_range > 0) {
					x = fmod(x + wrap_range, 2.0f * wrap_range) - wrap_range;
					y = fmod(y + wrap_range, 2.0f * wrap_range) - wrap_range;
					z = fmod(z + wrap_range, 2.0f * wrap_range) - wrap_range;
				}

				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, glm::vec3(x, y, z));
				model = glm::scale(model, glm::vec3(dot->size * 0.01f));
				sphere_shader->setMat4("model", model);

				glBindVertexArray(sphere_VAO);
				glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			} else if (auto graph = std::dynamic_pointer_cast<Graph>(shape)) {
				graph->render(*sphere_shader, *edge_shader, sphere_VAO, sphere_index_count, projection, view);
			}
		}

		void RenderTrail(const Trail& trail, const glm::mat4& projection, const glm::mat4& view) {
			if (trail.positions.size() < 2) {
				return;
			}

			trail_shader->use();
			trail_shader->setMat4("view", view);

			glm::mat4 model = glm::mat4(1.0f);
			trail_shader->setMat4("model", model);
			trail_shader->setMat4("projection", projection);

			trail_shader->setFloat("thickness", 0.02f);

			glBindVertexArray(trail.VAO);
			glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, trail.positions.size() + 2);
			glBindVertexArray(0);
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
			impl->SetupPerspective();
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
		impl->UpdateAutoCamera(delta_time, shapes);

		glm::mat4 projection = glm::perspective(glm::radians(impl->camera.fov), (float)impl->width / (float)impl->height, 0.1f, 100.0f);
		glm::vec3 camera_pos(impl->camera.x, impl->camera.y, impl->camera.z);
		glm::mat4 view = glm::lookAt(camera_pos, camera_pos + impl->camera.front, impl->camera.up);

		impl->RenderGrid(projection, view);

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
				trail.AddPosition(shape->x, shape->y, shape->z, shape->r, shape->g, shape->b);

				// Update last seen time for this trail
				impl->trail_last_update[shape->id] = time;

				// Render trail
				impl->RenderTrail(trail, projection, view);
				impl->RenderShape(shape, projection, view);
			}

			// Render trails for dots that are no longer active but still fading
			for (auto& trail_pair : impl->trails) {
				int    trail_id = trail_pair.first;
				Trail& trail = trail_pair.second;

				// If this trail is for a dot we didn't see this frame, just render the trail
				if (current_shape_ids.find(trail_id) == current_shape_ids.end() && !trail.positions.empty()) {
					// Use a default color for orphaned trails (white/gray)
					impl->RenderTrail(trail, projection, view);
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
