#include "graphics.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>
#include <algorithm>

#include "Config.h"
#include "ResourceManager.h"
#include "SkyboxManager.h"
#include "FloorManager.h"
#include "UIManager.h"
#include "dot.h"
#include "logger.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/GlitchEffect.h"
#include "post_processing/effects/NegativeEffect.h"
#include "post_processing/effects/ColorShiftEffect.h"
#include "post_processing/effects/BlackAndWhiteEffect.h"
#include "post_processing/effects/ShimmeryEffect.h"
#include "post_processing/effects/WireframeEffect.h"
#include "task_thread_pool.hpp"
#include "terrain.h"
#include "terrain_generator.h"
#include "trail.h"
#include "ui/PostProcessingWidget.h"
#include "visual_effects.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <shader.h>

namespace Boidsish {
	constexpr float kMinCameraHeight = 0.1f;
	constexpr float kCameraRollSpeed = 45.0f; // degrees per second
	constexpr float kCameraSpeedStep = 2.5f;
	constexpr float kMinCameraSpeed = 0.5f;
	constexpr int   kBlurPasses = 4;

	struct Visualizer::VisualizerImpl {
		Visualizer*                           parent;
		GLFWwindow*                           window;
		int                                   width, height;
		Camera                                camera;
		std::vector<ShapeFunction>            shape_functions;
		std::vector<std::shared_ptr<Shape>>   shapes;
		std::map<int, std::shared_ptr<Trail>> trails;
		std::map<int, float>                  trail_last_update;

		InputState                                             input_state{};
		InputCallback                                          input_callback;
		std::unique_ptr<UI::UIManager>                         ui_manager;
		std::unique_ptr<PostProcessing::PostProcessingManager> post_processing_manager_;
		int                                                    exit_key;

		Config config;

		std::unique_ptr<TerrainGenerator> terrain_generator;

		std::shared_ptr<Shader> shader;
		std::unique_ptr<Shader> trail_shader;
		std::unique_ptr<Shader> postprocess_shader_;

		std::unique_ptr<ResourceManager> resource_manager_;
		std::unique_ptr<SkyboxManager> skybox_manager_;
		std::unique_ptr<FloorManager> floor_manager_;

		GLuint                  main_fbo_, main_fbo_texture_, main_fbo_rbo_;
		glm::mat4               projection, reflection_vp;

		double last_mouse_x = 0.0, last_mouse_y = 0.0;
		bool   first_mouse = true;

		bool                                           paused = false;
		float                                          simulation_time = 0.0f;
		float                                          ripple_strength = 0.0f;
		std::chrono::high_resolution_clock::time_point last_frame;

		CameraMode camera_mode = CameraMode::AUTO;
		float      auto_camera_time = 0.0f;
		float      auto_camera_angle = 0.0f;
		float      auto_camera_height_offset = 0.0f;
		float      auto_camera_distance = 10.0f;

		int   tracked_dot_index = 0;
		float single_track_orbit_yaw = 0.0f;
		float single_track_orbit_pitch = 20.0f;
		float single_track_distance = 15.0f;

		bool is_fullscreen_ = false;
		int  windowed_xpos_, windowed_ypos_, windowed_width_, windowed_height_;

		// Adaptive Tessellation
		glm::vec3 last_camera_pos_{0.0f, 0.0f, 0.0f};
		float     camera_velocity_{0.0f};
		float     avg_frame_time_{1.0f / 60.0f};
		float     tess_quality_multiplier_{1.0f};

		// Config-driven feature flags
		bool effects_enabled_;
		bool terrain_enabled_;
		bool floor_enabled_;

		task_thread_pool::task_thread_pool thread_pool;

		VisualizerImpl(Visualizer* p, int w, int h, const char* title):
			parent(p), width(w), height(h), config("boidsish.ini") {
			config.Load();
			width = config.GetInt("window_width", w);
			height = config.GetInt("window_height", h);
			effects_enabled_ = config.GetBool("enable_effects", true);
			terrain_enabled_ = config.GetBool("enable_terrain", true);
			floor_enabled_ = config.GetBool("enable_floor", true);
			is_fullscreen_ = config.GetBool("fullscreen", false);

			exit_key = GLFW_KEY_ESCAPE;
			input_callback = [this](const InputState& state) { this->DefaultInputHandler(state); };

			last_frame = std::chrono::high_resolution_clock::now();
			if (!glfwInit())
				throw std::runtime_error("Failed to initialize GLFW");

			glfwSetErrorCallback([](int error, const char* description) {
				std::cerr << "GLFW Error " << error << ": " << description << std::endl;
			});

			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
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

			glfwWindowHint(GLFW_SAMPLES, 4);
			glfwSetWindowUserPointer(window, this);
			glfwSetKeyCallback(window, KeyCallback);
			glfwSetCursorPosCallback(window, MouseCallback);
			glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glFrontFace(GL_CCW);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glEnable(GL_MULTISAMPLE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			resource_manager_ = std::make_unique<ResourceManager>();
			shader = std::make_shared<Shader>("shaders/vis.vert", "shaders/vis.frag");
			Shape::shader = shader;
			trail_shader = std::make_unique<Shader>("shaders/trail.vert", "shaders/trail.frag");
			if (floor_enabled_) {
				floor_manager_ = std::make_unique<FloorManager>(width, height);
				skybox_manager_ = std::make_unique<SkyboxManager>();
			}
			if (effects_enabled_) {
				postprocess_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
			}
			if (terrain_enabled_) {
				terrain_generator = std::make_unique<TerrainGenerator>(*resource_manager_);
			}

			resource_manager_->BindUBOs(*shader);
			resource_manager_->BindUBOs(*trail_shader);

			ui_manager = std::make_unique<UI::UIManager>(window);

			Shape::InitSphereMesh();

			// --- Main Scene Framebuffer ---
			glGenFramebuffers(1, &main_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, main_fbo_);

			// Color attachment
			glGenTextures(1, &main_fbo_texture_);
			glBindTexture(GL_TEXTURE_2D, main_fbo_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, main_fbo_texture_, 0);

			// Depth and stencil renderbuffer
			glGenRenderbuffers(1, &main_fbo_rbo_);
			glBindRenderbuffer(GL_RENDERBUFFER, main_fbo_rbo_);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, main_fbo_rbo_);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "ERROR::FRAMEBUFFER:: Main framebuffer is not complete!" << std::endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			if (effects_enabled_) {
				post_processing_manager_ = std::make_unique<PostProcessing::PostProcessingManager>(width, height);
				post_processing_manager_->Initialize();

				auto negative_effect = std::make_shared<PostProcessing::NegativeEffect>();
				negative_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(negative_effect);

				auto glitch_effect = std::make_shared<PostProcessing::GlitchEffect>();
				glitch_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(glitch_effect);

				auto color_shift_effect = std::make_shared<PostProcessing::ColorShiftEffect>();
				color_shift_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(color_shift_effect);

				auto black_and_white_effect = std::make_shared<PostProcessing::BlackAndWhiteEffect>();
				black_and_white_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(black_and_white_effect);

				auto shimmery_effect = std::make_shared<PostProcessing::ShimmeryEffect>();
				shimmery_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(shimmery_effect);

				auto wireframe_effect = std::make_shared<PostProcessing::WireframeEffect>();
				wireframe_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(wireframe_effect);


				// --- UI ---
				auto post_processing_widget = std::make_shared<UI::PostProcessingWidget>(*post_processing_manager_);
				ui_manager->AddWidget(post_processing_widget);
			}
		}

		~VisualizerImpl() {
			config.SetInt("window_width", width);
			config.SetInt("window_height", height);
			config.SetBool("fullscreen", is_fullscreen_);
			config.Save();

			// Explicitly reset UI manager before destroying window context
			ui_manager.reset();

			Shape::DestroySphereMesh();
			glDeleteFramebuffers(1, &main_fbo_);
			glDeleteTextures(1, &main_fbo_texture_);
			glDeleteRenderbuffers(1, &main_fbo_rbo_);

			if (window)
				glfwDestroyWindow(window);
			glfwTerminate();
		}

		std::shared_ptr<PostProcessing::IPostProcessingEffect> GetEffectByName(const std::string& name) {
			auto& effects = post_processing_manager_->GetEffects();
			auto it = std::find_if(effects.begin(), effects.end(), [&](const auto& effect) {
				return effect->GetName() == name;
			});
			if (it != effects.end()) {
				return *it;
			}
			return nullptr;
		}


		Frustum CalculateFrustum(const glm::mat4& view, const glm::mat4& projection) {
			Frustum   frustum;
			glm::mat4 vp = projection * view;

			frustum.planes[0].normal.x = vp[0][3] + vp[0][0];
			frustum.planes[0].normal.y = vp[1][3] + vp[1][0];
			frustum.planes[0].normal.z = vp[2][3] + vp[2][0];
			frustum.planes[0].distance = vp[3][3] + vp[3][0];

			frustum.planes[1].normal.x = vp[0][3] - vp[0][0];
			frustum.planes[1].normal.y = vp[1][3] - vp[1][0];
			frustum.planes[1].normal.z = vp[2][3] - vp[2][0];
			frustum.planes[1].distance = vp[3][3] - vp[3][0];

			frustum.planes[2].normal.x = vp[0][3] + vp[0][1];
			frustum.planes[2].normal.y = vp[1][3] + vp[1][1];
			frustum.planes[2].normal.z = vp[2][3] + vp[2][1];
			frustum.planes[2].distance = vp[3][3] + vp[3][1];

			frustum.planes[3].normal.x = vp[0][3] - vp[0][1];
			frustum.planes[3].normal.y = vp[1][3] - vp[1][1];
			frustum.planes[3].normal.z = vp[2][3] - vp[2][1];
			frustum.planes[3].distance = vp[3][3] - vp[3][1];

			frustum.planes[4].normal.x = vp[0][3] + vp[0][2];
			frustum.planes[4].normal.y = vp[1][3] + vp[1][2];
			frustum.planes[4].normal.z = vp[2][3] + vp[2][2];
			frustum.planes[4].distance = vp[3][3] + vp[3][2];

			frustum.planes[5].normal.x = vp[0][3] - vp[0][2];
			frustum.planes[5].normal.y = vp[1][3] - vp[1][2];
			frustum.planes[5].normal.z = vp[2][3] - vp[2][2];
			frustum.planes[5].distance = vp[3][3] - vp[3][2];

			for (int i = 0; i < 6; ++i) {
				float length = glm::length(frustum.planes[i].normal);
				frustum.planes[i].normal /= length;
				frustum.planes[i].distance /= length;
			}

			return frustum;
		}

		glm::mat4 SetupMatrices(const Camera& cam_to_use) {
			projection = glm::perspective(glm::radians(cam_to_use.fov), (float)width / (float)height, 0.1f, 1000.0f);
			glm::vec3 cameraPos(cam_to_use.x, cam_to_use.y, cam_to_use.z);

			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cam_to_use.front(), cam_to_use.up());
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
			shader->setFloat("ripple_strength", ripple_strength);
			shader->setMat4("view", view);
			if (clip_plane) {
				shader->setVec4("clipPlane", *clip_plane);
			} else {
				shader->setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
			}

			if (shapes.empty()) {
				return;
			}

			CleanupOldTrails(time, shapes);
			std::set<int> current_shape_ids;
			for (const auto& shape : shapes) {
				current_shape_ids.insert(shape->GetId());
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

		void RenderTerrain(const glm::mat4& view, const std::optional<glm::vec4>& clip_plane) {
			if (!terrain_enabled_)
				return;

			terrain_generator->Render(view, projection, clip_plane);
		}

		void DefaultInputHandler(const InputState& state) {
			if (camera_mode == CameraMode::FREE) {
				float     camera_speed_val = camera.speed * state.delta_time;
				glm::vec3 front(
					cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw)),
					sin(glm::radians(camera.pitch)),
					-cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw))
				);
				glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

				if (state.keys[GLFW_KEY_W]) {
					camera.x += front.x * camera_speed_val;
					camera.y += front.y * camera_speed_val;
					camera.z += front.z * camera_speed_val;
				}
				if (state.keys[GLFW_KEY_S]) {
					camera.x -= front.x * camera_speed_val;
					camera.y -= front.y * camera_speed_val;
					camera.z -= front.z * camera_speed_val;
				}
				if (state.keys[GLFW_KEY_A]) {
					camera.x -= right.x * camera_speed_val;
					camera.z -= right.z * camera_speed_val;
				}
				if (state.keys[GLFW_KEY_D]) {
					camera.x += right.x * camera_speed_val;
					camera.z += right.z * camera_speed_val;
				}
				if (state.keys[GLFW_KEY_SPACE])
					camera.y += camera_speed_val;
				if (state.keys[GLFW_KEY_LEFT_SHIFT])
					camera.y -= camera_speed_val;
				if (state.keys[GLFW_KEY_Q])
					camera.roll -= kCameraRollSpeed * state.delta_time;
				if (state.keys[GLFW_KEY_E])
					camera.roll += kCameraRollSpeed * state.delta_time;
				if (camera.y < kMinCameraHeight)
					camera.y = kMinCameraHeight;

				float sensitivity = 1.f;
				float xoffset = state.mouse_delta_x * sensitivity;
				float yoffset = state.mouse_delta_y * sensitivity;

				camera.yaw += xoffset;
				camera.pitch += yoffset;

				if (camera.pitch > 89.0f)
					camera.pitch = 89.0f;
				if (camera.pitch < -89.0f)
					camera.pitch = -89.0f;
			}

			if (camera_mode == CameraMode::TRACKING) {
				float sensitivity = 0.1f;
				float xoffset = state.mouse_delta_x * sensitivity;
				float yoffset = state.mouse_delta_y * sensitivity;

				single_track_orbit_yaw += xoffset;
				single_track_orbit_pitch += yoffset;

				if (single_track_orbit_pitch > 89.0f)
					single_track_orbit_pitch = 89.0f;
				if (single_track_orbit_pitch < -89.0f)
					single_track_orbit_pitch = -89.0f;
			}

			if (state.key_down[GLFW_KEY_0]) {
				parent->SetCameraMode(CameraMode::FREE);
			} else if (state.key_down[GLFW_KEY_9]) {
				if (camera_mode == CameraMode::TRACKING) {
					tracked_dot_index++;
				} else {
					parent->SetCameraMode(CameraMode::TRACKING);
				}
			} else if (state.key_down[GLFW_KEY_8]) {
				parent->SetCameraMode(CameraMode::AUTO);
			} else if (state.key_down[GLFW_KEY_7]) {
				parent->SetCameraMode(CameraMode::STATIONARY);
			}

			if (state.keys[GLFW_KEY_EQUAL]) {
				single_track_distance -= 0.5f;
				if (single_track_distance < 1.0f)
					single_track_distance = 1.0f;
			}
			if (state.keys[GLFW_KEY_MINUS]) {
				single_track_distance += 0.5f;
			}

			if (state.key_down[GLFW_KEY_LEFT_BRACKET]) {
				camera.speed -= kCameraSpeedStep;
				if (camera.speed < kMinCameraSpeed)
					camera.speed = kMinCameraSpeed;
			}
			if (state.key_down[GLFW_KEY_RIGHT_BRACKET]) {
				camera.speed += kCameraSpeedStep;
			}

			if (state.key_down[GLFW_KEY_P])
				paused = !paused;
			if (effects_enabled_) {
				if (state.key_down[GLFW_KEY_R])
					ripple_strength = (ripple_strength > 0.0f) ? 0.0f : 0.05f;
				if (state.key_down[GLFW_KEY_C]){
					auto effect = GetEffectByName("Color Shift");
					if (effect) effect->SetEnabled(!effect->IsEnabled());
				}
				if (state.key_down[GLFW_KEY_1]){
					auto effect = GetEffectByName("Black and White");
					if (effect) effect->SetEnabled(!effect->IsEnabled());
				}
				if (state.key_down[GLFW_KEY_2]){
					auto effect = GetEffectByName("Negative");
					if (effect) effect->SetEnabled(!effect->IsEnabled());
				}
				if (state.key_down[GLFW_KEY_3]){
					auto effect = GetEffectByName("Shimmery");
					if (effect) effect->SetEnabled(!effect->IsEnabled());
				}
				if (state.key_down[GLFW_KEY_4]){
					auto effect = GetEffectByName("Glitch");
					if (effect) effect->SetEnabled(!effect->IsEnabled());
				}
				if (state.key_down[GLFW_KEY_5]){
					auto effect = GetEffectByName("Wireframe");
					if (effect) effect->SetEnabled(!effect->IsEnabled());
				}
			}

			if (state.key_down[GLFW_KEY_F11]) {
				ToggleFullscreen();
			}
		}

		void ToggleFullscreen() {
			is_fullscreen_ = !is_fullscreen_;
			if (is_fullscreen_) {
				glfwGetWindowPos(window, &windowed_xpos_, &windowed_ypos_);
				glfwGetWindowSize(window, &windowed_width_, &windowed_height_);
				GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			} else {
				glfwSetWindowMonitor(
					window,
					nullptr,
					windowed_xpos_,
					windowed_ypos_,
					windowed_width_,
					windowed_height_,
					0
				);
			}
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
					    (current_time - update_it->second) > 2.0f) {
						trail_it = trails.erase(trail_it);
						trail_last_update.erase(trail_id);
						continue;
					}
				}
				++trail_it;
			}
		}

		void UpdateAutoCamera(float delta_time, const std::vector<std::shared_ptr<Shape>>& shapes) {
			if (camera_mode != CameraMode::AUTO || shapes.empty()) {
				return;
			}

			auto_camera_time += delta_time;

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

			glm::vec3 target_camera_pos(
				mean_x + cos(effective_angle) * auto_camera_distance,
				camera_height,
				mean_z + sin(effective_angle) * auto_camera_distance
			);

			glm::vec3 current_camera_pos(camera.x, camera.y, camera.z);
			glm::vec3 new_pos = current_camera_pos + (target_camera_pos - current_camera_pos) * (delta_time * 1.0f);
			camera.x = new_pos.x;
			camera.y = new_pos.y;
			camera.z = new_pos.z;

			if (camera.y < kMinCameraHeight)
				camera.y = kMinCameraHeight;

			float dx = mean_x - camera.x;
			float dy = mean_y - camera.y;
			float dz = mean_z - camera.z;

			float distance_xz = sqrt(dx * dx + dz * dz);

			camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
			camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;
			camera.pitch = std::max(-89.0f, std::min(30.0f, camera.pitch));
		}

		void UpdateSingleTrackCamera(float delta_time, const std::vector<std::shared_ptr<Shape>>& shapes) {
			if (camera_mode != CameraMode::TRACKING || shapes.empty()) {
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

		static void KeyCallback(GLFWwindow* w, int key, int, int action, int) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (key == impl->exit_key && action == GLFW_PRESS) {
				glfwSetWindowShouldClose(w, true);
				return;
			}

			if (key >= 0 && key < kMaxKeys) {
				if (action == GLFW_PRESS) {
					impl->input_state.keys[key] = true;
					impl->input_state.key_down[key] = true;
				} else if (action == GLFW_RELEASE) {
					impl->input_state.keys[key] = false;
					impl->input_state.key_up[key] = true;
				}
			}
		}

		static void MouseCallback(GLFWwindow* w, double xpos, double ypos) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (impl->first_mouse) {
				impl->last_mouse_x = xpos;
				impl->last_mouse_y = ypos;
				impl->first_mouse = false;
			}

			impl->input_state.mouse_delta_x = xpos - impl->last_mouse_x;
			impl->input_state.mouse_delta_y = impl->last_mouse_y - ypos;
			impl->last_mouse_x = xpos;
			impl->last_mouse_y = ypos;
			impl->input_state.mouse_x = xpos;
			impl->input_state.mouse_y = ypos;
		}

		static void FramebufferSizeCallback(GLFWwindow* w, int width, int height) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			impl->width = width;
			impl->height = height;
			glViewport(0, 0, width, height);

			if (impl->floor_enabled_) {
				impl->floor_manager_->Resize(width, height);
			}

			glBindTexture(GL_TEXTURE_2D, impl->main_fbo_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glBindRenderbuffer(GL_RENDERBUFFER, impl->main_fbo_rbo_);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

			if (impl->effects_enabled_ && impl->post_processing_manager_) {
				impl->post_processing_manager_->Resize(width, height);
			}
		}
	};

	Visualizer::Visualizer(int w, int h, const char* t): impl(new VisualizerImpl(this, w, h, t)) {}

	Visualizer::~Visualizer() = default;

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
		std::fill_n(impl->input_state.key_down, kMaxKeys, false);
		std::fill_n(impl->input_state.key_up, kMaxKeys, false);
		impl->input_state.mouse_delta_x = 0;
		impl->input_state.mouse_delta_y = 0;

		glfwPollEvents();
		auto  current_frame = std::chrono::high_resolution_clock::now();
		float delta_time = std::chrono::duration<float>(current_frame - impl->last_frame).count();
		impl->last_frame = current_frame;

		impl->input_state.delta_time = delta_time;

		if (impl->input_callback) {
			impl->input_callback(impl->input_state);
		}

		if (!impl->paused) {
			impl->simulation_time += delta_time;
		}

		// --- Adaptive Tessellation Logic ---
		glm::vec3 current_camera_pos(impl->camera.x, impl->camera.y, impl->camera.z);
		if (delta_time > 0.0f) {
			impl->camera_velocity_ = glm::distance(current_camera_pos, impl->last_camera_pos_) / delta_time;
		}
		impl->last_camera_pos_ = current_camera_pos;

		// Simple moving average for frame time
		impl->avg_frame_time_ = impl->avg_frame_time_ * 0.95f + delta_time * 0.05f;

		// Determine target quality
		float target_quality = 1.0f;
		if (impl->avg_frame_time_ > 1.0f / 30.0f) { // Framerate drops below 30 FPS
			target_quality = 0.4f;
		} else if (impl->avg_frame_time_ > 1.0f / 45.0f) { // Framerate drops below 45 FPS
			target_quality = 0.7f;
		} else if (impl->camera_velocity_ > 25.0f) { // High camera speed
			target_quality = 0.6f;
		}

		// Dampened adjustment
		if (impl->tess_quality_multiplier_ < target_quality) {
			// Linear increase
			impl->tess_quality_multiplier_ += 0.5f * delta_time;
			if (impl->tess_quality_multiplier_ > target_quality) {
				impl->tess_quality_multiplier_ = target_quality;
			}
		} else if (impl->tess_quality_multiplier_ > target_quality) {
			// Exponential decrease
			impl->tess_quality_multiplier_ = glm::mix(
				impl->tess_quality_multiplier_,
				target_quality,
				1.0f - exp(-delta_time * 5.0f)
			);
		}
	}

	void Visualizer::Render() {
		impl->shapes.clear();
		if (!impl->shape_functions.empty()) {
			for (const auto& func : impl->shape_functions) {
				auto new_shapes = func(impl->simulation_time);
				impl->shapes.insert(impl->shapes.end(), new_shapes.begin(), new_shapes.end());
			}
		}

		glm::mat4 view_matrix = impl->SetupMatrices();
		if (impl->terrain_enabled_) {
			Frustum frustum = impl->CalculateFrustum(view_matrix, impl->projection);
			impl->terrain_generator->update(frustum, impl->camera);
		}

		if (impl->camera_mode == CameraMode::TRACKING) {
			impl->UpdateSingleTrackCamera(impl->input_state.delta_time, impl->shapes);
		} else if (impl->camera_mode == CameraMode::AUTO) {
			impl->UpdateAutoCamera(impl->input_state.delta_time, impl->shapes);
		}

		if (impl->effects_enabled_) {
			VisualEffectsUbo ubo_data = {};
			for (const auto& shape : impl->shapes) {
				for (const auto& effect : shape->GetActiveEffects()) {
					if (effect == VisualEffect::RIPPLE) {
						ubo_data.ripple_enabled = 1;
					}
				}
			}
			impl->resource_manager_->UpdateVisualEffectsUBO(ubo_data);
		}

		float     light_x = 50.0f * cos(impl->simulation_time * 0.05f);
		float     light_y = 25.0f + 1.8 * abs(sin(impl->simulation_time * 0.01));
		float     light_z = 50.0f * sin(impl->simulation_time * 0.05f);
		glm::vec3 lightPos(light_x, light_y, light_z);
		impl->resource_manager_->UpdateLightingUBO(lightPos, impl->camera.pos(), impl->simulation_time);

		if (impl->floor_enabled_) {
			impl->floor_manager_->BeginReflectionPass();
			Camera reflection_cam = impl->camera;
			reflection_cam.y = -reflection_cam.y;
			reflection_cam.pitch = -reflection_cam.pitch;
			glm::mat4 reflection_view = impl->SetupMatrices(reflection_cam);
			impl->reflection_vp = impl->projection * reflection_view;
			impl->skybox_manager_->Render(impl->projection, reflection_view);
			impl->RenderSceneObjects(
				reflection_view,
				reflection_cam,
				impl->shapes,
				impl->simulation_time,
				glm::vec4(0, 1, 0, 0.01)
			);
			impl->RenderTerrain(reflection_view, glm::vec4(0, 1, 0, 0.01));
			impl->floor_manager_->EndReflectionPass();
			impl->floor_manager_->BlurReflection(kBlurPasses);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, impl->main_fbo_);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 view = impl->SetupMatrices();
		if (impl->floor_enabled_) {
			impl->skybox_manager_->Render(impl->projection, view);
			impl->floor_manager_->Render(view, impl->projection, impl->reflection_vp);
		}
		impl->RenderSceneObjects(view, impl->camera, impl->shapes, impl->simulation_time, std::nullopt);
		impl->RenderTerrain(view, std::nullopt);

		if (impl->effects_enabled_) {
			for (auto& effect : impl->post_processing_manager_->GetEffects()) {
				if (auto glitch_effect = std::dynamic_pointer_cast<PostProcessing::GlitchEffect>(effect)) {
					glitch_effect->SetTime(impl->simulation_time);
				}
				if (auto shimmery_effect = std::dynamic_pointer_cast<PostProcessing::ShimmeryEffect>(effect)) {
					shimmery_effect->SetTime(impl->simulation_time);
				}
			}

			bool any_effect_enabled = false;
			for (const auto& effect : impl->post_processing_manager_->GetEffects()) {
				if (effect->IsEnabled()) {
					any_effect_enabled = true;
					break;
				}
			}

			GLuint final_texture = impl->main_fbo_texture_;
			if (any_effect_enabled) {
				final_texture = impl->post_processing_manager_->ApplyEffects(impl->main_fbo_texture_);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDisable(GL_DEPTH_TEST);
			glClear(GL_COLOR_BUFFER_BIT);

			impl->postprocess_shader_->use();
			impl->postprocess_shader_->setInt("sceneTexture", 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, final_texture);

			glBindVertexArray(impl->post_processing_manager_->GetQuadVao());
			glDrawArrays(GL_TRIANGLES, 0, 6);
		} else {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, impl->main_fbo_);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(
				0,
				0,
				impl->width,
				impl->height,
				0,
				0,
				impl->width,
				impl->height,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		impl->ui_manager->Render();

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

	void Visualizer::SetInputCallback(InputCallback callback) {
		impl->input_callback = callback;
	}

	void Visualizer::AddWidget(std::shared_ptr<UI::IWidget> widget) {
		impl->ui_manager->AddWidget(widget);
	}

	void Visualizer::SetExitKey(int key) {
		impl->exit_key = key;
	}

	void Visualizer::SetCameraMode(CameraMode mode) {
		impl->camera_mode = mode;
		switch (mode) {
		case CameraMode::FREE:
			glfwSetInputMode(impl->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			impl->first_mouse = true;
			break;
		case CameraMode::AUTO:
			glfwSetInputMode(impl->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case CameraMode::TRACKING:
			glfwSetInputMode(impl->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			impl->first_mouse = true;
			break;
		case CameraMode::STATIONARY:
			glfwSetInputMode(impl->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		}
	}

	void Visualizer::TogglePause() {
		impl->paused = !impl->paused;
	}

	void Visualizer::ToggleEffect(VisualEffect effect) {
		std::string name;
		switch (effect) {
		case VisualEffect::RIPPLE:
			impl->ripple_strength = (impl->ripple_strength > 0.0f) ? 0.0f : 0.05f;
			return;
		case VisualEffect::COLOR_SHIFT:
			name = "Color Shift";
			break;
		case VisualEffect::BLACK_AND_WHITE:
			name = "Black and White";
			break;
		case VisualEffect::NEGATIVE:
			name = "Negative";
			break;
		case VisualEffect::SHIMMERY:
			name = "Shimmery";
			break;
		case VisualEffect::GLITCHED:
			name = "Glitch";
			break;
		case VisualEffect::WIREFRAME:
			name = "Wireframe";
			break;
		}

		auto effect_ptr = impl->GetEffectByName(name);
		if (effect_ptr) {
			effect_ptr->SetEnabled(!effect_ptr->IsEnabled());
		}
	}

	task_thread_pool::task_thread_pool& Visualizer::GetThreadPool() {
		return impl->thread_pool;
	}

	std::tuple<float, glm::vec3> Visualizer::GetTerrainPointProperties(float x, float y) const {
		return impl->terrain_generator->pointProperties(x, y);
	}

	const std::vector<std::shared_ptr<Terrain>>& Visualizer::GetTerrainChunks() const {
		return impl->terrain_generator->getVisibleChunks();
	}

	Config& Visualizer::GetConfig() {
		return impl->config;
	}

}
