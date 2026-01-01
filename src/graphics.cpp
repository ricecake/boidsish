#include "graphics.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "Config.h"
#include "UIManager.h"
#include "clone_manager.h"
#include "dot.h"
#include "hud.h"
#include "hud_manager.h"
#include "ui/hud_widget.h"
#include "entity.h"
#include "logger.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/GlitchEffect.h"
#include "post_processing/effects/NegativeEffect.h"
#include "post_processing/effects/OpticalFlowEffect.h"
#include "post_processing/effects/StrobeEffect.h"
#include "post_processing/effects/TimeStutterEffect.h"
#include "post_processing/effects/WhispTrailEffect.h"
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
		std::unique_ptr<CloneManager>         clone_manager;
		std::map<int, std::shared_ptr<Trail>> trails;
		std::map<int, float>                  trail_last_update;

		InputState                                             input_state{};
		std::vector<InputCallback>                             input_callbacks;
		std::unique_ptr<UI::UIManager>                         ui_manager;
		std::unique_ptr<HudManager>                            hud_manager;
		std::unique_ptr<PostProcessing::PostProcessingManager> post_processing_manager_;
		int                                                    exit_key;

		Config config;

		std::unique_ptr<TerrainGenerator> terrain_generator;

		std::shared_ptr<Shader> shader;
		std::unique_ptr<Shader> plane_shader;
		std::unique_ptr<Shader> sky_shader;
		std::unique_ptr<Shader> trail_shader;
		std::unique_ptr<Shader> blur_shader;
		std::unique_ptr<Shader> postprocess_shader_;
		GLuint                  plane_vao, plane_vbo, sky_vao, blur_quad_vao, blur_quad_vbo;
		GLuint                  reflection_fbo, reflection_texture, reflection_depth_rbo;
		GLuint                  pingpong_fbo[2], pingpong_texture[2];
		GLuint                  main_fbo_, main_fbo_texture_, main_fbo_rbo_;
		GLuint                  lighting_ubo;
		GLuint                  visual_effects_ubo;
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

		std::shared_ptr<EntityBase> chase_target_ = nullptr;

		bool color_shift_effect = false;

		bool is_fullscreen_ = false;
		int  windowed_xpos_, windowed_ypos_, windowed_width_, windowed_height_;

		// Artistic effects
		bool black_and_white_effect = false;
		bool negative_effect = false;
		bool shimmery_effect = false;
		bool glitched_effect = false;
		bool wireframe_effect = false;

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
			input_callbacks.push_back([this](const InputState& state) { this->DefaultInputHandler(state); });

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

			shader = std::make_shared<Shader>("shaders/vis.vert", "shaders/vis.frag");
			Shape::shader = shader;
			trail_shader = std::make_unique<Shader>("shaders/trail.vert", "shaders/trail.frag");
			if (floor_enabled_) {
				plane_shader = std::make_unique<Shader>("shaders/plane.vert", "shaders/plane.frag");
				sky_shader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
				blur_shader = std::make_unique<Shader>("shaders/blur.vert", "shaders/blur.frag");
			}
			if (effects_enabled_) {
				postprocess_shader_ = std::make_unique<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
			}
			if (terrain_enabled_) {
				terrain_generator = std::make_unique<TerrainGenerator>();
			}
			clone_manager = std::make_unique<CloneManager>();

			glGenBuffers(1, &lighting_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
			// Allocate space for 3 vec3s with std140 padding (each vec3 padded to 16 bytes) 16 + 16 + 16
			glBufferData(GL_UNIFORM_BUFFER, 48, NULL, GL_STATIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			glBindBufferRange(GL_UNIFORM_BUFFER, 0, lighting_ubo, 0, 48);

			if (effects_enabled_) {
				glGenBuffers(1, &visual_effects_ubo);
				glBindBuffer(GL_UNIFORM_BUFFER, visual_effects_ubo);
				glBufferData(GL_UNIFORM_BUFFER, sizeof(VisualEffectsUbo), NULL, GL_DYNAMIC_DRAW);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
				glBindBufferRange(GL_UNIFORM_BUFFER, 1, visual_effects_ubo, 0, sizeof(VisualEffectsUbo));
			}

			shader->use();
			SetupShaderBindings(*shader);
			SetupShaderBindings(*trail_shader);
			if (floor_enabled_) {
				SetupShaderBindings(*plane_shader);
				SetupShaderBindings(*sky_shader);
			}

			ui_manager = std::make_unique<UI::UIManager>(window);
            logger::LOG("Initializing HudManager...");
            hud_manager = std::make_unique<HudManager>();
            logger::LOG("HudManager initialized. Creating HudWidget...");
            auto hud_widget = std::make_shared<UI::HudWidget>(*hud_manager);
            ui_manager->AddWidget(hud_widget);
            logger::LOG("HudWidget created and added.");

			if (terrain_enabled_) {
				Terrain::terrain_shader_ = std::make_shared<Shader>(
					"shaders/terrain.vert",
					"shaders/terrain.frag",
					"shaders/terrain.tcs",
					"shaders/terrain.tes"
					// , "shaders/terrain.geom"
				);
				SetupShaderBindings(*Terrain::terrain_shader_);
			}

			Shape::InitSphereMesh();

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

			if (floor_enabled_) {
				float quad_vertices[] = {
					// Definitive CCW winding
					-1.0f,
					0.0f,
					1.0f,
					1.0f,
					0.0f,
					-1.0f,
					-1.0f,
					0.0f,
					-1.0f,
					-1.0f,
					0.0f,
					1.0f,
					1.0f,
					0.0f,
					1.0f,
					1.0f,
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
				// --- Post Processing Manager ---
				post_processing_manager_ = std::make_unique<PostProcessing::PostProcessingManager>(
					width,
					height,
					blur_quad_vao
				);
				post_processing_manager_->Initialize();

				auto negative_effect = std::make_shared<PostProcessing::NegativeEffect>();
				negative_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(negative_effect);

				auto glitch_effect = std::make_shared<PostProcessing::GlitchEffect>();
				glitch_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(glitch_effect);

				auto optical_flow_effect = std::make_shared<PostProcessing::OpticalFlowEffect>();
				optical_flow_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(optical_flow_effect);

				auto strobe_effect = std::make_shared<PostProcessing::StrobeEffect>();
				strobe_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(strobe_effect);

				auto whisp_trail_effect = std::make_shared<PostProcessing::WhispTrailEffect>();
				whisp_trail_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(whisp_trail_effect);

				auto time_stutter_effect = std::make_shared<PostProcessing::TimeStutterEffect>();
				time_stutter_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(time_stutter_effect);

				// --- UI ---
				auto post_processing_widget = std::make_shared<UI::PostProcessingWidget>(*post_processing_manager_);
				ui_manager->AddWidget(post_processing_widget);
			}
		}

		void SetupShaderBindings(Shader& shader_to_setup) {
			shader_to_setup.use();
			glUniformBlockBinding(shader_to_setup.ID, glGetUniformBlockIndex(shader_to_setup.ID, "Lighting"), 0);
			glUniformBlockBinding(shader_to_setup.ID, glGetUniformBlockIndex(shader_to_setup.ID, "VisualEffects"), 1);
		}

		~VisualizerImpl() {
			config.SetInt("window_width", width);
			config.SetInt("window_height", height);
			config.SetBool("fullscreen", is_fullscreen_);
			config.Save();

			// Explicitly reset UI manager before destroying window context
			ui_manager.reset();

			Shape::DestroySphereMesh();
			if (floor_enabled_) {
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
			}
			if (window)
				glfwDestroyWindow(window);
			glfwTerminate();
		}

		// TODO: Offload frustum culling to a compute shader for performance.
		// See performance_and_quality_audit.md#1-gpu-accelerated-frustum-culling
		Frustum CalculateFrustum(const glm::mat4& view, const glm::mat4& projection) {
			Frustum   frustum;
			glm::mat4 vp = projection * view;

			// Left plane
			frustum.planes[0].normal.x = vp[0][3] + vp[0][0];
			frustum.planes[0].normal.y = vp[1][3] + vp[1][0];
			frustum.planes[0].normal.z = vp[2][3] + vp[2][0];
			frustum.planes[0].distance = vp[3][3] + vp[3][0];

			// Right plane
			frustum.planes[1].normal.x = vp[0][3] - vp[0][0];
			frustum.planes[1].normal.y = vp[1][3] - vp[1][0];
			frustum.planes[1].normal.z = vp[2][3] - vp[2][0];
			frustum.planes[1].distance = vp[3][3] - vp[3][0];

			// Bottom plane
			frustum.planes[2].normal.x = vp[0][3] + vp[0][1];
			frustum.planes[2].normal.y = vp[1][3] + vp[1][1];
			frustum.planes[2].normal.z = vp[2][3] + vp[2][1];
			frustum.planes[2].distance = vp[3][3] + vp[3][1];

			// Top plane
			frustum.planes[3].normal.x = vp[0][3] - vp[0][1];
			frustum.planes[3].normal.y = vp[1][3] - vp[1][1];
			frustum.planes[3].normal.z = vp[2][3] - vp[2][1];
			frustum.planes[3].distance = vp[3][3] - vp[3][1];

			// Near plane
			frustum.planes[4].normal.x = vp[0][3] + vp[0][2];
			frustum.planes[4].normal.y = vp[1][3] + vp[1][2];
			frustum.planes[4].normal.z = vp[2][3] + vp[2][2];
			frustum.planes[4].distance = vp[3][3] + vp[3][2];

			// Far plane
			frustum.planes[5].normal.x = vp[0][3] - vp[0][2];
			frustum.planes[5].normal.y = vp[1][3] - vp[1][2];
			frustum.planes[5].normal.z = vp[2][3] - vp[2][2];
			frustum.planes[5].distance = vp[3][3] - vp[3][2];

			// Normalize the planes
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
			shader->setBool("colorShift", color_shift_effect);
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
				// TODO: Move trail generation to the GPU for performance.
				// See performance_and_quality_audit.md#3-gpu-based-trail-generation
				// Only create trails for shapes with trail_length > 0
				if (shape->GetTrailLength() > 0 && !paused) {
					if (trails.find(shape->GetId()) == trails.end()) {
						trails[shape->GetId()] = std::make_shared<Trail>(shape->GetTrailLength());
						if (shape->IsTrailIridescent()) {
							trails[shape->GetId()]->SetIridescence(true);
						}
						// Set rocket trail property
						if (shape->IsTrailRocket()) {
							trails[shape->GetId()]->SetUseRocketTrail(true);
						}
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

			// Render clones
			shader->use();
			clone_manager->Render(*shader);
		}

		void RenderTerrain(const glm::mat4& view, const std::optional<glm::vec4>& clip_plane) {
			if (!terrain_enabled_)
				return;
			auto terrain_chunks = terrain_generator->getVisibleChunks();
			if (terrain_chunks.empty()) {
				return;
			}

			Terrain::terrain_shader_->use();
			Terrain::terrain_shader_->setMat4("view", view);
			Terrain::terrain_shader_->setMat4("projection", projection);
			Terrain::terrain_shader_->setFloat("uTessQualityMultiplier", tess_quality_multiplier_);
			Terrain::terrain_shader_->setFloat("uTessLevelMax", 64.0f);
			Terrain::terrain_shader_->setFloat("uTessLevelMin", 1.0f);

			if (clip_plane) {
				Terrain::terrain_shader_->setVec4("clipPlane", *clip_plane);
			} else {
				Terrain::terrain_shader_->setVec4("clipPlane", glm::vec4(0, 0, 0, 0)); // No clipping
			}

			for (const auto& chunk : terrain_chunks) {
				chunk->render();
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

		// TODO: Replace the multi-pass Gaussian blur with a more performant technique.
		// See performance_and_quality_audit.md#2-optimized-screen-space-reflections-and-blur
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

			glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(600.0f));
			plane_shader->setMat4("model", model);
			plane_shader->setMat4("view", view);
			plane_shader->setMat4("projection", projection);
			glBindVertexArray(plane_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}

		void DefaultInputHandler(const InputState& state) {
			// Camera movement, orbit, and zoom controls
			switch (camera_mode) {
			case CameraMode::FREE: {
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
				break;
			}
			case CameraMode::TRACKING: {
				float sensitivity = 0.1f;
				float xoffset = state.mouse_delta_x * sensitivity;
				float yoffset = state.mouse_delta_y * sensitivity;

				single_track_orbit_yaw += xoffset;
				single_track_orbit_pitch += yoffset;

				if (single_track_orbit_pitch > 89.0f)
					single_track_orbit_pitch = 89.0f;
				if (single_track_orbit_pitch < -89.0f)
					single_track_orbit_pitch = -89.0f;
				break;
			}
			default:
				// No movement controls for other modes (AUTO, STATIONARY, CHASE)
				break;
			}

			// Camera mode switching (only if not in CHASE mode)
			if (camera_mode != CameraMode::CHASE) {
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
			}

			// Single track camera zoom
			if (state.keys[GLFW_KEY_EQUAL]) {
				single_track_distance -= 0.5f;
				if (single_track_distance < 1.0f)
					single_track_distance = 1.0f;
			}
			if (state.keys[GLFW_KEY_MINUS]) {
				single_track_distance += 0.5f;
			}

			// Camera speed adjustment
			if (state.key_down[GLFW_KEY_LEFT_BRACKET]) {
				camera.speed -= kCameraSpeedStep;
				if (camera.speed < kMinCameraSpeed)
					camera.speed = kMinCameraSpeed;
			}
			if (state.key_down[GLFW_KEY_RIGHT_BRACKET]) {
				camera.speed += kCameraSpeedStep;
			}

			// Toggles
			if (state.key_down[GLFW_KEY_P])
				paused = !paused;
			if (effects_enabled_) {
				if (state.key_down[GLFW_KEY_R])
					ripple_strength = (ripple_strength > 0.0f) ? 0.0f : 0.05f;
				if (state.key_down[GLFW_KEY_C])
					color_shift_effect = !color_shift_effect;
				if (state.key_down[GLFW_KEY_1])
					black_and_white_effect = !black_and_white_effect;
				if (state.key_down[GLFW_KEY_2])
					negative_effect = !negative_effect;
				if (state.key_down[GLFW_KEY_3])
					shimmery_effect = !shimmery_effect;
				if (state.key_down[GLFW_KEY_4])
					glitched_effect = !glitched_effect;
				if (state.key_down[GLFW_KEY_5])
					wireframe_effect = !wireframe_effect;
			}

			if (state.key_down[GLFW_KEY_F11]) {
				ToggleFullscreen();
			}

			if (state.key_down[GLFW_KEY_F1]) {
				for (auto& effect : post_processing_manager_->GetEffects()) {
					if (effect->GetName() == "OpticalFlow") {
						effect->SetEnabled(!effect->IsEnabled());
					}
				}
			}
		}

		void ToggleFullscreen() {
			// Save the current cursor mode.
			int cursor_mode = glfwGetInputMode(window, GLFW_CURSOR);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			is_fullscreen_ = !is_fullscreen_;
			if (is_fullscreen_) {
				// Save windowed mode size and position
				glfwGetWindowPos(window, &windowed_xpos_, &windowed_ypos_);
				glfwGetWindowSize(window, &windowed_width_, &windowed_height_);

				// Switch to fullscreen
				GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
				const GLFWvidmode* mode = glfwGetVideoMode(monitor);
				glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			} else {
				// Restore windowed mode
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
			// Restore the original cursor mode if it wasn't already normal.
			if (cursor_mode != GLFW_CURSOR_NORMAL) {
				glfwSetInputMode(window, GLFW_CURSOR, cursor_mode);
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

		void UpdateChaseCamera(float delta_time) {
			if (camera_mode != CameraMode::CHASE || !chase_target_) {
				return;
			}

			// 1. Get target state
			const auto& boid_pos = chase_target_->GetPosition();
			const auto& boid_vel = chase_target_->GetVelocity();
			glm::vec3   target_pos(boid_pos.x, boid_pos.y, boid_pos.z);
			glm::vec3   target_vel(boid_vel.x, boid_vel.y, boid_vel.z);

			// 2. Determine forward direction from velocity
			glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f); // Default forward
			if (glm::dot(target_vel, target_vel) > 0.0001f) {
				forward = glm::normalize(target_vel);
			}

			// 3. Define camera offset
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec3 desired_cam_pos = target_pos - forward * 15.0f + up * 5.0f;
			glm::vec3 look_at_pos = target_pos + forward * 10.0f;

			// 3. Smoothly interpolate camera position
			// Frame-rate independent interpolation using exponential decay
			float     lerp_factor = 1.0f - exp(-delta_time * 2.5f);
			glm::vec3 new_cam_pos = glm::mix(camera.pos(), desired_cam_pos, lerp_factor);

			camera.x = new_cam_pos.x;
			camera.y = new_cam_pos.y;
			camera.z = new_cam_pos.z;
			if (camera.y < kMinCameraHeight)
				camera.y = kMinCameraHeight;

			// 4. Calculate desired orientation based on look-at
			glm::vec3 front = glm::normalize(look_at_pos - new_cam_pos);
			float     desired_yaw = glm::degrees(atan2(front.x, -front.z));
			float     desired_pitch = glm::degrees(asin(front.y));

			// 5. Smoothly interpolate yaw (with wrapping) and pitch
			float yaw_diff = desired_yaw - camera.yaw;
			while (yaw_diff > 180.0f)
				yaw_diff -= 360.0f;
			while (yaw_diff < -180.0f)
				yaw_diff += 360.0f;
			camera.yaw += yaw_diff * lerp_factor;

			camera.pitch = glm::mix(camera.pitch, desired_pitch, lerp_factor);
			camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));

			// 6. Smoothly interpolate roll to zero to keep the camera level
			camera.roll = glm::mix(camera.roll, 0.0f, lerp_factor);
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

		static void KeyCallback(GLFWwindow* w, int key, int /* sc */, int action, int /* mods */) {
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

			// --- Resize main scene framebuffer ---
			glBindTexture(GL_TEXTURE_2D, impl->main_fbo_texture_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glBindRenderbuffer(GL_RENDERBUFFER, impl->main_fbo_rbo_);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

			// --- Resize post-processing manager ---
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
		// Reset per-frame input state
		std::fill_n(impl->input_state.key_down, kMaxKeys, false);
		std::fill_n(impl->input_state.key_up, kMaxKeys, false);
		impl->input_state.mouse_delta_x = 0;
		impl->input_state.mouse_delta_y = 0;

		glfwPollEvents();
		auto  current_frame = std::chrono::high_resolution_clock::now();
		float delta_time = std::chrono::duration<float>(current_frame - impl->last_frame).count();
		impl->last_frame = current_frame;

		impl->input_state.delta_time = delta_time;

		for (const auto& callback : impl->input_callbacks) {
			if (callback) {
				callback(impl->input_state);
			}
		}

		if (!impl->paused) {
			// TODO: Implement a fixed timestep for simulation stability.
			// See performance_and_quality_audit.md#4-fixed-timestep-for-simulation-stability
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
		// --- 1. RENDER SCENE TO FBO ---
		// Note: The reflection and blur passes are pre-passes that generate textures for the main scene.
		// They have their own FBOs. The main scene pass below is what we want to capture.

		// Shape generation and updates (must happen before any rendering)
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
		} else if (impl->camera_mode == CameraMode::CHASE) {
			impl->UpdateChaseCamera(impl->input_state.delta_time);
		}

		// Update clone manager
		impl->clone_manager->Update(impl->simulation_time, impl->camera.pos());

		// UBO Updates
		if (impl->effects_enabled_) {
			VisualEffectsUbo ubo_data = {};
			for (const auto& shape : impl->shapes) {
				for (const auto& effect : shape->GetActiveEffects()) {
					if (effect == VisualEffect::RIPPLE) {
						ubo_data.ripple_enabled = 1;
					} else if (effect == VisualEffect::COLOR_SHIFT) {
						ubo_data.color_shift_enabled = 1;
					} else if (effect == VisualEffect::FREEZE_FRAME_TRAIL) {
						impl->clone_manager->CaptureClone(shape, impl->simulation_time);
					}
				}
			}

			ubo_data.black_and_white_enabled = impl->black_and_white_effect;
			ubo_data.negative_enabled = impl->negative_effect;
			ubo_data.shimmery_enabled = impl->shimmery_effect;
			ubo_data.glitched_enabled = impl->glitched_effect;
			ubo_data.wireframe_enabled = impl->wireframe_effect;

			glBindBuffer(GL_UNIFORM_BUFFER, impl->visual_effects_ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VisualEffectsUbo), &ubo_data);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		float     light_x = 50.0f * cos(impl->simulation_time * 0.05f);
		float     light_y = 25.0f + 1.8 * abs(sin(impl->simulation_time * 0.01));
		float     light_z = 50.0f * sin(impl->simulation_time * 0.05f);
		glm::vec3 lightPos(light_x, light_y, light_z);
		glBindBuffer(GL_UNIFORM_BUFFER, impl->lighting_ubo);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec3), &lightPos[0]);
		glBufferSubData(
			GL_UNIFORM_BUFFER,
			16,
			sizeof(glm::vec3),
			&glm::vec3(impl->camera.x, impl->camera.y, impl->camera.z)[0]
		);
		glBufferSubData(GL_UNIFORM_BUFFER, 32, sizeof(glm::vec3), &glm::vec3(1.0f, 1.0f, 1.0f)[0]);
		glBufferSubData(GL_UNIFORM_BUFFER, 44, sizeof(float), &impl->simulation_time);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		if (impl->floor_enabled_) {
			// --- Reflection Pre-Pass ---
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
				impl->RenderSceneObjects(
					reflection_view,
					reflection_cam,
					impl->shapes,
					impl->simulation_time,
					glm::vec4(0, 1, 0, 0.01)
				);
				impl->RenderTerrain(reflection_view, glm::vec4(0, 1, 0, 0.01));
			}
			glDisable(GL_CLIP_DISTANCE0);

			// --- Blur Pre-Pass ---
			impl->RenderBlur(kBlurPasses);
		}

		// --- Main Scene Pass (renders to our FBO) ---
		glBindFramebuffer(GL_FRAMEBUFFER, impl->main_fbo_);
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 view = impl->SetupMatrices();
		if (impl->floor_enabled_) {
			impl->RenderSky(view);
			impl->RenderPlane(view);
		}
		impl->RenderSceneObjects(view, impl->camera, impl->shapes, impl->simulation_time, std::nullopt);
		impl->RenderTerrain(view, std::nullopt);

		if (impl->effects_enabled_) {
			// --- Post-processing Pass (renders FBO texture to screen) ---
			// Update time-dependent effects
			for (auto& effect : impl->post_processing_manager_->GetEffects()) {
				effect->SetTime(impl->simulation_time);
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

			glBindVertexArray(impl->blur_quad_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
		} else {
			// --- Passthrough without Post-processing ---
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

		// --- UI Pass (renders on top of the fullscreen quad) ---
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

	const Camera& Visualizer::GetCamera() const {
		return impl->camera;
	}

	void Visualizer::SetCamera(const Camera& camera) {
		impl->camera = camera;
	}

	void Visualizer::AddInputCallback(InputCallback callback) {
		impl->input_callbacks.push_back(callback);
	}

	void Visualizer::SetChaseCamera(std::shared_ptr<EntityBase> target) {
		if (target) {
			impl->chase_target_ = target;
			SetCameraMode(CameraMode::CHASE);
		} else {
			impl->chase_target_ = nullptr;
			SetCameraMode(CameraMode::FREE);
		}
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
		case CameraMode::CHASE:
			glfwSetInputMode(impl->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		}
	}

	void Visualizer::TogglePause() {
		impl->paused = !impl->paused;
	}

	void Visualizer::ToggleEffect(VisualEffect effect) {
		switch (effect) {
		case VisualEffect::RIPPLE:
			impl->ripple_strength = (impl->ripple_strength > 0.0f) ? 0.0f : 0.05f;
			break;
		case VisualEffect::COLOR_SHIFT:
			impl->color_shift_effect = !impl->color_shift_effect;
			break;
		case VisualEffect::BLACK_AND_WHITE:
			impl->black_and_white_effect = !impl->black_and_white_effect;
			break;
		case VisualEffect::NEGATIVE:
			impl->negative_effect = !impl->negative_effect;
			break;
		case VisualEffect::SHIMMERY:
			impl->shimmery_effect = !impl->shimmery_effect;
			break;
		case VisualEffect::GLITCHED:
			impl->glitched_effect = !impl->glitched_effect;
			break;
		case VisualEffect::WIREFRAME:
			impl->wireframe_effect = !impl->wireframe_effect;
			break;
		case VisualEffect::FREEZE_FRAME_TRAIL:
			break;
		}
	}

	task_thread_pool::task_thread_pool& Visualizer::GetThreadPool() {
		return impl->thread_pool;
	}

	std::tuple<float, glm::vec3> Visualizer::GetTerrainPointProperties(float x, float y) const {
		return impl->terrain_generator->pointProperties(x, y);
	}

	float Visualizer::GetTerrainMaxHeight() const {
		if (impl->terrain_generator) {
			return impl->terrain_generator->GetMaxHeight();
		}
		return 0.0f;
	}

	const std::vector<std::shared_ptr<Terrain>>& Visualizer::GetTerrainChunks() const {
		return impl->terrain_generator->getVisibleChunks();
	}

	Config& Visualizer::GetConfig() {
		return impl->config;
	}

    void Visualizer::AddHudIcon(const HudIcon& icon) {
        impl->hud_manager->AddIcon(icon);
    }

    void Visualizer::UpdateHudIcon(int id, const HudIcon& icon) {
        impl->hud_manager->UpdateIcon(id, icon);
    }

    void Visualizer::RemoveHudIcon(int id) {
        impl->hud_manager->RemoveIcon(id);
    }

    void Visualizer::AddHudNumber(const HudNumber& number) {
        impl->hud_manager->AddNumber(number);
    }

    void Visualizer::UpdateHudNumber(int id, const HudNumber& number) {
        impl->hud_manager->UpdateNumber(id, number);
    }

    void Visualizer::RemoveHudNumber(int id) {
        impl->hud_manager->RemoveNumber(id);
    }

    void Visualizer::AddHudGauge(const HudGauge& gauge) {
        impl->hud_manager->AddGauge(gauge);
    }

    void Visualizer::UpdateHudGauge(int id, const HudGauge& gauge) {
        impl->hud_manager->UpdateGauge(id, gauge);
    }

    void Visualizer::RemoveHudGauge(int id) {
        impl->hud_manager->RemoveGauge(id);
    }

} // namespace Boidsish
