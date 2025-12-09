#include "graphics.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "dot.h"
#include "trail.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <shader.h>

namespace Boidsish {

	std::shared_ptr<Shader> Shape::shader = nullptr;

	struct Visualizer::VisualizerImpl {
		GLFWwindow*                           window;
		int                                   width, height;
		Camera                                camera;
		ShapeFunction                         shape_function;
		std::map<int, std::shared_ptr<Trail>> trails;
		std::map<int, float>                  trail_last_update;

		std::shared_ptr<Shader> shader;
		std::unique_ptr<Shader> grid_shader;
		std::unique_ptr<Shader> trail_shader;
		GLuint                  grid_vao, grid_vbo;
		glm::mat4               projection;

		double last_mouse_x = 0.0, last_mouse_y = 0.0;
		bool   first_mouse = true;
		bool   keys[1024] = {false};

		bool  auto_camera_mode = true;
		float auto_camera_time = 0.0f;
		float auto_camera_angle = 0.0f;
		float auto_camera_height_offset = 0.0f;
		float auto_camera_distance = 10.0f;

		bool single_track_mode = false;
		int  tracked_dot_index = 0;

		std::chrono::high_resolution_clock::time_point start_time;

		VisualizerImpl(int w, int h, const char* title): width(w), height(h) {
			start_time = std::chrono::high_resolution_clock::now();
			if (!glfwInit())
				throw std::runtime_error("Failed to initialize GLFW");

			glfwSetErrorCallback([](int error, const char* description) {
				std::cerr << "GLFW Error " << error << ": " << description << std::endl;
			});

			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

			window = glfwCreateWindow(width, height, title, nullptr, nullptr);
			if (!window) {
				glfwTerminate();
				throw std::runtime_error("Failed to create GLFW window");
			}
			glfwMakeContextCurrent(window);

			if (glewInit() != GLEW_OK) {
				throw std::runtime_error("Failed to initialize GLEW");
			}

			glfwSetWindowUserPointer(window, this);
			glfwSetKeyCallback(window, KeyCallback);
			glfwSetCursorPosCallback(window, MouseCallback);
			glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			shader = std::make_shared<Shader>("shaders/vis.vert", "shaders/vis.frag");
			Shape::shader = shader;
			grid_shader = std::make_unique<Shader>("shaders/grid.vert", "shaders/grid.frag");
			trail_shader = std::make_unique<Shader>("shaders/trail.vert", "shaders/trail.frag", "shaders/trail.geom");

			Dot::InitSphereMesh();

			float quad_vertices[] = {
				-1.0f,
				0.0f,
				-1.0f,
				1.0f,
				0.0f,
				-1.0f,
				1.0f,
				0.0f,
				1.0f,
				1.0f,
				0.0f,
				1.0f,
				-1.0f,
				0.0f,
				1.0f,
				-1.0f,
				0.0f,
				-1.0f
			};
			glGenVertexArrays(1, &grid_vao);
			glBindVertexArray(grid_vao);
			glGenBuffers(1, &grid_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindVertexArray(0);
		}

		~VisualizerImpl() {
			Dot::CleanupSphereMesh();
			glDeleteVertexArrays(1, &grid_vao);
			glDeleteBuffers(1, &grid_vbo);
			if (window)
				glfwDestroyWindow(window);
			glfwTerminate();
		}

		glm::mat4 SetupMatrices() {
			projection = glm::perspective(glm::radians(camera.fov), (float)width / (float)height, 0.1f, 1000.0f);
			glm::vec3 cameraPos(camera.x, camera.y, camera.z);
			glm::vec3 front;
			front.x = cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw));
			front.y = sin(glm::radians(camera.pitch));
			front.z = -cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw));
			front = glm::normalize(front);
			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));
			shader->use();
			shader->setMat4("projection", projection);
			shader->setMat4("view", view);
			return view;
		}

		void RenderGrid(const glm::mat4& view) {
			glDisable(GL_DEPTH_TEST);
			grid_shader->use();
			glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f));
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
			if (auto_camera_mode)
				return;
			float     camera_speed = 5.0f * delta_time;
			glm::vec3 front(
				cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw)),
				sin(glm::radians(camera.pitch)),
				-cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw))
			);
			glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
			if (keys[GLFW_KEY_W]) {
				camera.x += front.x * camera_speed;
				camera.y += front.y * camera_speed;
				camera.z += front.z * camera_speed;
			}
			if (keys[GLFW_KEY_S]) {
				camera.x -= front.x * camera_speed;
				camera.y -= front.y * camera_speed;
				camera.z -= front.z * camera_speed;
			}
			if (keys[GLFW_KEY_A]) {
				camera.x -= right.x * camera_speed;
				camera.z -= right.z * camera_speed;
			}
			if (keys[GLFW_KEY_D]) {
				camera.x += right.x * camera_speed;
				camera.z += right.z * camera_speed;
			}
			if (keys[GLFW_KEY_SPACE])
				camera.y += camera_speed;
			if (keys[GLFW_KEY_LEFT_SHIFT])
				camera.y -= camera_speed;
		}

		void CleanupOldTrails(float current_time, const std::vector<std::shared_ptr<Shape>>& active_shapes) {
			std::set<int> active_ids;
			for (const auto& shape : active_shapes) {
				active_ids.insert(shape->id);
			}

			auto trail_it = trails.begin();
			while (trail_it != trails.end()) {
				int trail_id = trail_it->first;
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

		void UpdateAutoCamera(float delta_time, const std::vector<std::shared_ptr<Shape>>& shapes) {
			if (!auto_camera_mode || shapes.empty()) {
				return;
			}

			auto_camera_time += delta_time;

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

			float extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
			float target_distance = extent * 2.0f + 5.0f;

			auto_camera_distance += (target_distance - auto_camera_distance) * delta_time * 0.5f;

			float height_cycle = sin(auto_camera_time * 0.3f) * 0.5f + 0.5f;
			float camera_height = mean_y + 1.0f + height_cycle * auto_camera_distance * 0.5f;

			float rotation_speed = 0.2f + 0.1f * sin(auto_camera_time * 0.15f);
			auto_camera_angle += rotation_speed * delta_time;

			float direction_modifier = sin(auto_camera_time * 0.08f) * 0.3f;
			float effective_angle = auto_camera_angle + direction_modifier;

			camera.x = mean_x + cos(effective_angle) * auto_camera_distance;
			camera.z = mean_z + sin(effective_angle) * auto_camera_distance;
			camera.y = camera_height;

			float dx = mean_x - camera.x;
			float dy = mean_y - camera.y;
			float dz = mean_z - camera.z;

			float distance_xz = sqrt(dx * dx + dz * dz);

			camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
			camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;
			camera.pitch = std::max(-89.0f, std::min(30.0f, camera.pitch));
		}

		void UpdateSingleTrackCamera(float delta_time, const std::vector<std::shared_ptr<Shape>>& shapes) {
			if (!single_track_mode || shapes.empty()) {
				return;
			}
			if (tracked_dot_index >= static_cast<int>(shapes.size())) {
				tracked_dot_index = 0;
			}

			const auto& target_dot = shapes[tracked_dot_index];
			float       target_x = target_dot->x;
			float       target_y = target_dot->y;
			float       target_z = target_dot->z;

			float distance = 15.0f;
			float camera_height_offset = 5.0f;
			float pan_speed = 2.0f * delta_time;

			camera.x += (target_x - camera.x) * pan_speed;
			camera.y += (target_y + camera_height_offset - camera.y) * pan_speed;
			camera.z += (target_z - distance - camera.z) * pan_speed;

			float dx = target_x - camera.x;
			float dy = target_y - camera.y;
			float dz = target_z - camera.z;
			float distance_xz = sqrt(dx * dx + dz * dz);

			camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
			camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;
			camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));
		}

		static void KeyCallback(GLFWwindow* w, int key, int sc, int action, int mods) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (key >= 0 && key < 1024) {
				if (action == GLFW_PRESS)
					impl->keys[key] = true;
				else if (action == GLFW_RELEASE)
					impl->keys[key] = false;
			}
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(w, true);
			if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
				impl->auto_camera_mode = !impl->auto_camera_mode;
				glfwSetInputMode(w, GLFW_CURSOR, impl->auto_camera_mode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
				if (!impl->auto_camera_mode)
					impl->first_mouse = true;
			}
			if (key == GLFW_KEY_9 && action == GLFW_PRESS)
				impl->single_track_mode = true;
			if (key == GLFW_KEY_8 && action == GLFW_PRESS)
				impl->single_track_mode = false;
		}

		static void MouseCallback(GLFWwindow* w, double xpos, double ypos) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (impl->auto_camera_mode)
				return;
			if (impl->first_mouse) {
				impl->last_mouse_x = xpos;
				impl->last_mouse_y = ypos;
				impl->first_mouse = false;
			}
			float xoffset = xpos - impl->last_mouse_x, yoffset = impl->last_mouse_y - ypos;
			impl->last_mouse_x = xpos;
			impl->last_mouse_y = ypos;
			xoffset *= 0.1f;
			yoffset *= 0.1f;
			impl->camera.yaw += xoffset;
			impl->camera.pitch += yoffset;
			if (impl->camera.pitch > 89.0f)
				impl->camera.pitch = 89.0f;
			if (impl->camera.pitch < -89.0f)
				impl->camera.pitch = -89.0f;
		}

		static void FramebufferSizeCallback(GLFWwindow* w, int width, int height) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			impl->width = width;
			impl->height = height;
			glViewport(0, 0, width, height);
		}
	};

	Visualizer::Visualizer(int w, int h, const char* t): impl(new VisualizerImpl(w, h, t)) {}

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
		float time = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - impl->start_time).count();

		std::vector<std::shared_ptr<Shape>> shapes;
		if (impl->shape_function)
			shapes = impl->shape_function(time);

		static auto last_frame_time = std::chrono::high_resolution_clock::now();
		auto        current_frame_time = std::chrono::high_resolution_clock::now();
		float       delta_time = std::chrono::duration<float>(current_frame_time - last_frame_time).count();
		last_frame_time = current_frame_time;

		if (impl->single_track_mode) {
			impl->UpdateSingleTrackCamera(delta_time, shapes);
		} else {
			impl->UpdateAutoCamera(delta_time, shapes);
		}

		glm::mat4 view = impl->SetupMatrices();
		impl->RenderGrid(view);

		impl->shader->use();
		impl->shader->setVec3("lightPos", 1.0f, 1.0f, 1.0f);
		impl->shader->setVec3("viewPos", impl->camera.x, impl->camera.y, impl->camera.z);
		impl->shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);

		if (!shapes.empty()) {
			impl->CleanupOldTrails(time, shapes);
			std::set<int> current_shape_ids;
			for (const auto& shape : shapes) {
				current_shape_ids.insert(shape->id);
				if (impl->trails.find(shape->id) == impl->trails.end()) {
					impl->trails[shape->id] = std::make_shared<Trail>(shape->trail_length);
				}
				impl->trails[shape->id]->AddPosition(shape->x, shape->y, shape->z);
				impl->trail_last_update[shape->id] = time;
				shape->render();
			}

			impl->trail_shader->use();
			impl->trail_shader->setMat4("view", view);
			impl->trail_shader->setMat4("projection", impl->projection);
			for (const auto& pair : impl->trails) {
				auto it = std::find_if(shapes.begin(), shapes.end(), [&](const auto& s) {
					return s->id == pair.first;
				});
				if (it != shapes.end()) {
					pair.second->Render(*impl->trail_shader, (*it)->r, (*it)->g, (*it)->b);
				} else {
					pair.second->Render(*impl->trail_shader, 0.7f, 0.7f, 0.7f);
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
