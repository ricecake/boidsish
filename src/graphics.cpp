#include "graphics.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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
	struct Visualizer::VisualizerImpl {
		GLFWwindow*                           window;
		int                                   width, height;
		Camera                                camera;
		std::vector<ShapeFunction>            shape_functions;
		std::map<int, std::shared_ptr<Trail>> trails;
		std::map<int, float>                  trail_last_update;

		std::shared_ptr<Shader> shader;
		std::unique_ptr<Shader> plane_shader;
		std::unique_ptr<Shader> sky_shader;
		std::unique_ptr<Shader> trail_shader;
		std::unique_ptr<Shader> blur_shader;
		GLuint                  plane_vao, plane_vbo, sky_vao, blur_quad_vao, blur_quad_vbo;
		GLuint                  reflection_fbo, reflection_texture, reflection_depth_rbo;
		GLuint                  pingpong_fbo[2], pingpong_texture[2];
		GLuint                  lighting_ubo;
		glm::mat4               projection, reflection_vp;

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
			plane_shader = std::make_unique<Shader>("shaders/plane.vert", "shaders/plane.frag");
			sky_shader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
			trail_shader = std::make_unique<Shader>("shaders/trail.vert", "shaders/trail.frag", "shaders/trail.geom");
			blur_shader = std::make_unique<Shader>("shaders/blur.vert", "shaders/blur.frag");

			glGenBuffers(1, &lighting_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
			// Allocate space for 3 vec3s with std140 padding (each vec3 padded to 16 bytes)
			glBufferData(GL_UNIFORM_BUFFER, 48, NULL, GL_STATIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			glBindBufferRange(GL_UNIFORM_BUFFER, 0, lighting_ubo, 0, 48);

			shader->use();
			glUniformBlockBinding(shader->ID, glGetUniformBlockIndex(shader->ID, "Lighting"), 0);
			plane_shader->use();
			glUniformBlockBinding(plane_shader->ID, glGetUniformBlockIndex(plane_shader->ID, "Lighting"), 0);
			trail_shader->use();
			glUniformBlockBinding(trail_shader->ID, glGetUniformBlockIndex(trail_shader->ID, "Lighting"), 0);

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
			glGenVertexArrays(1, &plane_vao);
			glBindVertexArray(plane_vao);
			glGenBuffers(1, &plane_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, plane_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindVertexArray(0);

			glGenVertexArrays(1, &sky_vao);

			float blur_quad_vertices[] = {
				// positions   // texCoords
				-1.0f,  1.0f,  0.0f, 1.0f,
				-1.0f, -1.0f,  0.0f, 0.0f,
				 1.0f, -1.0f,  1.0f, 0.0f,

				-1.0f,  1.0f,  0.0f, 1.0f,
				 1.0f, -1.0f,  1.0f, 0.0f,
				 1.0f,  1.0f,  1.0f, 1.0f
			};
			glGenVertexArrays(1, &blur_quad_vao);
			glBindVertexArray(blur_quad_vao);
			glGenBuffers(1, &blur_quad_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, blur_quad_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(blur_quad_vertices), blur_quad_vertices, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
			glEnableVertexAttribArray(1);
			glBindVertexArray(0);

			// --- Reflection Framebuffer ---
			glGenFramebuffers(1, &reflection_fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, reflection_fbo);

			// Color attachment
			glGenTextures(1, &reflection_texture);
			glBindTexture(GL_TEXTURE_2D, reflection_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflection_texture, 0);

			// Depth renderbuffer
			glGenRenderbuffers(1, &reflection_depth_rbo);
			glBindRenderbuffer(GL_RENDERBUFFER, reflection_depth_rbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflection_depth_rbo);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// --- Ping-pong Framebuffers for blurring ---
			glGenFramebuffers(2, pingpong_fbo);
			glGenTextures(2, pingpong_texture);
			for (unsigned int i = 0; i < 2; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo[i]);
				glBindTexture(GL_TEXTURE_2D, pingpong_texture[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_texture[i], 0);
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					std::cerr << "ERROR::FRAMEBUFFER:: Ping-pong Framebuffer is not complete!" << std::endl;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		~VisualizerImpl() {
			Dot::CleanupSphereMesh();
			glDeleteVertexArrays(1, &plane_vao);
			glDeleteBuffers(1, &plane_vbo);
			glDeleteVertexArrays(1, &sky_vao);
			glDeleteVertexArrays(1, &blur_quad_vao);
			glDeleteBuffers(1, &blur_quad_vbo);
			glDeleteFramebuffers(1, &reflection_fbo);
			glDeleteTextures(1, &reflection_texture);
			glDeleteRenderbuffers(1, &reflection_depth_rbo);
			glDeleteFramebuffers(2, pingpong_fbo);
			glDeleteTextures(2, pingpong_texture);
			if (window)
				glfwDestroyWindow(window);
			glfwTerminate();
		}

		glm::mat4 SetupMatrices(const Camera& cam_to_use) {
			projection = glm::perspective(glm::radians(cam_to_use.fov), (float)width / (float)height, 0.1f, 1000.0f);
			glm::vec3 cameraPos(cam_to_use.x, cam_to_use.y, cam_to_use.z);
			glm::vec3 front;
			front.x = cos(glm::radians(cam_to_use.pitch)) * sin(glm::radians(cam_to_use.yaw));
			front.y = sin(glm::radians(cam_to_use.pitch));
			front.z = -cos(glm::radians(cam_to_use.pitch)) * cos(glm::radians(cam_to_use.yaw));
			front = glm::normalize(front);
			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + front, glm::vec3(0.0f, 1.0f, 0.0f));
			shader->use();
			shader->setMat4("projection", projection);
			shader->setMat4("view", view);
			return view;
		}

		glm::mat4 SetupMatrices() { return SetupMatrices(camera); }

		void RenderSceneObjects(
			const glm::mat4&                               view,
			const Camera&                                  cam,
			const std::vector<std::shared_ptr<Shape>>&     shapes,
			float                                          time,
			const std::optional<glm::vec4>&                clip_plane
		) {
			shader->use();
			shader->setMat4("view", view);
			if (clip_plane) {
				shader->setVec4("clipPlane", *clip_plane);
			} else {
				shader->setVec4("clipPlane", glm::vec4(0, 0, 0, 0)); // No clipping
			}

			if (shapes.empty()) {
				return;
			}

			CleanupOldTrails(time, shapes);
			std::set<int> current_shape_ids;
			for (const auto& shape : shapes) {
				current_shape_ids.insert(shape->id);
				if (trails.find(shape->id) == trails.end()) {
					trails[shape->id] = std::make_shared<Trail>(shape->trail_length);
				}
				trails[shape->id]->AddPoint(glm::vec3(shape->x, shape->y, shape->z),
				                        	glm::vec3(shape->r, shape->g, shape->b));
				trail_last_update[shape->id] = time;
				shape->render();
			}

			trail_shader->use();
			trail_shader->setMat4("view", view);
			trail_shader->setMat4("projection", projection);
			for (const auto& pair : trails) {
				pair.second->Render(*trail_shader);
			}
		}

		void RenderSky(const glm::mat4& view) {
			glDisable(GL_DEPTH_TEST);
			sky_shader->use();
			sky_shader->setMat4("invProjection", glm::inverse(projection));
			sky_shader->setMat4("invView", glm::inverse(view));
			glBindVertexArray(sky_vao);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glBindVertexArray(0);
			glEnable(GL_DEPTH_TEST);
		}

		void RenderBlur(int amount) {
			glDisable(GL_DEPTH_TEST);
			blur_shader->use();
			bool horizontal = true, first_iteration = true;
			for (int i = 0; i < amount; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo[horizontal]);
				blur_shader->setInt("horizontal", horizontal);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, first_iteration ? reflection_texture : pingpong_texture[!horizontal]);
				glBindVertexArray(blur_quad_vao);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				horizontal = !horizontal;
				if (first_iteration) first_iteration = false;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glEnable(GL_DEPTH_TEST);
		}

		void RenderPlane(const glm::mat4& view) {
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			plane_shader->use();
			plane_shader->setInt("reflectionTexture", 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, pingpong_texture[0]);
			plane_shader->setMat4("reflectionViewProjection", reflection_vp);

			glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(500.0f));
			plane_shader->setMat4("model", model);
			plane_shader->setMat4("view", view);
			plane_shader->setMat4("projection", projection);
			glBindVertexArray(plane_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
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

			// --- Resize reflection framebuffer ---
			glBindTexture(GL_TEXTURE_2D, impl->reflection_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glBindRenderbuffer(GL_RENDERBUFFER, impl->reflection_depth_rbo);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

			// --- Resize ping-pong framebuffers ---
			for (unsigned int i = 0; i < 2; i++) {
				glBindTexture(GL_TEXTURE_2D, impl->pingpong_texture[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
			}
		}
	};

	Visualizer::Visualizer(int w, int h, const char* t): impl(new VisualizerImpl(w, h, t)) {}

	Visualizer::~Visualizer() {
		delete impl;
	}

	void Visualizer::AddShapeHandler(ShapeFunction func) {
		impl->shape_functions.push_back(func);
	}

	void Visualizer::ClearShapeHandlers() {
		impl->shape_functions.clear();
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
		if (!impl->shape_functions.empty()) {
			for (const auto& func : impl->shape_functions) {
				auto new_shapes = func(time);
				shapes.insert(shapes.end(), new_shapes.begin(), new_shapes.end());
			}
		}

		static auto last_frame_time = std::chrono::high_resolution_clock::now();
		auto        current_frame_time = std::chrono::high_resolution_clock::now();
		float       delta_time = std::chrono::duration<float>(current_frame_time - last_frame_time).count();
		last_frame_time = current_frame_time;

		if (impl->single_track_mode) {
			impl->UpdateSingleTrackCamera(delta_time, shapes);
		} else {
			impl->UpdateAutoCamera(delta_time, shapes);
		}

		glBindBuffer(GL_UNIFORM_BUFFER, impl->lighting_ubo);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec3), &glm::vec3(1.0f, 100.0f, 25.0f)[0]);
		glBufferSubData(GL_UNIFORM_BUFFER, 16, sizeof(glm::vec3), &glm::vec3(impl->camera.x, impl->camera.y, impl->camera.z)[0]);
		glBufferSubData(GL_UNIFORM_BUFFER, 32, sizeof(glm::vec3), &glm::vec3(1.0f, 1.0f, 1.0f)[0]);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		// --- Reflection Pass ---
		glEnable(GL_CLIP_DISTANCE0);
		{
			glBindFramebuffer(GL_FRAMEBUFFER, impl->reflection_fbo);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			Camera reflection_cam = impl->camera;
			reflection_cam.y = -reflection_cam.y;
			reflection_cam.pitch = -reflection_cam.pitch;

			glm::mat4 reflection_view = impl->SetupMatrices(reflection_cam);
			impl->reflection_vp = impl->projection * reflection_view;

			impl->RenderSky(reflection_view);
			impl->RenderSceneObjects(reflection_view, reflection_cam, shapes, time, glm::vec4(0, 1, 0, 0.01));
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		glDisable(GL_CLIP_DISTANCE0);

		impl->RenderBlur(10);

		// --- Main Pass ---
		glm::mat4 view = impl->SetupMatrices();
		impl->RenderSky(view);
		impl->RenderPlane(view);
		impl->RenderSceneObjects(view, impl->camera, shapes, time, std::nullopt);

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
