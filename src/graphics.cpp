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
	constexpr float kMinCameraHeight = 0.1f;

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

		bool                                           paused = false;
		float                                          simulation_time = 0.0f;
		std::chrono::high_resolution_clock::time_point last_frame;

		bool  auto_camera_mode = true;
		float auto_camera_time = 0.0f;
		float auto_camera_angle = 0.0f;
		float auto_camera_height_offset = 0.0f;
		float auto_camera_distance = 10.0f;
		glm::vec3 auto_camera_target = glm::vec3(0.0f);
		float auto_camera_target_update_time = 0.0f;

		bool  single_track_mode = false;
		int   tracked_dot_index = 0;
		float single_track_orbit_yaw = 0.0f;
		float single_track_orbit_pitch = 20.0f;
		float single_track_distance = 15.0f;

		VisualizerImpl(int w, int h, const char* title): width(w), height(h) {
			last_frame = std::chrono::high_resolution_clock::now();
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
			trail_shader = std::make_unique<Shader>("shaders/trail.vert", "shaders/trail.frag");
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

			Shape::InitSphereMesh();

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

			float blur_quad_vertices[] = {// positions   // texCoords
			                              -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

			                              -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f
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
			Shape::DestroySphereMesh();
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
			const glm::mat4& view,
			const Camera& /* cam */,
			const std::vector<std::shared_ptr<Shape>>& shapes,
			float                                      time,
			const std::optional<glm::vec4>&            clip_plane
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
				current_shape_ids.insert(shape->GetId());
				// Only create trails for shapes with trail_length > 0
				if (shape->GetTrailLength() > 0 && !paused) {
					if (trails.find(shape->GetId()) == trails.end()) {
						trails[shape->GetId()] = std::make_shared<Trail>(shape->GetTrailLength());
					}
					trails[shape->GetId()]->AddPoint(
						glm::vec3(shape->GetX(), shape->GetY(), shape->GetZ()),
						glm::vec3(shape->GetR(), shape->GetG(), shape->GetB())
					);
					trail_last_update[shape->GetId()] = time;
				}
				shape->render();
			}

			trail_shader->use();
			trail_shader->setMat4("view", view);
			trail_shader->setMat4("projection", projection);
			glm::mat4 model = glm::mat4(1.0f);
			trail_shader->setMat4("model", model);
			if (clip_plane) {
				trail_shader->setVec4("clipPlane", *clip_plane);
			} else {
				trail_shader->setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
			}
			for (auto& pair : trails) {
				pair.second->Render(*trail_shader);
			}
		}

		void RenderSky(const glm::mat4& invProjection, const glm::mat4& invView) {
			glDisable(GL_DEPTH_TEST);
			sky_shader->use();
			sky_shader->setMat4("invProjection", invProjection);
			sky_shader->setMat4("invView", invView);
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
				if (first_iteration)
					first_iteration = false;
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
			if (auto_camera_mode || single_track_mode)
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
			if (camera.y < kMinCameraHeight)
				camera.y = kMinCameraHeight;
		}

		void CleanupOldTrails(float current_time, const std::vector<std::shared_ptr<Shape>>& active_shapes) {
			std::set<int> active_ids;
			for (const auto& shape : active_shapes) {
				active_ids.insert(shape->GetId());
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
			auto_camera_target_update_time += delta_time;

			if (auto_camera_target_update_time > 0.1f) {
				auto_camera_target_update_time = 0.0f;
				float mean_x = 0.0f, mean_y = 0.0f, mean_z = 0.0f;
				float min_x = shapes[0]->GetX(), max_x = shapes[0]->GetX();
				float min_y = shapes[0]->GetY(), max_y = shapes[0]->GetY();
				float min_z = shapes[0]->GetZ(), max_z = shapes[0]->GetZ();

				for (const auto& shape : shapes) {
					mean_x += shape->GetX();
					mean_y += shape->GetY();
					mean_z += shape->GetZ();
					min_x = std::min(min_x, shape->GetX());
					max_x = std::max(max_x, shape->GetX());
					min_y = std::min(min_y, shape->GetY());
					max_y = std::max(max_y, shape->GetY());
					min_z = std::min(min_z, shape->GetZ());
					max_z = std::max(max_z, shape->GetZ());
				}
				auto_camera_target.x = mean_x / shapes.size();
				auto_camera_target.y = mean_y / shapes.size();
				auto_camera_target.z = mean_z / shapes.size();

				float extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
				float target_distance = extent * 2.0f + 5.0f;

				auto_camera_distance += (target_distance - auto_camera_distance) * delta_time * 0.5f;
			}

			float height_cycle = sin(auto_camera_time * 0.3f) * 0.5f + 0.5f;
			float camera_height = auto_camera_target.y + 1.0f + height_cycle * auto_camera_distance * 0.5f;

			float rotation_speed = 0.2f + 0.1f * sin(auto_camera_time * 0.15f);
			auto_camera_angle += rotation_speed * delta_time;

			float direction_modifier = sin(auto_camera_time * 0.08f) * 0.3f;
			float effective_angle = auto_camera_angle + direction_modifier;

			glm::vec3 target_camera_pos(
				auto_camera_target.x + cos(effective_angle) * auto_camera_distance,
				camera_height,
				auto_camera_target.z + sin(effective_angle) * auto_camera_distance
			);

			glm::vec3 current_camera_pos(camera.x, camera.y, camera.z);
			glm::vec3 new_pos = current_camera_pos + (target_camera_pos - current_camera_pos) * (delta_time * 1.0f);
			camera.x = new_pos.x;
			camera.y = new_pos.y;
			camera.z = new_pos.z;

			if (camera.y < kMinCameraHeight)
				camera.y = kMinCameraHeight;

			float dx = auto_camera_target.x - camera.x;
			float dy = auto_camera_target.y - camera.y;
			float dz = auto_camera_target.z - camera.z;

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

			glm::vec3 target_pos(target_dot->GetX(), target_dot->GetY(), target_dot->GetZ());

			float yaw_rad = glm::radians(single_track_orbit_yaw);
			float pitch_rad = glm::radians(single_track_orbit_pitch);

			glm::vec3 offset;
			offset.x = single_track_distance * cos(pitch_rad) * sin(yaw_rad);
			offset.y = single_track_distance * sin(pitch_rad);
			offset.z = -single_track_distance * cos(pitch_rad) * cos(yaw_rad);

			glm::vec3 camera_pos = target_pos - offset;

			camera.x = camera_pos.x;
			camera.y = camera_pos.y;
			camera.z = camera_pos.z;

			if (camera.y < kMinCameraHeight)
				camera.y = kMinCameraHeight;

			glm::vec3 front = glm::normalize(target_pos - camera_pos);

			camera.yaw = glm::degrees(atan2(front.x, -front.z));
			camera.pitch = glm::degrees(asin(front.y));
			camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));
		}

		static void KeyCallback(GLFWwindow* w, int key, int /* sc */, int action, int /* mods */) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (key >= 0 && key < 1024) {
				if (action == GLFW_PRESS)
					impl->keys[key] = true;
				else if (action == GLFW_RELEASE)
					impl->keys[key] = false;
			}
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(w, true);
			if (action == GLFW_PRESS) {
				if (key == GLFW_KEY_0) {
					impl->single_track_mode = false;
					impl->auto_camera_mode = !impl->auto_camera_mode;
					glfwSetInputMode(
						w,
						GLFW_CURSOR,
						impl->auto_camera_mode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED
					);
					if (!impl->auto_camera_mode)
						impl->first_mouse = true;
				} else if (key == GLFW_KEY_9) {
					if (impl->single_track_mode) {
						impl->tracked_dot_index++;
					} else {
						impl->auto_camera_mode = false;
						impl->single_track_mode = true;
						glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
						impl->first_mouse = true;
					}
				} else if (key == GLFW_KEY_8) {
					impl->single_track_mode = false;
					impl->auto_camera_mode = true;
					glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				} else if (key == GLFW_KEY_EQUAL) {
					impl->single_track_distance -= 0.5f;
					if (impl->single_track_distance < 1.0f)
						impl->single_track_distance = 1.0f;
				} else if (key == GLFW_KEY_MINUS) {
					impl->single_track_distance += 0.5f;
				}
			}
			if (key == GLFW_KEY_P && action == GLFW_PRESS)
				impl->paused = !impl->paused;
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

			float xoffset = xpos - impl->last_mouse_x;
			float yoffset = impl->last_mouse_y - ypos;
			impl->last_mouse_x = xpos;
			impl->last_mouse_y = ypos;

			float sensitivity = 0.1f;
			xoffset *= sensitivity;
			yoffset *= sensitivity;

			if (impl->single_track_mode) {
				impl->single_track_orbit_yaw += xoffset;
				impl->single_track_orbit_pitch += yoffset;

				if (impl->single_track_orbit_pitch > 89.0f)
					impl->single_track_orbit_pitch = 89.0f;
				if (impl->single_track_orbit_pitch < -89.0f)
					impl->single_track_orbit_pitch = -89.0f;
			} else {
				impl->camera.yaw += xoffset;
				impl->camera.pitch += yoffset;

				if (impl->camera.pitch > 89.0f)
					impl->camera.pitch = 89.0f;
				if (impl->camera.pitch < -89.0f)
					impl->camera.pitch = -89.0f;
			}
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
		auto  current_frame = std::chrono::high_resolution_clock::now();
		float delta_time = std::chrono::duration<float>(current_frame - impl->last_frame).count();
		impl->last_frame = current_frame;

		if (!impl->paused) {
			impl->simulation_time += delta_time;
		}
		impl->ProcessInput(delta_time);
	}

	void Visualizer::Render() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		std::vector<std::shared_ptr<Shape>> shapes;
		if (!impl->shape_functions.empty()) {
			for (const auto& func : impl->shape_functions) {
				auto new_shapes = func(impl->simulation_time);
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
		glBufferSubData(
			GL_UNIFORM_BUFFER,
			16,
			sizeof(glm::vec3),
			&glm::vec3(impl->camera.x, impl->camera.y, impl->camera.z)[0]
		);
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

			glm::mat4 inv_reflection_proj = glm::inverse(impl->projection);
			glm::mat4 inv_reflection_view = glm::inverse(reflection_view);
			impl->RenderSky(inv_reflection_proj, inv_reflection_view);
			impl->RenderSceneObjects(
				reflection_view,
				reflection_cam,
				shapes,
				impl->simulation_time,
				glm::vec4(0, 1, 0, 0.01)
			);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		glDisable(GL_CLIP_DISTANCE0);

		impl->RenderBlur(4);

		// --- Main Pass ---
		glm::mat4 view = impl->SetupMatrices();
		glm::mat4 inv_proj = glm::inverse(impl->projection);
		glm::mat4 inv_view = glm::inverse(view);
		impl->RenderSky(inv_proj, inv_view);
		impl->RenderPlane(view);
		impl->RenderSceneObjects(view, impl->camera, shapes, impl->simulation_time, std::nullopt);

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
