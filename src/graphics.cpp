#include "graphics.h"

#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <thread>
#include <vector>

#include "ConfigManager.h"
#include "UIManager.h"
#include "akira_effect.h"
#include "arcade_text.h"
#include "audio_manager.h"
#include "checkpoint_ring.h"
#include "clone_manager.h"
#include "curved_text.h"
#include "decor_manager.h"
#include "dot.h"
#include "entity.h"
#include "fire_effect_manager.h"
#include "hud.h"
#include "hud_manager.h"
#include "instance_manager.h"
#include "light_manager.h"
#include "line.h"
#include "logger.h"
#include "mesh_explosion_manager.h"
#include "path.h"
#include "render_queue.h"
#include "shader_table.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/AutoExposureEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/GlitchEffect.h"
#include "post_processing/effects/NegativeEffect.h"
#include "post_processing/effects/OpticalFlowEffect.h"
#include "post_processing/effects/SdfVolumeEffect.h"
#include "post_processing/effects/SsaoEffect.h"
#include "post_processing/effects/StrobeEffect.h"
#include "post_processing/effects/SuperSpeedEffect.h"
#include "post_processing/effects/TimeStutterEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"
#include "post_processing/effects/WhispTrailEffect.h"
#include "sdf_volume_manager.h"
#include "shadow_manager.h"
#include "shockwave_effect.h"
#include "sound_effect_manager.h"
#include "spline.h"
#include "task_thread_pool.hpp"
#include "terrain.h"
#include "terrain_generator.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include "trail.h"
#include "trail_render_manager.h"
#include "ui/ConfigWidget.h"
#include "ui/EffectsWidget.h"
#include "ui/LightsWidget.h"
#include "ui/PostProcessingWidget.h"
#include "ui/SceneWidget.h"
#include "ui/hud_widget.h"
#include "visual_effects.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_projection.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <shader.h>

namespace Boidsish {

	/**
	 * @brief Registers core C++ constants for use within shaders.
	 * This ensures that shader-side buffers and loops always match C++ expectations.
	 */
	static void RegisterShaderConstants() {
		static bool registered = false;
		if (registered)
			return;

		// Lighting and Shadows
		ShaderBase::RegisterConstant("MAX_LIGHTS", Constants::Class::Shadows::MaxLights());
		ShaderBase::RegisterConstant("MAX_SHADOW_MAPS", Constants::Class::Shadows::MaxShadowMaps());
		ShaderBase::RegisterConstant("MAX_CASCADES", Constants::Class::Shadows::MaxCascades());

		// Terrain and Decor
		ShaderBase::RegisterConstant("CHUNK_SIZE", Constants::Class::Terrain::ChunkSize());

		// Effects
		ShaderBase::RegisterConstant("MAX_SHOCKWAVES", Constants::Class::Shockwaves::MaxShockwaves());

		registered = true;
	}

	// OpenGL Debug callback for diagnosing GPU errors
	static void GLAPIENTRY OpenGLDebugCallback(
		GLenum        source,
		GLenum        type,
		GLuint        id,
		GLenum        severity,
		GLsizei       length,
		const GLchar* message,
		const void*   userParam
	) {
		// Ignore non-significant notifications
		if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
			return;

		const char* sourceStr = "UNKNOWN";
		switch (source) {
		case GL_DEBUG_SOURCE_API:
			sourceStr = "API";
			break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
			sourceStr = "WINDOW_SYSTEM";
			break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER:
			sourceStr = "SHADER_COMPILER";
			break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:
			sourceStr = "THIRD_PARTY";
			break;
		case GL_DEBUG_SOURCE_APPLICATION:
			sourceStr = "APPLICATION";
			break;
		case GL_DEBUG_SOURCE_OTHER:
			sourceStr = "OTHER";
			break;
		}

		const char* typeStr = "UNKNOWN";
		switch (type) {
		case GL_DEBUG_TYPE_ERROR:
			typeStr = "ERROR";
			break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
			typeStr = "DEPRECATED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			typeStr = "UNDEFINED_BEHAVIOR";
			break;
		case GL_DEBUG_TYPE_PORTABILITY:
			typeStr = "PORTABILITY";
			break;
		case GL_DEBUG_TYPE_PERFORMANCE:
			typeStr = "PERFORMANCE";
			break;
		case GL_DEBUG_TYPE_MARKER:
			typeStr = "MARKER";
			break;
		case GL_DEBUG_TYPE_PUSH_GROUP:
			typeStr = "PUSH_GROUP";
			break;
		case GL_DEBUG_TYPE_POP_GROUP:
			typeStr = "POP_GROUP";
			break;
		case GL_DEBUG_TYPE_OTHER:
			typeStr = "OTHER";
			break;
		}

		const char* severityStr = "UNKNOWN";
		switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH:
			severityStr = "HIGH";
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			severityStr = "MEDIUM";
			break;
		case GL_DEBUG_SEVERITY_LOW:
			severityStr = "LOW";
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			severityStr = "NOTIFICATION";
			break;
		}

		std::cerr << "\n[OpenGL Debug] " << severityStr << " | " << typeStr << " | " << sourceStr << "\n"
				  << "  ID: " << id << "\n"
				  << "  Message: " << message << "\n"
				  << std::endl;

		// Break into debugger on high severity errors (optional)
		if (severity == GL_DEBUG_SEVERITY_HIGH && type == GL_DEBUG_TYPE_ERROR) {
			std::cerr << "  *** HIGH SEVERITY ERROR - Check shader compilation or GL state ***\n" << std::endl;
		}
	}

	struct Visualizer::VisualizerImpl {
		Visualizer*                           parent;
		GLFWwindow*                           window;
		int                                   width, height;
		Camera                                camera;
		std::vector<ShapeFunction>            shape_functions;
		std::vector<std::shared_ptr<Shape>>   shapes;            // Legacy shapes from callbacks
		std::map<int, std::shared_ptr<Shape>> persistent_shapes; // New persistent shapes
		std::vector<std::shared_ptr<Shape>>   transient_effects; // Short-lived effects like CurvedText
		ConcurrentQueue<ShapeCommand>         shape_command_queue;
		std::unique_ptr<CloneManager>         clone_manager;
		std::unique_ptr<InstanceManager>      instance_manager;
		std::unique_ptr<FireEffectManager>    fire_effect_manager;
		std::unique_ptr<MeshExplosionManager> mesh_explosion_manager;
		std::unique_ptr<SoundEffectManager>   sound_effect_manager;
		std::unique_ptr<ShockwaveManager>     shockwave_manager;
		std::unique_ptr<AkiraEffectManager>   akira_effect_manager;
		std::unique_ptr<SdfVolumeManager>     sdf_volume_manager;
		std::unique_ptr<ShadowManager>        shadow_manager;
		std::unique_ptr<SceneManager>         scene_manager;
		std::unique_ptr<DecorManager>         decor_manager;
		std::map<int, std::shared_ptr<Trail>> trails;
		std::map<int, float>                  trail_last_update;
		LightManager                          light_manager;
		ShaderTable                           shader_table;
		RenderQueue                           render_queue;

		InputState                                             input_state{};
		std::vector<InputCallback>                             input_callbacks;
		std::vector<PrepareCallback>                           prepare_callbacks;
		bool                                                   prepared_{false};
		std::unique_ptr<UI::UIManager>                         ui_manager;
		std::unique_ptr<HudManager>                            hud_manager;
		std::unique_ptr<PostProcessing::PostProcessingManager> post_processing_manager_;
		int                                                    exit_key;

		std::shared_ptr<ITerrainGenerator>    terrain_generator;
		std::shared_ptr<TerrainRenderManager> terrain_render_manager;
		std::unique_ptr<TrailRenderManager>   trail_render_manager;

		std::shared_ptr<Shader> shader;
		std::shared_ptr<Shader> plane_shader;
		std::shared_ptr<Shader> sky_shader;
		std::shared_ptr<Shader> trail_shader;
		std::shared_ptr<Shader> blur_shader;
		std::shared_ptr<Shader> postprocess_shader_;
		GLuint                  plane_vao{0}, plane_vbo{0}, sky_vao{0}, blur_quad_vao{0}, blur_quad_vbo{0};
		GLuint                  reflection_fbo{0}, reflection_texture{0}, reflection_depth_rbo{0};
		GLuint                  pingpong_fbo[2]{0}, pingpong_texture[2]{0};
		GLuint                  main_fbo_{0}, main_fbo_texture_{0}, main_fbo_depth_texture_{0}, main_fbo_rbo_{0};
		GLuint                  lighting_ubo{0};
		GLuint                  visual_effects_ubo{0};
		GLuint                  frustum_ubo{0};
		glm::mat4               projection, reflection_vp;

		double last_mouse_x = 0.0, last_mouse_y = 0.0;
		bool   first_mouse = true;

		bool                                           paused = false;
		float                                          simulation_time = 0.0f;
		float                                          time_scale = 1.0f;
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

		std::shared_ptr<EntityBase>            chase_target_ = nullptr;
		std::vector<std::weak_ptr<EntityBase>> chase_targets_;
		int                                    current_chase_target_index_ = -1;

		// Path following camera state
		std::shared_ptr<Path> path_target_ = nullptr;
		int                   path_segment_index_ = 0;
		float                 path_t_ = 0.0f;
		int                   path_direction_ = 1;
		float                 path_speed_ = Constants::Project::Camera::DefaultPathSpeed();
		glm::quat             path_orientation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		float                 path_auto_bank_angle_ = 0.0f;

		bool is_fullscreen_ = false;
		int  windowed_xpos_, windowed_ypos_, windowed_width_, windowed_height_;

		float render_scale = 1.0f;
		int   render_width, render_height;

		// Adaptive Tessellation
		glm::vec3 last_camera_pos_{0.0f, 0.0f, 0.0f};
		float     last_camera_yaw_{0.0f};
		float     last_camera_pitch_{0.0f};
		float     camera_velocity_{0.0f};
		glm::vec2 camera_angular_velocity_{0.0f, 0.0f};
		float     avg_frame_time_{1.0f / 60.0f};
		float     tess_quality_multiplier_{1.0f};

		// Camera shake state
		float     shake_intensity = 0.0f;
		float     shake_timer = 0.0f;
		float     shake_duration = 0.0f;
		glm::vec3 shake_offset{0.0f};

		// Cached global settings
		float camera_roll_speed_;
		float camera_speed_step_;
		bool  enable_hdr_ = false;

		// Shadow optimization state
		bool      any_shadow_caster_moved = true;
		bool      camera_is_close_to_scene = true;
		float     shadow_update_distance_threshold = 200.0f;
		glm::vec3 last_shadow_update_camera_pos{0.0f, -1000.0f, 0.0f};
		glm::vec3 last_shadow_update_camera_front{0.0f, 0.0f, -1.0f};
		uint64_t  frame_count_ = 0;

		struct ShadowMapState {
			float     debt = 0.0f;
			int       last_update_frame = 0;
			bool      needs_update = false;
			glm::vec3 last_pos{0.0f, -1000.0f, 0.0f};
			glm::vec3 last_front{0.0f, 0.0f, -1.0f};
			glm::vec3 last_light_pos{0.0f};
			glm::vec3 last_light_dir{0.0f, -1.0f, 0.0f};
			glm::mat4 last_light_space_matrix{1.0f}; // Cache for temporal stability
			float     rotation_accumulator = 0.0f;   // Track cumulative rotation
		};

		std::array<ShadowMapState, 16> shadow_map_states_;
		int shadow_update_round_robin_ = 0; // For ensuring all cascades get updated periodically

		// Performance optimization: batched UBO updates and config caching
		std::vector<LightGPU> gpu_lights_cache_;
		LightingUbo           lighting_ubo_data_;
		FrameConfigCache      frame_config_;

		task_thread_pool::task_thread_pool thread_pool;
		std::unique_ptr<AudioManager>      audio_manager;

		VisualizerImpl(Visualizer* p, int w, int h, const char* title): parent(p), width(w), height(h) {
			RegisterShaderConstants();
			ConfigManager::GetInstance().Initialize(title);
			enable_hdr_ = ConfigManager::GetInstance().GetAppSettingBool("enable_hdr", false);
			width = ConfigManager::GetInstance().GetAppSettingInt("window_width", w);
			height = ConfigManager::GetInstance().GetAppSettingInt("window_height", h);
			is_fullscreen_ = ConfigManager::GetInstance().GetAppSettingBool("fullscreen", false);

			render_scale = ConfigManager::GetInstance().GetAppSettingFloat("render_scale", 1.0f);
			render_scale = std::clamp(render_scale, 0.1f, 1.0f);
			render_width = static_cast<int>(width * render_scale);
			render_height = static_cast<int>(height * render_scale);

			// Cache global settings
			camera_roll_speed_ = ConfigManager::GetInstance().GetGlobalSettingFloat(
				"camera_roll_speed",
				Constants::Project::Camera::RollSpeed()
			);
			camera_speed_step_ = ConfigManager::GetInstance().GetGlobalSettingFloat(
				"camera_speed_step",
				Constants::Project::Camera::SpeedStep()
			);

			// Initialize camera follow settings from config
			camera.follow_distance = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_follow_distance",
				Constants::Project::Camera::ChaseTrailBehind()
			);
			camera.follow_elevation = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_follow_elevation",
				Constants::Project::Camera::ChaseElevation()
			);
			camera.follow_look_ahead = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_follow_look_ahead",
				Constants::Project::Camera::ChaseLookAhead()
			);
			camera.follow_responsiveness = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_follow_responsiveness",
				Constants::Project::Camera::ChaseResponsiveness()
			);
			camera.path_smoothing = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_path_smoothing",
				Constants::Project::Camera::PathFollowSmoothing()
			);
			camera.path_bank_factor = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_path_bank_factor",
				Constants::Project::Camera::PathBankFactor()
			);
			camera.path_bank_speed = ConfigManager::GetInstance().GetAppSettingFloat(
				"camera_path_bank_speed",
				Constants::Project::Camera::PathBankSpeed()
			);

			exit_key = GLFW_KEY_ESCAPE;
			input_callbacks.push_back([this](const InputState& state) { this->DefaultInputHandler(state); });

			last_frame = std::chrono::high_resolution_clock::now();
			if (!glfwInit())
				throw std::runtime_error("Failed to initialize GLFW");

			glfwSetErrorCallback([](int error, const char* description) {
				std::cerr << "GLFW Error " << error << ": " << description << std::endl;
			});

			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Updated to 4.3 for compute shader support
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
			glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

			// Request debug context if debug mode is enabled
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_gl_debug", false)) {
				glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
			}

			window = glfwCreateWindow(width, height, title, nullptr, nullptr);
			if (!window) {
				glfwTerminate();
				throw std::runtime_error("Failed to create GLFW window");
			}
			glfwMakeContextCurrent(window);

			// Required for modern OpenGL on some drivers (especially Mesa)
			glewExperimental = GL_TRUE;
			if (glewInit() != GLEW_OK) {
				throw std::runtime_error("Failed to initialize GLEW");
			}
			// Clear any GL errors from glewExperimental
			while (glGetError() != GL_NO_ERROR) {
			}

			// Enable OpenGL debug output if configured
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_gl_debug", false)) {
				GLint flags;
				glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
				if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
					glEnable(GL_DEBUG_OUTPUT);
					glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
					glDebugMessageCallback(OpenGLDebugCallback, nullptr);
					// Enable all messages
					glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
					std::cout << "[OpenGL] Debug output enabled - errors will be reported in real-time\n";

					// Print GL version info for debugging
					std::cout << "[OpenGL] Version: " << glGetString(GL_VERSION) << "\n";
					std::cout << "[OpenGL] Renderer: " << glGetString(GL_RENDERER) << "\n";
					std::cout << "[OpenGL] GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
				} else {
					std::cerr << "[OpenGL] Warning: Debug context requested but not available\n";
				}
			}

			glfwWindowHint(GLFW_SAMPLES, 4);
			glfwSetWindowUserPointer(window, this);
			glfwSetKeyCallback(window, KeyCallback);
			glfwSetCursorPosCallback(window, MouseCallback);
			glfwSetMouseButtonCallback(window, MouseButtonCallback);
			glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glFrontFace(GL_CCW);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glEnable(GL_MULTISAMPLE);
			glEnable(GL_PROGRAM_POINT_SIZE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthFunc(GL_LEQUAL);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

			shader = std::make_shared<Shader>("shaders/vis.vert", "shaders/vis.frag");
			Shape::shader = shader;
			Shape::shader_handle = shader_table.Register(std::make_unique<RenderShader>(shader));

			trail_shader = std::make_shared<Shader>("shaders/trail.vert", "shaders/trail.frag");
			shader_table.Register(std::make_unique<RenderShader>(trail_shader));

			if (ConfigManager::GetInstance().GetAppSettingBool("enable_floor", true)) {
				plane_shader = std::make_shared<Shader>("shaders/plane.vert", "shaders/plane.frag");
				shader_table.Register(std::make_unique<RenderShader>(plane_shader));
			}
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_skybox", true)) {
				sky_shader = std::make_shared<Shader>("shaders/sky.vert", "shaders/sky.frag");
				shader_table.Register(std::make_unique<RenderShader>(sky_shader));
			}
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_floor", true) &&
			    ConfigManager::GetInstance().GetAppSettingBool("enable_floor_reflection", true)) {
				blur_shader = std::make_shared<Shader>("shaders/blur.vert", "shaders/blur.frag");
				shader_table.Register(std::make_unique<RenderShader>(blur_shader));
			}
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_effects", true)) {
				postprocess_shader_ =
					std::make_shared<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
				shader_table.Register(std::make_unique<RenderShader>(postprocess_shader_));
			}
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_terrain", true)) {
				terrain_generator = std::make_shared<TerrainGenerator>();
				last_camera_yaw_ = camera.yaw;
				last_camera_pitch_ = camera.pitch;
			}
			clone_manager = std::make_unique<CloneManager>();
			instance_manager = std::make_unique<InstanceManager>();
			fire_effect_manager = std::make_unique<FireEffectManager>();
			fire_effect_manager->Initialize(); // Must initialize on main thread with GL context
			mesh_explosion_manager = std::make_unique<MeshExplosionManager>();
			mesh_explosion_manager->Initialize(); // Must initialize on main thread with GL context
			shockwave_manager = std::make_unique<ShockwaveManager>();
			akira_effect_manager = std::make_unique<AkiraEffectManager>();
			SetupAkiraBindings();
			sdf_volume_manager = std::make_unique<SdfVolumeManager>();
			sdf_volume_manager->Initialize();
			shadow_manager = std::make_unique<ShadowManager>();
			scene_manager = std::make_unique<SceneManager>("scenes");
			decor_manager = std::make_unique<DecorManager>();
			audio_manager = std::make_unique<AudioManager>();
			sound_effect_manager = std::make_unique<SoundEffectManager>(audio_manager.get());
			trail_render_manager = std::make_unique<TrailRenderManager>();

			const int MAX_LIGHTS = 10;
			glGenBuffers(1, &lighting_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
			glBufferData(GL_UNIFORM_BUFFER, 704, NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::Lighting(), lighting_ubo, 0, 704);

			// Pre-allocate lighting cache for batched UBO updates
			gpu_lights_cache_.reserve(MAX_LIGHTS);

			if (ConfigManager::GetInstance().GetAppSettingBool("enable_effects", true)) {
				glGenBuffers(1, &visual_effects_ubo);
				glBindBuffer(GL_UNIFORM_BUFFER, visual_effects_ubo);
				glBufferData(GL_UNIFORM_BUFFER, sizeof(VisualEffectsUbo), NULL, GL_DYNAMIC_DRAW);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
				glBindBufferRange(
					GL_UNIFORM_BUFFER,
					Constants::UboBinding::VisualEffects(),
					visual_effects_ubo,
					0,
					sizeof(VisualEffectsUbo)
				);
			}

			// Frustum UBO for GPU-side culling
			// Layout: 6 vec4 planes (96 bytes) + vec3 camera pos + padding (16 bytes) = 112 bytes
			glGenBuffers(1, &frustum_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, frustum_ubo);
			glBufferData(GL_UNIFORM_BUFFER, 112, NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			glBindBufferRange(GL_UNIFORM_BUFFER, Constants::UboBinding::FrustumData(), frustum_ubo, 0, 112);

			shader->use();
			SetupShaderBindings(*shader);
			SetupShaderBindings(*trail_shader);

			CheckpointRingShape::SetShader(
				std::make_shared<Shader>("shaders/checkpoint.vert", "shaders/checkpoint.frag")
			);
			CheckpointRingShape::checkpoint_shader_handle =
				shader_table.Register(std::make_unique<RenderShader>(CheckpointRingShape::GetShader()));
			SetupShaderBindings(*CheckpointRingShape::GetShader());

			if (plane_shader) {
				SetupShaderBindings(*plane_shader);
			}
			if (sky_shader) {
				SetupShaderBindings(*sky_shader);
			}

			ui_manager = std::make_unique<UI::UIManager>(window);
			logger::LOG("Initializing HudManager...");
			hud_manager = std::make_unique<HudManager>();
			logger::LOG("HudManager initialized. Creating HudWidget...");
			auto hud_widget = std::make_shared<UI::HudWidget>(*hud_manager);
			ui_manager->AddWidget(hud_widget);
			logger::LOG("HudWidget created and added.");

			if (terrain_generator) {
				// Use terrain shaders with heightmap texture lookup
				Terrain::terrain_shader_ = std::make_shared<Shader>(
					"shaders/terrain.vert",
					"shaders/terrain.frag",
					"shaders/terrain.tcs",
					"shaders/terrain.tes"
				);
				Terrain::terrain_shader_handle =
					shader_table.Register(std::make_unique<RenderShader>(Terrain::terrain_shader_));
				SetupShaderBindings(*Terrain::terrain_shader_);

				// Create the terrain render manager
				// Query GPU for max texture array layers and use a safe initial allocation
				// Chunk size must match terrain_generator.h (32)
				GLint max_layers = 0;
				glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
				int initial_chunks = std::min(2048, max_layers > 0 ? max_layers : 512);
				logger::LOG(
					"Terrain render manager: GPU supports " + std::to_string(max_layers) +
					" texture array layers, using " + std::to_string(initial_chunks)
				);
				terrain_render_manager = std::make_shared<TerrainRenderManager>(32, initial_chunks);
				terrain_generator->SetRenderManager(terrain_render_manager);

				// Set up eviction callback so terrain generator knows when chunks are LRU-evicted
				// Capture weak_ptr to allow terrain generator to be swapped without dangling reference
				terrain_render_manager->SetEvictionCallback(
					[weak_gen = std::weak_ptr<ITerrainGenerator>(terrain_generator)](std::pair<int, int> key) {
						if (auto gen = weak_gen.lock()) {
							gen->InvalidateChunk(key);
						}
					}
				);
			}

			Shape::InitSphereMesh();
			Line::InitLineMesh();
			CheckpointRingShape::InitQuadMesh();

			if (postprocess_shader_ || blur_shader) {
				float blur_quad_vertices[] = {
					// positions   // texCoords
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
			}

			if (sky_shader) {
				glGenVertexArrays(1, &sky_vao);
			}

			if (plane_shader) {
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

				if (blur_shader) {
					// --- Reflection Framebuffer ---
					glGenFramebuffers(1, &reflection_fbo);
					glBindFramebuffer(GL_FRAMEBUFFER, reflection_fbo);

					// Color attachment
					glGenTextures(1, &reflection_texture);
					glBindTexture(GL_TEXTURE_2D, reflection_texture);
					glTexImage2D(
						GL_TEXTURE_2D,
						0,
						GL_RGB,
						render_width,
						render_height,
						0,
						GL_RGB,
						GL_UNSIGNED_BYTE,
						NULL
					);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflection_texture, 0);

					// Depth renderbuffer
					glGenRenderbuffers(1, &reflection_depth_rbo);
					glBindRenderbuffer(GL_RENDERBUFFER, reflection_depth_rbo);
					glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, render_width, render_height);
					glFramebufferRenderbuffer(
						GL_FRAMEBUFFER,
						GL_DEPTH_ATTACHMENT,
						GL_RENDERBUFFER,
						reflection_depth_rbo
					);

					if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
						std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
					glBindFramebuffer(GL_FRAMEBUFFER, 0);

					// --- Ping-pong Framebuffers for blurring ---
					glGenFramebuffers(2, pingpong_fbo);
					glGenTextures(2, pingpong_texture);
					for (unsigned int i = 0; i < 2; i++) {
						glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo[i]);
						glBindTexture(GL_TEXTURE_2D, pingpong_texture[i]);
						glTexImage2D(
							GL_TEXTURE_2D,
							0,
							GL_RGB16F,
							render_width,
							render_height,
							0,
							GL_RGB,
							GL_FLOAT,
							NULL
						);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
						glFramebufferTexture2D(
							GL_FRAMEBUFFER,
							GL_COLOR_ATTACHMENT0,
							GL_TEXTURE_2D,
							pingpong_texture[i],
							0
						);
						if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
							std::cerr << "ERROR::FRAMEBUFFER:: Ping-pong Framebuffer is not complete!" << std::endl;
					}
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				}
			}

			// --- Main Scene Framebuffer ---
			glGenFramebuffers(1, &main_fbo_);
			glBindFramebuffer(GL_FRAMEBUFFER, main_fbo_);

			// Color attachment
			glGenTextures(1, &main_fbo_texture_);
			glBindTexture(GL_TEXTURE_2D, main_fbo_texture_);
			if (enable_hdr_) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width, render_height, 0, GL_RGB, GL_FLOAT, NULL);
			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width, render_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, main_fbo_texture_, 0);

			// Depth-stencil texture (for shockwave depth testing and stencil operations)
			// Using GL_DEPTH24_STENCIL8 allows sampling depth while also providing stencil
			glGenTextures(1, &main_fbo_depth_texture_);
			glBindTexture(GL_TEXTURE_2D, main_fbo_depth_texture_);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_DEPTH24_STENCIL8,
				render_width,
				render_height,
				0,
				GL_DEPTH_STENCIL,
				GL_UNSIGNED_INT_24_8,
				NULL
			);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glFramebufferTexture2D(
				GL_FRAMEBUFFER,
				GL_DEPTH_STENCIL_ATTACHMENT,
				GL_TEXTURE_2D,
				main_fbo_depth_texture_,
				0
			);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cerr << "ERROR::FRAMEBUFFER:: Main framebuffer is not complete!" << std::endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// --- Shadow Manager (initialize unconditionally) ---
			shadow_manager->Initialize();
			shader_table.Register(std::make_unique<RenderShader>(shadow_manager->GetShadowShaderPtr()));

			if (postprocess_shader_) {
				// --- Post Processing Manager ---
				post_processing_manager_ = std::make_unique<PostProcessing::PostProcessingManager>(
					render_width,
					render_height,
					blur_quad_vao
				);
				post_processing_manager_->Initialize();
				post_processing_manager_->SetSharedDepthTexture(main_fbo_depth_texture_);

				// --- Shockwave Manager ---
				shockwave_manager->Initialize(render_width, render_height);

				auto auto_exposure_effect = std::make_shared<PostProcessing::AutoExposureEffect>();
				auto_exposure_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(auto_exposure_effect);

				auto ssao_effect = std::make_shared<PostProcessing::SsaoEffect>();
				ssao_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(ssao_effect);

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

				auto film_grain_effect = std::make_shared<PostProcessing::FilmGrainEffect>();
				film_grain_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(film_grain_effect);

				auto super_speed_effect = std::make_shared<PostProcessing::SuperSpeedEffect>();
				super_speed_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(super_speed_effect);

				auto atmosphere_effect = std::make_shared<PostProcessing::AtmosphereEffect>();
				atmosphere_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(atmosphere_effect);

				auto bloom_effect = std::make_shared<PostProcessing::BloomEffect>(render_width, render_height);
				bloom_effect->SetEnabled(false);
				post_processing_manager_->AddEffect(bloom_effect);

				auto sdf_volume_effect = std::make_shared<PostProcessing::SdfVolumeEffect>();
				sdf_volume_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(sdf_volume_effect);

				if (enable_hdr_) {
					auto tone_mapping_effect = std::make_shared<PostProcessing::ToneMappingEffect>();
					tone_mapping_effect->SetEnabled(true);
					bloom_effect->SetEnabled(true);
					post_processing_manager_->SetToneMappingEffect(tone_mapping_effect);
				}

				// --- UI ---
				auto post_processing_widget = std::make_shared<UI::PostProcessingWidget>(*post_processing_manager_);
				ui_manager->AddWidget(post_processing_widget);
			}

			auto config_widget = std::make_shared<UI::ConfigWidget>(*parent);
			ui_manager->AddWidget(config_widget);

			auto effects_widget = std::make_shared<UI::EffectsWidget>(parent);
			ui_manager->AddWidget(effects_widget);

			auto lights_widget = std::make_shared<UI::LightsWidget>(light_manager);
			ui_manager->AddWidget(lights_widget);

			auto scene_widget = std::make_shared<UI::SceneWidget>(*scene_manager, *parent);
			ui_manager->AddWidget(scene_widget);
		}

		void BindShadows(Shader& s) {
			s.use();
			if (shadow_manager && shadow_manager->IsInitialized() && frame_config_.enable_shadows) {
				shadow_manager->BindForRendering(s);
				std::array<int, 10> shadow_indices;
				shadow_indices.fill(-1);
				const auto& all_lights = light_manager.GetLights();
				for (size_t j = 0; j < all_lights.size() && j < 10; ++j) {
					shadow_indices[j] = all_lights[j].shadow_map_index;
				}
				s.setIntArray("lightShadowIndices", shadow_indices.data(), 10);
			} else {
				s.setInt("shadowMaps", 4);
				std::array<int, 10> shadow_indices;
				shadow_indices.fill(-1);
				s.setIntArray("lightShadowIndices", shadow_indices.data(), 10);
			}
		}

		void SetupShaderBindings(Shader& shader_to_setup) {
			shader_to_setup.use();
			GLuint sdf_volumes_idx = glGetUniformBlockIndex(shader_to_setup.ID, "SdfVolumes");
			if (sdf_volumes_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, sdf_volumes_idx, Constants::UboBinding::SdfVolumes());
			}
			GLuint lighting_idx = glGetUniformBlockIndex(shader_to_setup.ID, "Lighting");
			if (lighting_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, lighting_idx, Constants::UboBinding::Lighting());
			}
			GLuint effects_idx = glGetUniformBlockIndex(shader_to_setup.ID, "VisualEffects");
			if (effects_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, effects_idx, Constants::UboBinding::VisualEffects());
			}
			GLuint shadows_idx = glGetUniformBlockIndex(shader_to_setup.ID, "Shadows");
			if (shadows_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, shadows_idx, Constants::UboBinding::Shadows());
			}
			GLuint frustum_idx = glGetUniformBlockIndex(shader_to_setup.ID, "FrustumData");
			if (frustum_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, frustum_idx, Constants::UboBinding::FrustumData());
			}
			GLuint shockwaves_idx = glGetUniformBlockIndex(shader_to_setup.ID, "Shockwaves");
			if (shockwaves_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, shockwaves_idx, Constants::UboBinding::Shockwaves());
			}
		}

		void SetupAkiraBindings() {
			if (akira_effect_manager && akira_effect_manager->GetShader()) {
				SetupShaderBindings(*akira_effect_manager->GetShader());
			}
		}

		void RefreshFrameConfig() {
			auto& cfg = ConfigManager::GetInstance();
			frame_config_.effects_enabled = cfg.GetAppSettingBool("enable_effects", true);
			frame_config_.render_terrain = cfg.GetAppSettingBool("render_terrain", true);
			frame_config_.render_skybox = cfg.GetAppSettingBool("render_skybox", true);
			frame_config_.render_floor = cfg.GetAppSettingBool("render_floor", true);
			frame_config_.artistic_ripple = cfg.GetAppSettingBool("artistic_effect_ripple", false);
			frame_config_.artistic_color_shift = cfg.GetAppSettingBool("artistic_effect_color_shift", false);
			frame_config_.artistic_black_and_white = cfg.GetAppSettingBool("artistic_effect_black_and_white", false);
			frame_config_.artistic_negative = cfg.GetAppSettingBool("artistic_effect_negative", false);
			frame_config_.artistic_shimmery = cfg.GetAppSettingBool("artistic_effect_shimmery", false);
			frame_config_.artistic_glitched = cfg.GetAppSettingBool("artistic_effect_glitched", false);
			frame_config_.artistic_wireframe = cfg.GetAppSettingBool("artistic_effect_wireframe", false);
			frame_config_.enable_floor_reflection = cfg.GetAppSettingBool("enable_floor_reflection", true);
			frame_config_.enable_shadows = cfg.GetAppSettingBool("enable_shadows", true);
		}

		~VisualizerImpl() {
			ConfigManager::GetInstance().SetInt("window_width", width);
			ConfigManager::GetInstance().SetInt("window_height", height);
			ConfigManager::GetInstance().SetBool("fullscreen", is_fullscreen_);
			ConfigManager::GetInstance().SetFloat("render_scale", render_scale);

			ConfigManager::GetInstance().SetFloat("camera_follow_distance", camera.follow_distance);
			ConfigManager::GetInstance().SetFloat("camera_follow_elevation", camera.follow_elevation);
			ConfigManager::GetInstance().SetFloat("camera_follow_look_ahead", camera.follow_look_ahead);
			ConfigManager::GetInstance().SetFloat("camera_follow_responsiveness", camera.follow_responsiveness);
			ConfigManager::GetInstance().SetFloat("camera_path_smoothing", camera.path_smoothing);
			ConfigManager::GetInstance().SetFloat("camera_path_bank_factor", camera.path_bank_factor);
			ConfigManager::GetInstance().SetFloat("camera_path_bank_speed", camera.path_bank_speed);

			ConfigManager::GetInstance().Shutdown();

			// SoundEffectManager holds Sound objects that reference AudioManager's engine
			sound_effect_manager.reset();

			// AudioManager must be destroyed after all Sound objects
			audio_manager.reset();

			// TerrainGenerator must be destroyed before thread pool stops
			terrain_generator.reset();

			// Explicitly reset UI manager before destroying window context
			ui_manager.reset();

			Shape::DestroySphereMesh();
			Line::DestroyLineMesh();
			CheckpointRingShape::DestroyQuadMesh();

			if (blur_quad_vao)
				glDeleteVertexArrays(1, &blur_quad_vao);
			if (blur_quad_vbo)
				glDeleteBuffers(1, &blur_quad_vbo);
			if (plane_vao) {
				glDeleteVertexArrays(1, &plane_vao);
				glDeleteBuffers(1, &plane_vbo);
			}
			if (sky_vao) {
				glDeleteVertexArrays(1, &sky_vao);
			}
			if (reflection_fbo) {
				glDeleteFramebuffers(1, &reflection_fbo);
				glDeleteTextures(1, &reflection_texture);
				glDeleteRenderbuffers(1, &reflection_depth_rbo);
				glDeleteFramebuffers(2, pingpong_fbo);
				glDeleteTextures(2, pingpong_texture);
			}

			if (main_fbo_) {
				glDeleteFramebuffers(1, &main_fbo_);
				glDeleteTextures(1, &main_fbo_texture_);
				glDeleteTextures(1, &main_fbo_depth_texture_);
			}

			if (lighting_ubo) {
				glDeleteBuffers(1, &lighting_ubo);
			}

			if (visual_effects_ubo) {
				glDeleteBuffers(1, &visual_effects_ubo);
			}

			if (frustum_ubo) {
				glDeleteBuffers(1, &frustum_ubo);
			}

			if (window)
				glfwDestroyWindow(window);
			glfwTerminate();
		}

		// TODO: Offload frustum culling to a compute shader for performance.
		// See performance_and_quality_audit.md#1-gpu-accelerated-frustum-culling
		Frustum CalculateFrustum(const glm::mat4& view, const glm::mat4& projection) {
			return Frustum::FromViewProjection(view, projection);
		}

		glm::mat4 SetupMatrices(const Camera& cam_to_use) {
			float world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
			float far_plane = 1000.0f * std::max(1.0f, world_scale);
			projection = glm::perspective(glm::radians(cam_to_use.fov), (float)width / (float)height, 0.1f, far_plane);
			glm::vec3 cameraPos(cam_to_use.x, cam_to_use.y, cam_to_use.z);
			cameraPos += shake_offset;
			glm::mat4 view;

			if (camera_mode == CameraMode::PATH_FOLLOW) {
				glm::vec3 front = path_orientation_ * glm::vec3(0.0f, 0.0f, -1.0f);
				glm::vec3 up = path_orientation_ * glm::vec3(0.0f, 1.0f, 0.0f);
				view = glm::lookAt(cameraPos, cameraPos + front, up);
			} else {
				view = glm::lookAt(cameraPos, cameraPos + cam_to_use.front(), cam_to_use.up());
			}

			shader->use();
			shader->setMat4("projection", projection);
			shader->setMat4("view", view);

			if (CheckpointRingShape::GetShader()) {
				CheckpointRingShape::GetShader()->use();
				CheckpointRingShape::GetShader()->setMat4("projection", projection);
				CheckpointRingShape::GetShader()->setMat4("view", view);
			}

			return view;
		}

		glm::mat4 SetupMatrices() { return SetupMatrices(camera); }

		void UpdateTrails(const std::vector<std::shared_ptr<Shape>>& shapes, float time) {
			CleanupOldTrails(time, shapes);
			for (const auto& shape : shapes) {
				if (shape->GetTrailLength() > 0 && !paused) {
					if (trails.find(shape->GetId()) == trails.end()) {
						trails[shape->GetId()] = std::make_shared<Trail>(
							shape->GetTrailLength(),
							shape->GetTrailThickness()
						);
						if (shape->IsTrailIridescent()) {
							trails[shape->GetId()]->SetIridescence(true);
						}
						if (shape->IsTrailRocket()) {
							trails[shape->GetId()]->SetUseRocketTrail(true);
						}
						if (shape->GetTrailPBR()) {
							trails[shape->GetId()]->SetUsePBR(true);
							trails[shape->GetId()]->SetRoughness(shape->GetTrailRoughness());
							trails[shape->GetId()]->SetMetallic(shape->GetTrailMetallic());
						}
					}
					trails[shape->GetId()]->AddPoint(
						glm::vec3(shape->GetX(), shape->GetY(), shape->GetZ()),
						glm::vec3(shape->GetR(), shape->GetG(), shape->GetB())
					);
					trail_last_update[shape->GetId()] = time;
				}
			}
		}

		void RenderShapes(
			const glm::mat4&                           view,
			const std::vector<std::shared_ptr<Shape>>& shapes,
			float                                      time,
			const std::optional<glm::vec4>&            clip_plane
		) {
			shader->use();
			shader->setFloat("time", time);
			shader->setFloat(
				"ripple_strength",
				ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
			);
			shader->setMat4("view", view);
			if (clip_plane) {
				shader->setVec4("clipPlane", *clip_plane);
			} else {
				shader->setVec4("clipPlane", glm::vec4(0, 0, 0, 0)); // No clipping
			}

			// Enable GPU frustum culling for instanced rendering
			shader->setBool("enableFrustumCulling", true);

			shader->setInt("useVertexColor", 0);
			for (const auto& shape : shapes) {
				if (shape->UseNewRenderPath())
					continue;
				if (shape->IsHidden() || shape->IsTransparent()) {
					continue;
				}
				if (shape->IsInstanced()) {
					shader->setFloat("frustumCullRadius", shape->GetBoundingRadius());
					instance_manager->AddInstance(shape);
				} else {
					shader->setBool("isColossal", shape->IsColossal());
					shader->setFloat("frustumCullRadius", shape->GetBoundingRadius());
					shape->render();
				}
			}

			instance_manager->Render(*shader);

			// Render clones
			clone_manager->Render(*shader);

			// Disable frustum culling after shapes are rendered
			shader->setBool("enableFrustumCulling", false);
		}

		void RenderTransparentShapes(
			const glm::mat4&                           view,
			const Camera&                              cam,
			const std::vector<std::shared_ptr<Shape>>& shapes,
			float                                      time,
			const std::optional<glm::vec4>&            clip_plane
		) {
			std::vector<std::shared_ptr<Shape>> transparent_shapes;
			for (const auto& shape : shapes) {
				if (shape->UseNewRenderPath())
					continue;
				if (!shape->IsHidden() && shape->IsTransparent()) {
					transparent_shapes.push_back(shape);
				}
			}

			if (transparent_shapes.empty()) {
				return;
			}

			// Sort back-to-front for correct alpha blending
			glm::vec3 cameraPos = cam.pos();
			std::sort(transparent_shapes.begin(), transparent_shapes.end(), [&](const auto& a, const auto& b) {
				glm::vec3 posA(a->GetX(), a->GetY(), a->GetZ());
				glm::vec3 posB(b->GetX(), b->GetY(), b->GetZ());
				return glm::distance(cameraPos, posA) > glm::distance(cameraPos, posB);
			});

			shader->use();
			shader->setFloat("time", time);
			shader->setFloat(
				"ripple_strength",
				ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false) ? 0.05f : 0.0f
			);
			shader->setMat4("view", view);
			if (clip_plane) {
				shader->setVec4("clipPlane", *clip_plane);
			} else {
				shader->setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
			}

			// Enable GPU frustum culling for instanced rendering
			shader->setBool("enableFrustumCulling", true);

			// Disable depth writing for transparency to show objects behind
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_CULL_FACE);

			shader->setInt("useVertexColor", 0);
			for (const auto& shape : transparent_shapes) {
				if (shape->IsInstanced()) {
					shader->setFloat("frustumCullRadius", shape->GetBoundingRadius());
					instance_manager->AddInstance(shape);
				} else {
					shader->setBool("isColossal", shape->IsColossal());
					shader->setFloat("frustumCullRadius", shape->GetBoundingRadius());
					shape->render();
				}
			}

			instance_manager->Render(*shader);

			// Restore state
			glDepthMask(GL_TRUE);
			glEnable(GL_CULL_FACE);
			shader->setBool("enableFrustumCulling", false);
		}

		void ExecuteRenderQueue(
			const RenderQueue& queue,
			const glm::mat4&   view_mat,
			const glm::mat4&   proj_mat,
			RenderLayer        layer
		) {
			unsigned int current_shader_id = 0;
			unsigned int current_vao = 0;

			const auto& packets = queue.GetPackets(layer);
			for (const auto& packet : packets) {
				if (packet.shader_id == 0)
					continue;

				RenderShader* render_shader = shader_table.Get(packet.shader_handle);
				if (!render_shader)
					continue;

				ShaderBase* s = render_shader->GetBackingShader().get();

				// Minimize shader state changes
				if (packet.shader_id != current_shader_id) {
					s->use();
					current_shader_id = packet.shader_id;

					// Set frame-level uniforms using cached locations
					s->setMat4("view", view_mat);
					s->setMat4("projection", proj_mat);
					s->setFloat("time", simulation_time);
				}

				// Set packet-specific common uniforms from the grouped structure using cached locations
				s->setMat4("model", packet.uniforms.model);
				s->setVec3("objectColor", packet.uniforms.color);
				s->setFloat("objectAlpha", packet.uniforms.alpha);
				s->setBool("usePBR", packet.uniforms.use_pbr);

				if (packet.uniforms.use_pbr) {
					s->setFloat("roughness", packet.uniforms.roughness);
					s->setFloat("metallic", packet.uniforms.metallic);
					s->setFloat("ao", packet.uniforms.ao);
				}

				s->setBool("use_texture", packet.uniforms.use_texture || !packet.textures.empty());

				// Extended uniforms
				s->setBool("isLine", packet.uniforms.is_line);
				if (packet.uniforms.is_line) {
					s->setInt("lineStyle", packet.uniforms.line_style);
				}

				s->setBool("isTextEffect", packet.uniforms.is_text_effect);
				if (packet.uniforms.is_text_effect) {
					s->setFloat("textFadeProgress", packet.uniforms.text_fade_progress);
					s->setFloat("textFadeSoftness", packet.uniforms.text_fade_softness);
					s->setInt("textFadeMode", packet.uniforms.text_fade_mode);
				}

				s->setBool("isArcadeText", packet.uniforms.is_arcade_text);
				if (packet.uniforms.is_arcade_text) {
					s->setInt("arcadeWaveMode", packet.uniforms.arcade_wave_mode);
					s->setFloat("arcadeWaveAmplitude", packet.uniforms.arcade_wave_amplitude);
					s->setFloat("arcadeWaveFrequency", packet.uniforms.arcade_wave_frequency);
					s->setFloat("arcadeWaveSpeed", packet.uniforms.arcade_wave_speed);
					s->setBool("arcadeRainbowEnabled", packet.uniforms.arcade_rainbow_enabled);
					s->setFloat("arcadeRainbowSpeed", packet.uniforms.arcade_rainbow_speed);
					s->setFloat("arcadeRainbowFrequency", packet.uniforms.arcade_rainbow_frequency);
				}

				s->setInt("checkpointStyle", packet.uniforms.checkpoint_style);
				s->setInt("style", packet.uniforms.checkpoint_style);
				s->setFloat("radius", packet.uniforms.checkpoint_radius);
				s->setVec3("baseColor", packet.uniforms.color);

				// Minimize VAO state changes
				if (packet.vao != current_vao) {
					glBindVertexArray(packet.vao);
					current_vao = packet.vao;
				}

				// Render
				if (packet.ebo > 0) {
					glDrawElements(packet.draw_mode, packet.index_count, packet.index_type, 0);
				} else {
					glDrawArrays(packet.draw_mode, 0, packet.vertex_count);
				}
			}

			if (current_vao != 0)
				glBindVertexArray(0);
		}

		void RenderTrails(const glm::mat4& view, const std::optional<glm::vec4>& clip_plane) {
			// Use batched render manager if available
			if (trail_render_manager) {
				// Update trail data in the render manager
				for (auto& [trail_id, trail] : trails) {
					if (!trail_render_manager->HasTrail(trail_id)) {
						// Register new trail
						trail_render_manager->RegisterTrail(trail_id, trail->GetMaxVertexCount());
						trail->SetManagedByRenderManager(true);
					}

					// Update trail parameters
					trail_render_manager->SetTrailParams(
						trail_id,
						trail->GetIridescent(),
						trail->GetUseRocketTrail(),
						trail->GetUsePBR(),
						trail->GetRoughness(),
						trail->GetMetallic(),
						trail->GetBaseThickness()
					);

					// Update vertex data if dirty
					if (trail->IsDirty()) {
						trail_render_manager->UpdateTrailData(
							trail_id,
							trail->GetInterleavedVertexData(),
							trail->GetHead(),
							trail->GetTail(),
							trail->GetVertexCount(),
							trail->IsFull(),
							trail->GetMinBound(),
							trail->GetMaxBound()
						);
						trail->ClearDirty();
					}
				}

				// Commit updates and render all trails
				trail_render_manager->CommitUpdates();
				trail_render_manager->Render(*trail_shader, view, projection, clip_plane);
			}
		}

		void RenderTerrain(
			const glm::mat4&                view,
			const glm::mat4&                proj,
			const std::optional<glm::vec4>& clip_plane,
			bool                            is_shadow_pass = false,
			std::optional<Frustum>          shadow_frustum = std::nullopt,
			float                           quality_override = -1.0f
		) {
			if (!terrain_generator || !ConfigManager::GetInstance().GetAppSettingBool("render_terrain", true))
				return;

			// Use quality override if provided, otherwise use default multiplier
			float effective_quality = (quality_override > 0.0f) ? quality_override : tess_quality_multiplier_;

			// Inversely apply world scale to tessellation. Larger world = lower triangle density per unit.
			if (terrain_generator) {
				effective_quality /= terrain_generator->GetWorldScale();
			}

			// Determine viewport size for Screen Space Error calculations
			glm::vec2 viewport_size;
			if (is_shadow_pass) {
				viewport_size = glm::vec2(ShadowManager::kShadowMapSize, ShadowManager::kShadowMapSize);
			} else {
				viewport_size = glm::vec2(render_width, render_height);
			}

			// Set up shadow uniforms for terrain shader
			Terrain::terrain_shader_->use();
			Terrain::terrain_shader_->setBool("uIsShadowPass", is_shadow_pass);

			if (!is_shadow_pass) {
				BindShadows(*Terrain::terrain_shader_);
			}

			// Use batched render manager if available (single draw call for all chunks)
			if (terrain_render_manager) {
				// Calculate frustum for culling
				Frustum frustum = shadow_frustum.has_value() ? *shadow_frustum : CalculateFrustum(view, proj);

				// Prepare for rendering (frustum culling for instanced renderer)
				float world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
				terrain_render_manager->PrepareForRender(frustum, camera.pos(), world_scale);

				terrain_render_manager
					->Render(*Terrain::terrain_shader_, view, proj, viewport_size, clip_plane, effective_quality);
			} else {
				// Fallback to per-chunk rendering
				Terrain::terrain_shader_->use();
				Terrain::terrain_shader_->setMat4("view", view);
				Terrain::terrain_shader_->setMat4("projection", proj);
				Terrain::terrain_shader_->setVec2("uViewportSize", viewport_size);
				Terrain::terrain_shader_->setFloat("uTessQualityMultiplier", effective_quality);
				Terrain::terrain_shader_->setFloat("uTessLevelMax", 64.0f);
				Terrain::terrain_shader_->setFloat("uTessLevelMin", 1.0f);

				if (clip_plane) {
					Terrain::terrain_shader_->setVec4("clipPlane", *clip_plane);
				} else {
					Terrain::terrain_shader_->setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
				}

				auto terrain_chunks = terrain_generator->GetVisibleChunks();
				for (const auto& chunk : terrain_chunks) {
					chunk->render();
				}
			}
		}

		void RenderSky(const glm::mat4& view) {
			if (!sky_shader || !ConfigManager::GetInstance().GetAppSettingBool("render_skybox", true)) {
				return;
			}
			// Enable depth test with LEQUAL to allow sky at depth 1.0 to pass
			// but be rejected where opaque geometry already exists (early-Z optimization)
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_FALSE); // Don't write to depth buffer

			sky_shader->use();
			sky_shader->setMat4("invProjection", glm::inverse(projection));
			sky_shader->setMat4("invView", glm::inverse(view));
			glBindVertexArray(sky_vao);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glBindVertexArray(0);

			// Restore depth state
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
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
			if (!plane_shader || !ConfigManager::GetInstance().GetAppSettingBool("render_floor", true)) {
				return;
			}

			BindShadows(*plane_shader);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			plane_shader->use();
			plane_shader->setBool(
				"useReflection",
				ConfigManager::GetInstance().GetAppSettingBool("enable_floor_reflection", true)
			);
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_floor_reflection", true)) {
				plane_shader->setInt("reflectionTexture", 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, pingpong_texture[0]);
				plane_shader->setMat4("reflectionViewProjection", reflection_vp);
			}

			float     world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
			glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(600.0f * world_scale));
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
					camera.roll -= camera_roll_speed_ * state.delta_time;
				if (state.keys[GLFW_KEY_E])
					camera.roll += camera_roll_speed_ * state.delta_time;
				if (camera.y < Constants::Project::Camera::MinHeight())
					camera.y = Constants::Project::Camera::MinHeight();

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
				camera.speed -= camera_speed_step_;
				if (camera.speed < Constants::Project::Camera::MinSpeed())
					camera.speed = Constants::Project::Camera::MinSpeed();
			}
			if (state.key_down[GLFW_KEY_RIGHT_BRACKET]) {
				camera.speed += camera_speed_step_;
			}

			// Toggles
			if (state.key_down[GLFW_KEY_GRAVE_ACCENT])
				parent->ToggleMenus();
			if (state.key_down[GLFW_KEY_P])
				paused = !paused;

			if (state.key_down[GLFW_KEY_F11]) {
				ToggleFullscreen();
			}

			if (state.key_down[GLFW_KEY_F1]) {
				for (auto& effect : post_processing_manager_->GetPreToneMappingEffects()) {
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
						// Unregister from render manager before erasing
						if (trail_render_manager) {
							trail_render_manager->UnregisterTrail(trail_id);
						}
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

			if (camera.y < Constants::Project::Camera::MinHeight())
				camera.y = Constants::Project::Camera::MinHeight();

			float dx = mean_x - camera.x;
			float dy = mean_y - camera.y;
			float dz = mean_z - camera.z;

			float distance_xz = sqrt(dx * dx + dz * dz);

			camera.yaw = atan2(dx, -dz) * 180.0f / Constants::General::Math::Pi();
			camera.pitch = atan2(dy, distance_xz) * 180.0f / Constants::General::Math::Pi();
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
			glm::vec3 desired_cam_pos = target_pos - forward * camera.follow_distance + up * camera.follow_elevation;
			glm::vec3 look_at_pos = target_pos + forward * camera.follow_look_ahead;

			// 3. Smoothly interpolate camera position
			// Frame-rate independent interpolation using exponential decay
			float     lerp_factor = 1.0f - exp(-delta_time * camera.follow_responsiveness);
			glm::vec3 new_cam_pos = glm::mix(camera.pos(), desired_cam_pos, lerp_factor);

			camera.x = new_cam_pos.x;
			camera.y = new_cam_pos.y;
			camera.z = new_cam_pos.z;
			if (camera.y < Constants::Project::Camera::MinHeight())
				camera.y = Constants::Project::Camera::MinHeight();

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

			if (camera.y < Constants::Project::Camera::MinHeight())
				camera.y = Constants::Project::Camera::MinHeight();

			glm::vec3 front = glm::normalize(target_pos - camera_pos);

			camera.yaw = glm::degrees(atan2(front.x, -front.z));
			camera.pitch = glm::degrees(asin(front.y));
			camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));
		}

		void UpdatePathFollowCamera(float delta_time) {
			if (camera_mode != CameraMode::PATH_FOLLOW || !path_target_) {
				return;
			}

			// 1. Call path update logic
			auto update_result = path_target_->CalculateUpdate(
				Vector3(camera.x, camera.y, camera.z),
				path_orientation_,
				path_segment_index_,
				path_t_,
				path_direction_,
				path_speed_,
				delta_time
			);

			// 3. Update path state for the next frame
			path_segment_index_ = update_result.new_segment_index;
			path_t_ = update_result.new_t;
			path_direction_ = update_result.new_direction;

			// 4. Get target position from the result
			glm::vec3 target_pos_glm(update_result.position.x, update_result.position.y, update_result.position.z);

			// 5. Smoothly interpolate position and orientation
			float lerp_factor = 1.0f -
				exp(-delta_time * camera.path_smoothing); // Similar to chase camera for smoothness

			glm::vec3 new_cam_pos = glm::mix(camera.pos(), target_pos_glm, lerp_factor);
			camera.x = new_cam_pos.x;
			camera.y = new_cam_pos.y;
			camera.z = new_cam_pos.z;

			glm::quat desired_orientation = update_result.orientation;

			// --- Auto-banking logic ---
			float yaw_diff = glm::yaw(desired_orientation) - glm::yaw(path_orientation_);
			// Wrap yaw difference to the range [-PI, PI]
			if (yaw_diff > glm::pi<float>())
				yaw_diff -= 2.0f * glm::pi<float>();
			if (yaw_diff < -glm::pi<float>())
				yaw_diff += 2.0f * glm::pi<float>();

			float target_bank_angle = yaw_diff * camera.path_bank_factor;

			// Smoothly interpolate the bank angle
			path_auto_bank_angle_ = glm::mix(
				path_auto_bank_angle_,
				target_bank_angle,
				1.0f - exp(-delta_time * camera.path_bank_speed)
			);

			glm::quat bank_rotation = glm::angleAxis(path_auto_bank_angle_, glm::vec3(0.0f, 0.0f, 1.0f));
			glm::quat final_orientation = desired_orientation * bank_rotation;

			path_orientation_ = glm::slerp(path_orientation_, final_orientation, lerp_factor);

			// Ensure camera stays above a minimum height
			if (camera.y < Constants::Project::Camera::MinHeight())
				camera.y = Constants::Project::Camera::MinHeight();
		}

		static void KeyCallback(GLFWwindow* w, int key, int /* sc */, int action, int /* mods */) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (key == impl->exit_key && action == GLFW_PRESS) {
				glfwSetWindowShouldClose(w, true);
				return;
			}

			if (key >= 0 && key < Constants::Library::Input::MaxKeys()) {
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

		static void MouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			if (button >= 0 && button < Constants::Library::Input::MaxMouseButtons()) {
				if (action == GLFW_PRESS) {
					impl->input_state.mouse_buttons[button] = true;
					impl->input_state.mouse_button_down[button] = true;
				} else if (action == GLFW_RELEASE) {
					impl->input_state.mouse_buttons[button] = false;
					impl->input_state.mouse_button_up[button] = true;
				}
			}
		}

		static void FramebufferSizeCallback(GLFWwindow* w, int width, int height) {
			auto* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(w));
			impl->width = width;
			impl->height = height;

			impl->ResizeInternalFramebuffers();
		}

		void ResizeInternalFramebuffers() {
			render_width = static_cast<int>(width * render_scale);
			render_height = static_cast<int>(height * render_scale);

			if (reflection_fbo) {
				// --- Resize reflection framebuffer ---
				glBindTexture(GL_TEXTURE_2D, reflection_texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width, render_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
				glBindRenderbuffer(GL_RENDERBUFFER, reflection_depth_rbo);
				glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, render_width, render_height);

				// --- Resize ping-pong framebuffers ---
				for (unsigned int i = 0; i < 2; i++) {
					glBindTexture(GL_TEXTURE_2D, pingpong_texture[i]);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width, render_height, 0, GL_RGB, GL_FLOAT, NULL);
				}
			}

			// --- Resize main scene framebuffer ---
			glBindTexture(GL_TEXTURE_2D, main_fbo_texture_);
			if (enable_hdr_) {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, render_width, render_height, 0, GL_RGB, GL_FLOAT, NULL);
			} else {
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, render_width, render_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			}
			// Resize depth-stencil texture
			glBindTexture(GL_TEXTURE_2D, main_fbo_depth_texture_);
			glTexImage2D(
				GL_TEXTURE_2D,
				0,
				GL_DEPTH24_STENCIL8,
				render_width,
				render_height,
				0,
				GL_DEPTH_STENCIL,
				GL_UNSIGNED_INT_24_8,
				NULL
			);

			// --- Resize post-processing manager ---
			if (post_processing_manager_) {
				post_processing_manager_->Resize(render_width, render_height);
				post_processing_manager_->SetSharedDepthTexture(main_fbo_depth_texture_);
			}

			// --- Resize shockwave manager ---
			if (shockwave_manager) {
				shockwave_manager->Resize(render_width, render_height);
			}
		}
	};

	Visualizer::Visualizer(int w, int h, const char* t): impl(new VisualizerImpl(this, w, h, t)) {}

	Visualizer::~Visualizer() = default;

	void Visualizer::AddShapeHandler(ShapeFunction func) {
		impl->shape_functions.push_back(func);
	}

	void Visualizer::AddShape(std::shared_ptr<Shape> shape) {
		impl->shape_command_queue.push({ShapeCommandType::Add, shape, shape->GetId()});
	}

	void Visualizer::RemoveShape(int shape_id) {
		impl->shape_command_queue.push({ShapeCommandType::Remove, nullptr, shape_id});
	}

	void Visualizer::ClearShapeHandlers() {
		impl->shape_functions.clear();
	}

	bool Visualizer::ShouldClose() const {
		return glfwWindowShouldClose(impl->window);
	}

	void Visualizer::Update() {
		impl->frame_count_++;
		impl->hud_manager->Update(impl->input_state.delta_time, impl->camera);

		// Reset per-frame input state
		std::fill_n(impl->input_state.key_down, Constants::Library::Input::MaxKeys(), false);
		std::fill_n(impl->input_state.key_up, Constants::Library::Input::MaxKeys(), false);
		std::fill_n(impl->input_state.mouse_button_down, Constants::Library::Input::MaxMouseButtons(), false);
		std::fill_n(impl->input_state.mouse_button_up, Constants::Library::Input::MaxMouseButtons(), false);
		impl->input_state.mouse_delta_x = 0;
		impl->input_state.mouse_delta_y = 0;

		glfwPollEvents();

		auto  current_frame = std::chrono::high_resolution_clock::now();
		float delta_time = std::chrono::duration<float>(current_frame - impl->last_frame).count();
		impl->last_frame = current_frame;

		impl->input_state.delta_time = impl->time_scale * delta_time;

		for (const auto& callback : impl->input_callbacks) {
			if (callback) {
				callback(impl->input_state);
			}
		}

		if (!impl->paused) {
			// TODO: Implement a fixed timestep for simulation stability.
			// See performance_and_quality_audit.md#4-fixed-timestep-for-simulation-stability
			impl->simulation_time += impl->time_scale * delta_time;
		}

		impl->light_manager.Update(impl->input_state.delta_time);

		// --- Adaptive Tessellation Logic ---
		glm::vec3 current_camera_pos(impl->camera.x, impl->camera.y, impl->camera.z);
		if (delta_time > 0.0f) {
			impl->camera_velocity_ = glm::distance(current_camera_pos, impl->last_camera_pos_) / delta_time;

			float yaw_diff = impl->camera.yaw - impl->last_camera_yaw_;
			while (yaw_diff > 180.0f)
				yaw_diff -= 360.0f;
			while (yaw_diff < -180.0f)
				yaw_diff += 360.0f;

			float pitch_diff = impl->camera.pitch - impl->last_camera_pitch_;
			impl->camera_angular_velocity_ = glm::vec2(yaw_diff, pitch_diff) / delta_time;
		}
		impl->last_camera_pos_ = current_camera_pos;
		impl->last_camera_yaw_ = impl->camera.yaw;
		impl->last_camera_pitch_ = impl->camera.pitch;

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

		// Update camera shake
		if (impl->shake_timer > 0.0f) {
			impl->shake_timer -= delta_time;
			if (impl->shake_timer <= 0.0f) {
				impl->shake_timer = 0.0f;
				impl->shake_offset = glm::vec3(0.0f);
			} else {
				float t_shake = (impl->shake_duration > 0.0f) ? (impl->shake_timer / impl->shake_duration) : 0.0f;
				float current_intensity = impl->shake_intensity * t_shake; // Simple linear decay
				// Use a pseudo-random offset based on time
				float t = impl->simulation_time * 50.0f;
				impl->shake_offset = glm::vec3(
										 sin(t * 1.1f) * cos(t * 0.9f),
										 cos(t * 1.2f) * sin(t * 0.8f),
										 sin(t * 1.3f) * cos(t * 0.7f)
									 ) *
					current_intensity;
			}
		} else {
			impl->shake_offset = glm::vec3(0.0f);
		}
	}

	void Visualizer::Render() {
		impl->RefreshFrameConfig();
		impl->shapes.clear();

		// Update and collect transient effects
		auto it = impl->transient_effects.begin();
		while (it != impl->transient_effects.end()) {
			(*it)->Update(impl->input_state.delta_time);
			if ((*it)->IsExpired()) {
				it = impl->transient_effects.erase(it);
			} else {
				impl->shapes.push_back(*it);
				++it;
			}
		}

		// --- 1. RENDER SCENE TO FBO ---
		// Note: The reflection and blur passes are pre-passes that generate textures for the main scene.
		// They have their own FBOs. The main scene pass below is what we want to capture.

		// Shape generation and updates (must happen before any rendering)
		if (!impl->shape_functions.empty()) {
			for (const auto& func : impl->shape_functions) {
				auto new_shapes = func(impl->simulation_time);
				impl->shapes.insert(impl->shapes.end(), new_shapes.begin(), new_shapes.end());
			}
		}

		ShapeCommand command;
		while (impl->shape_command_queue.try_pop(command)) {
			switch (command.type) {
			case ShapeCommandType::Add:
				impl->persistent_shapes[command.shape->GetId()] = command.shape;
				break;
			case ShapeCommandType::Remove:
				impl->persistent_shapes.erase(command.shape_id);
				break;
			}
		}

		for (const auto& pair : impl->persistent_shapes) {
			impl->shapes.push_back(pair.second);
		}

		// Handle terrain clamping for shapes
		for (auto& shape : impl->shapes) {
			if (shape->IsClampedToTerrain()) {
				float     height;
				glm::vec3 normal;
				std::tie(height, normal) = GetTerrainPropertiesAtPoint(shape->GetX(), shape->GetZ());
				shape->SetPosition(shape->GetX(), height + shape->GetGroundOffset(), shape->GetZ());
			}
		}

		// --- Shadow Optimization: Check for object movement and camera proximity ---
		impl->any_shadow_caster_moved = false;
		glm::vec3 scene_center(0.0f);
		bool      has_shapes = !impl->shapes.empty();
		int       shadow_caster_count = 0;
		if (has_shapes) {
			for (const auto& shape : impl->shapes) {
				if (shape->CastsShadows()) {
					scene_center += glm::vec3(shape->GetX(), shape->GetY(), shape->GetZ());
					shadow_caster_count++;
					float distance_moved = glm::distance(
						shape->GetLastPosition(),
						glm::vec3(shape->GetX(), shape->GetY(), shape->GetZ())
					);
					if (distance_moved > 0.01f) { // Movement threshold
						impl->any_shadow_caster_moved = true;
					}
				}
			}
			if (shadow_caster_count > 0) {
				scene_center /= static_cast<float>(shadow_caster_count);
			} else {
				scene_center = impl->camera.pos();
			}
		}

		float distance_to_scene = has_shapes ? glm::distance(impl->camera.pos(), scene_center) : 0.0f;
		bool  has_terrain = (impl->terrain_generator != nullptr);

		// For terrain, we should ensure the shadow map covers the camera area.
		// If we are far from the shapes, or if terrain is the focus, center on camera.
		if (has_terrain || distance_to_scene > impl->shadow_update_distance_threshold) {
			// Snap scene_center to a grid to reduce shadow flickering when camera moves
			float grid_size = 10.0f;
			scene_center.x = std::floor(impl->camera.pos().x / grid_size) * grid_size;
			scene_center.y = std::floor(impl->camera.pos().y / grid_size) * grid_size;
			scene_center.z = std::floor(impl->camera.pos().z / grid_size) * grid_size;
			impl->camera_is_close_to_scene = true;
		} else {
			impl->camera_is_close_to_scene = (distance_to_scene < impl->shadow_update_distance_threshold);
		}

		impl->UpdateTrails(impl->shapes, impl->simulation_time);

		// --- Camera and Audio Updates ---
		if (impl->camera_mode == CameraMode::TRACKING) {
			impl->UpdateSingleTrackCamera(impl->input_state.delta_time, impl->shapes);
		} else if (impl->camera_mode == CameraMode::AUTO) {
			impl->UpdateAutoCamera(impl->input_state.delta_time, impl->shapes);
		} else if (impl->camera_mode == CameraMode::CHASE) {
			impl->UpdateChaseCamera(impl->input_state.delta_time);
		} else if (impl->camera_mode == CameraMode::PATH_FOLLOW) {
			impl->UpdatePathFollowCamera(impl->input_state.delta_time);
		}

		impl->audio_manager->UpdateListener(
			impl->camera.pos(),
			impl->camera.front(),
			impl->camera.up(),
			impl->camera_velocity_,
			impl->camera.fov
		);
		impl->audio_manager->Update();

		glm::mat4 view = impl->SetupMatrices();

		// --- Data-Driven Render Queue Collection ---
		impl->render_queue.Clear();

		// Prepare render context with updated view matrix
		RenderContext context;
		context.view = view;
		context.projection = impl->projection;
		context.view_pos = impl->camera.pos();
		context.time = impl->simulation_time;

		float world_scale = impl->terrain_generator ? impl->terrain_generator->GetWorldScale() : 1.0f;
		context.far_plane = 1000.0f * std::max(1.0f, world_scale);
		context.frustum = Frustum::FromViewProjection(view, impl->projection);
		context.shader_table = &impl->shader_table;

		for (const auto& shape : impl->shapes) {
			if (shape->UseNewRenderPath()) {
				std::vector<RenderPacket> packets;
				shape->GenerateRenderPackets(packets, context);

				for (const auto& packet : packets) {
					impl->render_queue.Submit(packet);
				}
			}
		}
		impl->render_queue.Sort();

		// Calculate frustum for terrain generation and decor placement
		Frustum generator_frustum;
		if (impl->terrain_generator) {
			// Create a widened and predictive frustum for the generator
			// This helps pre-generate chunks just out of view and in the direction of rotation
			float     world_scale = impl->terrain_generator->GetWorldScale();
			float     far_plane = 1000.0f * std::max(1.0f, world_scale);
			float     generator_fov = impl->camera.fov + 15.0f; // 15 degrees wider FOV
			glm::mat4 generator_proj = glm::perspective(
				glm::radians(generator_fov),
				(float)impl->width / (float)impl->height,
				0.1f,
				far_plane
			);

			// Predictive orientation based on current angular velocity
			float lead_time = 0.4f; // Look 0.4 seconds into the future
			float predicted_yaw = impl->camera.yaw + impl->camera_angular_velocity_.x * lead_time;
			float predicted_pitch = impl->camera.pitch + impl->camera_angular_velocity_.y * lead_time;

			Camera predicted_cam = impl->camera;
			predicted_cam.yaw = predicted_yaw;
			predicted_cam.pitch = predicted_pitch;

			glm::vec3 cameraPos(predicted_cam.x, predicted_cam.y, predicted_cam.z);
			glm::mat4 predicted_view = glm::lookAt(cameraPos, cameraPos + predicted_cam.front(), predicted_cam.up());

			generator_frustum = impl->CalculateFrustum(predicted_view, generator_proj);
			impl->terrain_generator->Update(generator_frustum, impl->camera);
		}

		// Update clone manager
		impl->clone_manager->Update(impl->simulation_time, impl->camera.pos());
		impl->fire_effect_manager->Update(impl->input_state.delta_time, impl->simulation_time);
		impl->mesh_explosion_manager->Update(impl->input_state.delta_time, impl->simulation_time);
		impl->sound_effect_manager->Update(impl->input_state.delta_time);
		impl->shockwave_manager->Update(impl->input_state.delta_time);
		if (impl->akira_effect_manager && impl->terrain_generator) {
			impl->akira_effect_manager->Update(impl->input_state.delta_time, *impl->terrain_generator);
		}
		impl->sdf_volume_manager->UpdateUBO();
		impl->sdf_volume_manager->BindUBO(Constants::UboBinding::SdfVolumes());
		impl->shockwave_manager->UpdateShaderData();
		impl->shockwave_manager->BindUBO(Constants::UboBinding::Shockwaves());

		if (impl->decor_manager && impl->terrain_generator && impl->terrain_render_manager) {
			impl->decor_manager->Update(
				impl->input_state.delta_time,
				impl->camera,
				generator_frustum,
				*impl->terrain_generator,
				impl->terrain_render_manager
			);
		}

		// UBO Updates - using cached config values
		if (impl->frame_config_.effects_enabled) {
			VisualEffectsUbo ubo_data{};
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

			// Use cached config values instead of per-call lookups
			ubo_data.black_and_white_enabled = impl->frame_config_.artistic_black_and_white;
			ubo_data.negative_enabled = impl->frame_config_.artistic_negative;
			ubo_data.shimmery_enabled = impl->frame_config_.artistic_shimmery;
			ubo_data.glitched_enabled = impl->frame_config_.artistic_glitched;
			ubo_data.wireframe_enabled = impl->frame_config_.artistic_wireframe;
			ubo_data.color_shift_enabled = ubo_data.color_shift_enabled || impl->frame_config_.artistic_color_shift;
			if (impl->frame_config_.artistic_ripple) {
				ubo_data.ripple_enabled = 1;
			}

			glBindBuffer(GL_UNIFORM_BUFFER, impl->visual_effects_ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VisualEffectsUbo), &ubo_data);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		// Batched lighting UBO update - single glBufferSubData instead of 8 calls
		{
			if (CheckpointRingShape::GetShader()) {
				CheckpointRingShape::GetShader()->use();
				CheckpointRingShape::GetShader()->setFloat("time", impl->simulation_time);
			}
			const auto& lights = impl->light_manager.GetLights();
			int         num_lights = std::min(static_cast<int>(lights.size()), 10);

			// Reuse cached vector to avoid per-frame allocation
			impl->gpu_lights_cache_.clear();
			for (int i = 0; i < num_lights; ++i) {
				impl->gpu_lights_cache_.push_back(lights[i].ToGPU());
			}

			// Fill the UBO struct in one pass
			std::memset(&impl->lighting_ubo_data_, 0, sizeof(LightingUbo));
			std::memcpy(impl->lighting_ubo_data_.lights, impl->gpu_lights_cache_.data(), num_lights * sizeof(LightGPU));
			impl->lighting_ubo_data_.num_lights = num_lights;
			impl->lighting_ubo_data_.world_scale = impl->terrain_generator ? impl->terrain_generator->GetWorldScale()
																		   : 1.0f;
			impl->lighting_ubo_data_.view_pos = impl->camera.pos();
			impl->lighting_ubo_data_.ambient_light = impl->light_manager.GetAmbientLight();
			impl->lighting_ubo_data_.time = impl->simulation_time;
			impl->lighting_ubo_data_.view_dir = impl->camera.front();

			// Single buffer upload instead of 8 separate calls
			glBindBuffer(GL_UNIFORM_BUFFER, impl->lighting_ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightingUbo), &impl->lighting_ubo_data_);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		// Update Frustum UBO for GPU-side culling
		{
			glm::mat4 view = glm::lookAt(
				impl->camera.pos(),
				impl->camera.pos() + impl->camera.front(),
				impl->camera.up()
			);
			Frustum render_frustum = impl->CalculateFrustum(view, impl->projection);

			// Pack frustum planes into vec4 array (normal.xyz, distance.w)
			glm::vec4 frustum_planes[6];
			for (int i = 0; i < 6; ++i) {
				frustum_planes[i] = glm::vec4(render_frustum.planes[i].normal, render_frustum.planes[i].distance);
			}

			glBindBuffer(GL_UNIFORM_BUFFER, impl->frustum_ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(frustum_planes), frustum_planes);
			glm::vec3 cam_pos = impl->camera.pos();
			glBufferSubData(GL_UNIFORM_BUFFER, 96, sizeof(glm::vec3), &cam_pos[0]);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
		}

		if (impl->reflection_fbo) {
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

				// Render opaque geometry first for early-Z benefit
				// Use reduced tessellation (25%) for reflection pass - it's blurred anyway
				// impl->RenderTerrain(
				// 	reflection_view,
				// 	impl->projection,
				// 	glm::vec4(0, 1, 0, 0.01),
				// 	false,
				// 	std::nullopt,
				// 	0.25f
				// );
				impl->RenderShapes(reflection_view, impl->shapes, impl->simulation_time, glm::vec4(0, 1, 0, 0.01));
				// Sky after opaque geometry
				impl->RenderSky(reflection_view);
				impl->RenderTransparentShapes(
					reflection_view,
					reflection_cam,
					impl->shapes,
					impl->simulation_time,
					glm::vec4(0, 1, 0, 0.01)
				);
				// Transparent effects last
				impl->fire_effect_manager->Render(reflection_view, impl->projection, reflection_cam.pos());
				impl->RenderTrails(reflection_view, glm::vec4(0, 1, 0, 0.01));
			}
			glDisable(GL_CLIP_DISTANCE0);

			// --- Blur Pre-Pass ---
			impl->RenderBlur(Constants::Class::Rendering::BlurPasses());
		}

		// --- Shadow Pass (render depth from each shadow-casting light) ---
		if (impl->shadow_manager && impl->shadow_manager->IsInitialized() && impl->frame_config_.enable_shadows) {
			std::vector<Light*> shadow_lights = impl->light_manager.GetShadowCastingLights();

			// 1. Assign stable map indices and identify maps
			for (auto& light : impl->light_manager.GetLights()) {
				light.shadow_map_index = -1;
			}

			struct MapUpdateInfo {
				int    map_index;
				Light* light;
				int    cascade_index; // -1 for standard
				float  weight;
			};

			std::vector<MapUpdateInfo> shadow_map_registry;
			int                        next_map_idx = 0;
			for (auto light : shadow_lights) {
				if (next_map_idx >= ShadowManager::kMaxShadowMaps)
					break;
				light->shadow_map_index = next_map_idx;
				if (light->type == DIRECTIONAL_LIGHT) {
					for (int c = 0; c < ShadowManager::kMaxCascades; ++c) {
						if (next_map_idx < ShadowManager::kMaxShadowMaps) {
							// Cascade 0 (near) needs updates most frequently for close detail
							// Cascade 3 (far) can tolerate more staleness but still needs rotation updates
							float weight = (c == 0) ? 8.0f : (c == 1) ? 4.0f : (c == 2) ? 2.0f : 1.0f;
							shadow_map_registry.push_back({next_map_idx++, light, c, weight});
						}
					}
				} else {
					shadow_map_registry.push_back({next_map_idx++, light, -1, 4.0f});
				}
			}

			// 2. Calculate camera rotation delta for rotation-sensitive updates
			// Rotation affects ALL cascades because the frustum shape changes
			float camera_rotation_delta = 1.0f - glm::dot(impl->camera.front(), impl->last_shadow_update_camera_front);
			bool  significant_rotation = camera_rotation_delta > 0.001f; // ~2.5 degrees
			bool  major_rotation = camera_rotation_delta > 0.01f;        // ~8 degrees

			// 3. Identify maps that need update with improved priority calculation
			std::vector<int> maps_to_update;
			// Allow more updates per frame during rotation to prevent stale far cascades
			const int max_updates_per_frame = major_rotation ? 4 : (significant_rotation ? 3 : 2);

			for (const auto& info : shadow_map_registry) {
				auto& state = impl->shadow_map_states_[info.map_index];

				float camera_move_dist = glm::distance(impl->camera.pos(), state.last_pos);
				float rotation_change = 1.0f - glm::dot(impl->camera.front(), state.last_front);
				bool  light_moved = glm::distance(info.light->position, state.last_light_pos) > 0.1f ||
					glm::distance(info.light->direction, state.last_light_dir) > 0.01f;

				// CRITICAL: Far cascades are MORE sensitive to rotation, not less!
				// A small camera rotation causes the far frustum to sweep across large world areas
				// Cascade 3 frustum corner can move 100s of units from a few degrees of rotation
				float rotation_sensitivity = (info.cascade_index == 0) ? 50.0f
					: (info.cascade_index == 1)                        ? 100.0f
					: (info.cascade_index == 2)                        ? 200.0f
																	   : 400.0f;
				state.rotation_accumulator += rotation_change * rotation_sensitivity;

				// Movement thresholds: near cascades need fine updates, far can be coarser
				float movement_threshold = (info.cascade_index == 0) ? 0.5f
					: (info.cascade_index == 1)                      ? 2.0f
					: (info.cascade_index == 2)                      ? 5.0f
																	 : 10.0f;

				// Rotation threshold: far cascades should update on smaller rotation changes
				float rotation_threshold = (info.cascade_index == 0) ? 1.0f
					: (info.cascade_index == 1)                      ? 0.7f
					: (info.cascade_index == 2)                      ? 0.5f
																	 : 0.3f;

				bool needs_movement_update = camera_move_dist > movement_threshold;
				bool needs_rotation_update = state.rotation_accumulator > rotation_threshold;
				bool movement_detected = impl->any_shadow_caster_moved || light_moved ||
					(has_terrain && (needs_movement_update || needs_rotation_update));

				if (movement_detected && impl->camera_is_close_to_scene) {
					// Weight by cascade importance
					float urgency = info.weight;
					// Rotation urgency scales with cascade distance (far cascades are more affected)
					if (needs_rotation_update) {
						float rotation_multiplier = 1.5f + info.cascade_index * 0.5f;
						urgency *= rotation_multiplier;
					}
					if (impl->any_shadow_caster_moved)
						urgency *= 1.5f;
					state.debt += urgency;
				} else {
					// Background refresh rate - far cascades refresh slightly faster
					// to catch distant terrain changes
					float background_rate = 0.02f + info.cascade_index * 0.005f;
					state.debt += background_rate;
				}
			}

			// 4. Select cascades to update - prioritize by debt but ensure round-robin fairness
			std::vector<std::pair<float, int>> debt_sorted;
			// Lower threshold during rotation for faster convergence
			float debt_threshold = significant_rotation ? 1.5f : 2.5f;
			for (const auto& info : shadow_map_registry) {
				auto& state = impl->shadow_map_states_[info.map_index];
				if (state.debt >= debt_threshold) {
					debt_sorted.push_back({state.debt, info.map_index});
				}
			}
			std::sort(debt_sorted.begin(), debt_sorted.end(), std::greater<>());

			for (int i = 0; i < std::min((int)debt_sorted.size(), max_updates_per_frame); ++i) {
				maps_to_update.push_back(debt_sorted[i].second);
			}

			// Force immediate update of ALL cascades on major rotation
			// This prevents the jarring "stale cascade" effect when turning quickly
			if (major_rotation && maps_to_update.size() < shadow_map_registry.size()) {
				for (const auto& info : shadow_map_registry) {
					if (std::find(maps_to_update.begin(), maps_to_update.end(), info.map_index) ==
					    maps_to_update.end()) {
						maps_to_update.push_back(info.map_index);
					}
				}
			}

			// Ensure at least one cascade gets updated via round-robin if nothing urgent
			if (maps_to_update.empty() && !shadow_map_registry.empty()) {
				impl->shadow_update_round_robin_ = (impl->shadow_update_round_robin_ + 1) % shadow_map_registry.size();
				// Every other frame, force a background update (was every 4th)
				if (impl->frame_count_ % 2 == 0) {
					maps_to_update.push_back(shadow_map_registry[impl->shadow_update_round_robin_].map_index);
				}
			}

			// 5. Update selected maps
			for (int map_idx : maps_to_update) {
				const auto& info = shadow_map_registry[map_idx];
				auto&       state = impl->shadow_map_states_[map_idx];

				float world_scale = impl->terrain_generator ? impl->terrain_generator->GetWorldScale() : 1.0f;
				impl->shadow_manager->BeginShadowPass(
					info.map_index,
					*info.light,
					scene_center,
					500.0f * std::max(1.0f, world_scale),
					info.cascade_index,
					view,
					impl->camera.fov,
					(float)impl->width / (float)impl->height
				);

				Shader& shadow_shader = impl->shadow_manager->GetShadowShader();
				shadow_shader.use();
				for (const auto& shape : impl->shapes) {
					if (shape->CastsShadows()) {
						shape->render(shadow_shader);
					}
				}

				glDisable(GL_CULL_FACE);
				impl->RenderTerrain(
					impl->shadow_manager->GetLightSpaceMatrix(info.map_index),
					glm::mat4(1.0f),
					std::nullopt,
					true,
					impl->shadow_manager->GetShadowFrustum(info.map_index)
				);
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT);

				impl->shadow_manager->EndShadowPass();

				state.debt = 0.0f;
				state.rotation_accumulator = 0.0f;
				state.last_update_frame = impl->frame_count_;
				state.last_light_space_matrix = impl->shadow_manager->GetLightSpaceMatrix(info.map_index);

				// Update last known positions for this specific shadow map
				state.last_pos = impl->camera.pos();
				state.last_front = impl->camera.front();
				state.last_light_pos = info.light->position;
				state.last_light_dir = info.light->direction;
			}

			// Update global last shadow camera state
			impl->last_shadow_update_camera_front = impl->camera.front();

			impl->shadow_manager->UpdateShadowUBO(shadow_lights);
		}

		bool effects_enabled = impl->frame_config_.effects_enabled;
		bool has_shockwaves = impl->shockwave_manager && impl->shockwave_manager->HasActiveShockwaves();
		bool skip_intermediate = (impl->render_scale == 1.0f && !effects_enabled && !has_shockwaves);

		// --- Main Scene Pass ---
		if (skip_intermediate) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, impl->width, impl->height);
		} else {
			glBindFramebuffer(GL_FRAMEBUFFER, impl->main_fbo_);
			glViewport(0, 0, impl->render_width, impl->render_height);
		}

		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		view = impl->SetupMatrices();

		// Render opaque geometry first (terrain, plane, shapes) to populate depth buffer
		impl->RenderPlane(view);
		impl->RenderTerrain(view, impl->projection, std::nullopt);

		// ALWAYS bind shadow maps (even if empty) to prevent sampler errors
		// An unbound sampler2DArrayShadow can cause shader failures on some GPUs
		impl->BindShadows(*impl->shader);

		impl->RenderShapes(view, impl->shapes, impl->simulation_time, std::nullopt);
		impl->ExecuteRenderQueue(impl->render_queue, view, impl->projection, RenderLayer::Opaque);
		if (impl->decor_manager) {
			impl->decor_manager->Render(view, impl->projection);
		}

		// Render sky AFTER opaque geometry so early-Z rejects covered fragments
		// This avoids expensive noise calculations for pixels already drawn
		impl->RenderSky(view);

		GLuint current_texture = impl->main_fbo_texture_;
		GLuint current_depth = impl->main_fbo_depth_texture_;

		if (effects_enabled) {
			impl->post_processing_manager_->BeginApply(current_texture, impl->main_fbo_, current_depth);
			impl->post_processing_manager_
				->ApplyEarlyEffects(view, impl->projection, impl->camera.pos(), impl->simulation_time);

			// Re-bind shadows for transparent objects as early effects may have changed texture bindings
			impl->BindShadows(*impl->shader);

			impl->post_processing_manager_->AttachDepthToCurrentFBO();
			glBindFramebuffer(GL_FRAMEBUFFER, impl->post_processing_manager_->GetCurrentFBO());
			current_texture = impl->post_processing_manager_->GetFinalTexture();
		}

		// Render transparent shapes after sky and early post-processing
		impl->RenderTransparentShapes(view, impl->camera, impl->shapes, impl->simulation_time, std::nullopt);
		impl->ExecuteRenderQueue(impl->render_queue, view, impl->projection, RenderLayer::Transparent);

		// Render transparent/particle effects last
		impl->fire_effect_manager->Render(view, impl->projection, impl->camera.pos());
		impl->mesh_explosion_manager->Render(view, impl->projection, impl->camera.pos());
		if (impl->akira_effect_manager) {
			impl->akira_effect_manager->Render(view, impl->projection, impl->simulation_time);
		}
		impl->RenderTrails(view, std::nullopt);

		if (effects_enabled) {
			// --- Post-processing Pass (renders FBO texture to screen) ---

			// Apply standard post-processing effects (at render resolution)
			impl->post_processing_manager_
				->ApplyLateEffects(view, impl->projection, impl->camera.pos(), impl->simulation_time);
			GLuint final_texture = impl->post_processing_manager_->GetFinalTexture();

			// Return to display resolution for final output
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, impl->width, impl->height);
			glDisable(GL_DEPTH_TEST);
			glClear(GL_COLOR_BUFFER_BIT);

			// Apply shockwave effect as the final pass (after other post-processing)
			// This ensures the distortion is visible and not processed by other effects
			if (has_shockwaves) {
				// We need to pass the target resolution for the viewport
				impl->shockwave_manager->ApplyScreenSpaceEffect(
					final_texture,
					impl->main_fbo_depth_texture_,
					view,
					impl->projection,
					impl->camera.pos(),
					impl->blur_quad_vao,
					impl->width,
					impl->height
				);
			} else {
				// No shockwaves - just render the post-processed texture (upscales automatically via viewport)
				impl->postprocess_shader_->use();
				impl->postprocess_shader_->setInt("sceneTexture", 0);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, final_texture);

				glBindVertexArray(impl->blur_quad_vao);
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		} else {
			// --- Passthrough without Post-processing ---
			if (!skip_intermediate) {
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glViewport(0, 0, impl->width, impl->height);
				glDisable(GL_DEPTH_TEST);
				glClear(GL_COLOR_BUFFER_BIT);

				// Still apply shockwave if active
				if (has_shockwaves) {
					impl->shockwave_manager->ApplyScreenSpaceEffect(
						impl->main_fbo_texture_,
						impl->main_fbo_depth_texture_,
						view,
						impl->projection,
						impl->camera.pos(),
						impl->blur_quad_vao,
						impl->width,
						impl->height
					);
				} else {
					// Direct blit with upscaling
					glBindFramebuffer(GL_READ_FRAMEBUFFER, impl->main_fbo_);
					glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
					glBlitFramebuffer(
						0,
						0,
						impl->render_width,
						impl->render_height,
						0,
						0,
						impl->width,
						impl->height,
						GL_COLOR_BUFFER_BIT,
						GL_LINEAR
					);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
				}
			}
		}

		// --- Shadow Optimization: Update last known positions for the next frame ---
		for (const auto& shape : impl->shapes) {
			shape->UpdateLastPosition();
		}

		// --- UI Pass (renders on top of the fullscreen quad) ---
		impl->ui_manager->Render();

		glfwSwapBuffers(impl->window);
	}

	void Visualizer::Prepare() {
		if (impl->prepared_) {
			return; // Already prepared
		}

		logger::LOG("Preparing visualizer...");

		// --- Pre-flight validation ---
		// Verify critical systems are initialized
		if (!impl->shader || impl->shader->ID == 0) {
			logger::ERROR("Main shader failed to compile - visualization may not work correctly");
		}

		// --- Warm up terrain cache ---
		if (impl->terrain_generator) {
			logger::LOG("Warming up terrain cache around camera position...");

			// Construct view and projection matrices from camera state
			glm::vec3 cameraPos(impl->camera.x, impl->camera.y, impl->camera.z);
			glm::mat4 view = glm::lookAt(cameraPos, cameraPos + impl->camera.front(), impl->camera.up());
			float     world_scale = impl->terrain_generator ? impl->terrain_generator->GetWorldScale() : 1.0f;
			float     far_plane = 1000.0f * std::max(1.0f, world_scale);
			glm::mat4 proj = glm::perspective(
				glm::radians(impl->camera.fov),
				(float)impl->width / (float)impl->height,
				0.1f,
				far_plane
			);

			// Update terrain once to start chunk loading around the camera
			impl->terrain_generator->Update(impl->CalculateFrustum(view, proj), impl->camera);

			// Process any pending async chunk loads
			// Give terrain generation a head start
			for (int i = 0; i < 10; ++i) {
				impl->terrain_generator->Update(impl->CalculateFrustum(view, proj), impl->camera);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		// --- Verify GPU features ---
		if (impl->fire_effect_manager) {
			logger::LOG(
				"Fire effect system: " +
				std::string(
					impl->fire_effect_manager->IsAvailable() ? "ready" : "disabled (compute shader unavailable)"
				)
			);
		}
		if (impl->mesh_explosion_manager) {
			logger::LOG(
				"Mesh explosion system: " +
				std::string(
					impl->mesh_explosion_manager->IsAvailable() ? "ready" : "disabled (compute shader unavailable)"
				)
			);
		}

		// --- Invoke user prepare callbacks ---
		if (!impl->prepare_callbacks.empty()) {
			logger::LOG("Invoking " + std::to_string(impl->prepare_callbacks.size()) + " prepare callback(s)...");
			for (auto& callback : impl->prepare_callbacks) {
				callback(*this);
			}
		}

		impl->prepared_ = true;
		logger::LOG("Visualizer prepared and ready");
	}

	void Visualizer::AddPrepareCallback(PrepareCallback callback) {
		impl->prepare_callbacks.push_back(std::move(callback));
	}

	void Visualizer::Run() {
		Prepare(); // Ensure system is ready before starting main loop

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

	glm::mat4 Visualizer::GetProjectionMatrix() const {
		return impl->projection;
	}

	glm::mat4 Visualizer::GetViewMatrix() const {
		return impl->SetupMatrices(impl->camera);
	}

	GLFWwindow* Visualizer::GetWindow() const {
		return impl->window;
	}

	void Visualizer::SetCamera(const Camera& camera) {
		impl->camera = camera;
	}

	void Visualizer::AddInputCallback(InputCallback callback) {
		impl->input_callbacks.push_back(callback);
	}

	std::optional<glm::vec3> Visualizer::ScreenToWorld(double screen_x, double screen_y) const {
		glm::vec3 screen_pos(screen_x, impl->height - screen_y, 0.0f);

		glm::vec4 viewport(0.0f, 0.0f, impl->width, impl->height);

		glm::mat4 view = impl->SetupMatrices(impl->camera);

		glm::vec3 world_pos = glm::unProject(screen_pos, view, impl->projection, viewport);

		const auto& cam = impl->camera;
		glm::vec3   ray_origin = cam.pos();

		screen_pos.z = 1.0f;
		glm::vec3 far_plane_pos = glm::unProject(screen_pos, view, impl->projection, viewport);

		glm::vec3 ray_dir = glm::normalize(far_plane_pos - ray_origin);

		float world_scale = impl->terrain_generator ? impl->terrain_generator->GetWorldScale() : 1.0f;
		float max_ray_dist = 1000.0f * std::max(1.0f, world_scale);

		float                      distance;
		[[maybe_unused]] glm::vec3 normal;
		if (impl->terrain_generator->RaycastCached(ray_origin, ray_dir, max_ray_dist, distance, normal)) {
			return ray_origin + ray_dir * distance;
		}

		return std::nullopt;
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

	void Visualizer::AddChaseTarget(std::shared_ptr<EntityBase> target) {
		if (!target)
			return;
		impl->chase_targets_.push_back(target);
		if (impl->chase_target_ == nullptr) {
			SetChaseCamera(target);
			impl->current_chase_target_index_ = impl->chase_targets_.size() - 1;
		}
	}

	void Visualizer::CycleChaseTarget() {
		if (impl->chase_targets_.empty())
			return;

		// Optional: Periodically clean up expired targets to prevent vector growth
		if (impl->chase_targets_.size() > 100 && (impl->frame_count_ % 1000 == 0)) {
			auto it = std::remove_if(impl->chase_targets_.begin(), impl->chase_targets_.end(), [](const auto& wp) {
				return wp.expired();
			});
			impl->chase_targets_.erase(it, impl->chase_targets_.end());
		}

		int num_targets = static_cast<int>(impl->chase_targets_.size());
		int start_index = impl->current_chase_target_index_;

		// Determine starting point for the search
		int search_start = (start_index == -1) ? 0 : (start_index + 1) % num_targets;

		// Search for the next valid target in one full cycle
		for (int i = 0; i < num_targets; ++i) {
			int index = (search_start + i) % num_targets;
			if (auto target = impl->chase_targets_[index].lock()) {
				impl->current_chase_target_index_ = index;
				SetChaseCamera(target);
				return;
			}
		}
	}

	void Visualizer::SetSuperSpeedIntensity(float intensity) {
		if (impl->post_processing_manager_) {
			for (auto& effect : impl->post_processing_manager_->GetPreToneMappingEffects()) {
				if (auto super_speed = std::dynamic_pointer_cast<PostProcessing::SuperSpeedEffect>(effect)) {
					super_speed->SetIntensity(intensity);
					break;
				}
			}
		}
	}

	void Visualizer::SetCameraShake(float intensity, float duration) {
		impl->shake_intensity = intensity;
		impl->shake_timer = duration;
		impl->shake_duration = duration;
	}

	void Visualizer::SetPathCamera(std::shared_ptr<Path> path) {
		if (path && !path->GetWaypoints().empty()) {
			impl->path_target_ = path;
			impl->path_segment_index_ = 0;
			impl->path_t_ = 0.0f;
			impl->path_direction_ = 1;
			SetCameraMode(CameraMode::PATH_FOLLOW);

			// Initialize camera position and orientation to the start of the path
			const auto& start_waypoint = path->GetWaypoints()[0];
			auto initial_state = path->CalculateUpdate(start_waypoint.position, glm::quat(), 0, 0.0f, 1, 0.0f, 0.0f);

			impl->camera.x = start_waypoint.position.x;
			impl->camera.y = start_waypoint.position.y;
			impl->camera.z = start_waypoint.position.z;
			impl->path_orientation_ = initial_state.orientation;

		} else {
			impl->path_target_ = nullptr;
			SetCameraMode(CameraMode::FREE);
		}
	}

	void Visualizer::AddWidget(std::shared_ptr<UI::IWidget> widget) {
		impl->ui_manager->AddWidget(widget);
	}

	void Visualizer::SetExitKey(int key) {
		impl->exit_key = key;
	}

	CameraMode Visualizer::GetCameraMode() const {
		return impl->camera_mode;
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
		case CameraMode::PATH_FOLLOW:
			glfwSetInputMode(impl->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		}
	}

	void Visualizer::TogglePause() {
		impl->paused = !impl->paused;
	}

	bool Visualizer::GetPause() {
		return impl->paused;
	}

	void Visualizer::SetPause(bool p) {
		impl->paused = p;
	}

	void Visualizer::ToggleEffect(VisualEffect effect) {
		auto& config = ConfigManager::GetInstance();
		switch (effect) {
		case VisualEffect::RIPPLE:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_ripple", false));
			break;
		case VisualEffect::COLOR_SHIFT:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_color_shift", false));
			break;
		case VisualEffect::BLACK_AND_WHITE:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_black_and_white", false));
			break;
		case VisualEffect::NEGATIVE:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_negative", false));
			break;
		case VisualEffect::SHIMMERY:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_shimmery", false));
			break;
		case VisualEffect::GLITCHED:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_glitched", false));
			break;
		case VisualEffect::WIREFRAME:
			SetEffectEnabled(effect, !config.GetAppSettingBool("artistic_effect_wireframe", false));
			break;
		case VisualEffect::FREEZE_FRAME_TRAIL:
			break;
		}
	}

	void Visualizer::SetEffectEnabled(VisualEffect effect, bool enabled) {
		auto& config = ConfigManager::GetInstance();
		switch (effect) {
		case VisualEffect::RIPPLE:
			config.SetBool("artistic_effect_ripple", enabled);
			break;
		case VisualEffect::COLOR_SHIFT:
			config.SetBool("artistic_effect_color_shift", enabled);
			break;
		case VisualEffect::BLACK_AND_WHITE:
			config.SetBool("artistic_effect_black_and_white", enabled);
			break;
		case VisualEffect::NEGATIVE:
			config.SetBool("artistic_effect_negative", enabled);
			break;
		case VisualEffect::SHIMMERY:
			config.SetBool("artistic_effect_shimmery", enabled);
			break;
		case VisualEffect::GLITCHED:
			config.SetBool("artistic_effect_glitched", enabled);
			break;
		case VisualEffect::WIREFRAME:
			config.SetBool("artistic_effect_wireframe", enabled);
			break;
		case VisualEffect::FREEZE_FRAME_TRAIL:
			break;
		}
	}

	void Visualizer::ToggleMenus() {
		impl->ui_manager->ToggleMenus();
	}

	task_thread_pool::task_thread_pool& Visualizer::GetThreadPool() {
		return impl->thread_pool;
	}

	LightManager& Visualizer::GetLightManager() {
		return impl->light_manager;
	}

	std::tuple<float, glm::vec3> Visualizer::CalculateTerrainPropertiesAtPoint(float x, float y) const {
		if (impl->terrain_generator) {
			return impl->terrain_generator->CalculateTerrainPropertiesAtPoint(x, y);
		}
		return {0.0f, glm::vec3(0, 1, 0)};
	}

	std::tuple<float, glm::vec3> Visualizer::GetTerrainPropertiesAtPoint(float x, float y) const {
		if (impl->terrain_generator) {
			return impl->terrain_generator->GetTerrainPropertiesAtPoint(x, y);
		}
		return {0.0f, glm::vec3(0, 1, 0)};
	}

	float Visualizer::GetTerrainMaxHeight() const {
		if (impl->terrain_generator) {
			return impl->terrain_generator->GetMaxHeight();
		}
		return 0.0f;
	}

	const std::vector<std::shared_ptr<Terrain>>& Visualizer::GetTerrainChunks() const {
		return impl->terrain_generator->GetVisibleChunks();
	}

	std::shared_ptr<ITerrainGenerator> Visualizer::GetTerrain() {
		return impl->terrain_generator;
	}

	std::shared_ptr<const ITerrainGenerator> Visualizer::GetTerrain() const {
		return impl->terrain_generator;
	}

	const TerrainGenerator* Visualizer::GetTerrainGenerator() const {
		// Legacy method - attempt to cast to concrete type
		return dynamic_cast<const TerrainGenerator*>(impl->terrain_generator.get());
	}

	void Visualizer::InstallTerrainGenerator(std::shared_ptr<ITerrainGenerator> generator) {
		// Swap the terrain generator
		impl->terrain_generator = std::move(generator);

		// Set up the render manager for the new generator
		if (impl->terrain_render_manager && impl->terrain_generator) {
			impl->terrain_generator->SetRenderManager(impl->terrain_render_manager);

			// Set up eviction callback with weak_ptr to avoid preventing destruction
			impl->terrain_render_manager->SetEvictionCallback(
				[weak_gen = std::weak_ptr<ITerrainGenerator>(impl->terrain_generator)](std::pair<int, int> chunk_key) {
					if (auto gen = weak_gen.lock()) {
						gen->InvalidateChunk(chunk_key);
					}
				}
			);
		}
	}

	std::shared_ptr<HudIcon>
	Visualizer::AddHudIcon(const std::string& path, HudAlignment alignment, glm::vec2 position, glm::vec2 size) {
		return impl->hud_manager->AddIcon(path, alignment, position, size);
	}

	std::shared_ptr<HudNumber> Visualizer::AddHudNumber(
		float              value,
		const std::string& label,
		HudAlignment       alignment,
		glm::vec2          position,
		int                precision
	) {
		return impl->hud_manager->AddNumber(value, label, alignment, position, precision);
	}

	std::shared_ptr<HudGauge> Visualizer::AddHudGauge(
		float              value,
		const std::string& label,
		HudAlignment       alignment,
		glm::vec2          position,
		glm::vec2          size
	) {
		return impl->hud_manager->AddGauge(value, label, alignment, position, size);
	}

	std::shared_ptr<HudCompass> Visualizer::AddHudCompass(HudAlignment alignment, glm::vec2 position) {
		auto compass = std::make_shared<HudCompass>(alignment, position);
		impl->hud_manager->AddElement(compass);
		return compass;
	}

	std::shared_ptr<HudLocation> Visualizer::AddHudLocation(HudAlignment alignment, glm::vec2 position) {
		auto location = std::make_shared<HudLocation>(alignment, position);
		impl->hud_manager->AddElement(location);
		return location;
	}

	std::shared_ptr<HudScore> Visualizer::AddHudScore(HudAlignment alignment, glm::vec2 position) {
		auto score = std::make_shared<HudScore>(alignment, position);
		impl->hud_manager->AddElement(score);
		return score;
	}

	std::shared_ptr<HudMessage> Visualizer::AddHudMessage(
		const std::string& message,
		HudAlignment       alignment,
		glm::vec2          position,
		float              fontSizeScale
	) {
		return impl->hud_manager->AddMessage(message, alignment, position, fontSizeScale);
	}

	std::shared_ptr<HudIconSet> Visualizer::AddHudIconSet(
		const std::vector<std::string>& paths,
		HudAlignment                    alignment,
		glm::vec2                       position,
		glm::vec2                       size,
		float                           spacing
	) {
		auto iconSet = std::make_shared<HudIconSet>(paths, alignment, position, size, spacing);
		impl->hud_manager->AddElement(iconSet);
		return iconSet;
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

	std::shared_ptr<FireEffect> Visualizer::AddFireEffect(
		const glm::vec3& position,
		FireEffectStyle  style,
		const glm::vec3& direction,
		const glm::vec3& velocity,
		int              max_particles,
		float            lifetime
	) {
		return impl->fire_effect_manager->AddEffect(position, style, direction, velocity, max_particles, lifetime);
	}

	FireEffectManager* Visualizer::GetFireEffectManager() {
		return impl->fire_effect_manager.get();
	}

	DecorManager* Visualizer::GetDecorManager() {
		return impl->decor_manager.get();
	}

	PostProcessing::PostProcessingManager& Visualizer::GetPostProcessingManager() {
		return *impl->post_processing_manager_;
	}

	float Visualizer::GetLastFrameTime() const {
		return impl->input_state.delta_time;
	}

	void Visualizer::RemoveFireEffect(const std::shared_ptr<FireEffect>& effect) const {
		impl->fire_effect_manager->RemoveEffect(effect);
	}

	std::shared_ptr<SoundEffect> Visualizer::AddSoundEffect(
		const std::string& filepath,
		const glm::vec3&   position,
		const glm::vec3&   velocity,
		float              volume,
		bool               loop,
		float              lifetime
	) {
		return impl->sound_effect_manager->AddEffect(filepath, position, velocity, volume, loop, lifetime);
	}

	void Visualizer::RemoveSoundEffect(const std::shared_ptr<SoundEffect>& effect) const {
		impl->sound_effect_manager->RemoveEffect(effect);
	}

	void Visualizer::TogglePostProcessingEffect(const std::string& name) {
		if (impl->post_processing_manager_) {
			for (auto& effect : impl->post_processing_manager_->GetPreToneMappingEffects()) {
				if (effect->GetName() == name) {
					effect->SetEnabled(!effect->IsEnabled());
					break;
				}
			}
		}
	}

	void Visualizer::TogglePostProcessingEffect(const std::string& name, const bool newState) {
		if (impl->post_processing_manager_) {
			for (auto& effect : impl->post_processing_manager_->GetPreToneMappingEffects()) {
				if (effect->GetName() == name) {
					effect->SetEnabled(newState);
					break;
				}
			}
		}
	}

	void Visualizer::SetFilmGrainIntensity(float intensity) {
		if (impl->post_processing_manager_) {
			for (auto& effect : impl->post_processing_manager_->GetPreToneMappingEffects()) {
				if (auto film_grain = std::dynamic_pointer_cast<PostProcessing::FilmGrainEffect>(effect)) {
					film_grain->SetIntensity(intensity);
					break;
				}
			}
		}
	}

	bool Visualizer::AddShockwave(
		const glm::vec3& position,
		const glm::vec3& normal,
		float            max_radius,
		float            duration,
		float            intensity,
		float            ring_width,
		const glm::vec3& color
	) {
		return impl->shockwave_manager
			->AddShockwave(position, normal, max_radius, duration, intensity, ring_width, color);
	}

	void Visualizer::TriggerAkira(const glm::vec3& position, float radius) {
		if (impl->akira_effect_manager) {
			impl->akira_effect_manager->Trigger(position, radius);
		}
	}

	int Visualizer::AddSdfSource(const SdfSource& source) {
		return impl->sdf_volume_manager->AddSource(source);
	}

	void Visualizer::UpdateSdfSource(int id, const SdfSource& source) {
		impl->sdf_volume_manager->UpdateSource(id, source);
	}

	void Visualizer::RemoveSdfSource(int id) {
		impl->sdf_volume_manager->RemoveSource(id);
	}

	void Visualizer::ExplodeShape(std::shared_ptr<Shape> shape, float intensity, const glm::vec3& velocity) {
		impl->mesh_explosion_manager->ExplodeShape(shape, intensity, velocity);
	}

	std::shared_ptr<CurvedText> Visualizer::AddCurvedTextEffect(
		const std::string& text,
		const glm::vec3&   position,
		float              radius,
		float              angle_degrees,
		const glm::vec3&   wrap_normal,
		const glm::vec3&   text_normal,
		float              duration,
		const std::string& font_path,
		float              font_size,
		float              depth,
		const glm::vec3&   color
	) {
		auto effect = std::make_shared<CurvedText>(
			text,
			font_path,
			font_size,
			depth,
			position,
			radius,
			angle_degrees,
			wrap_normal,
			text_normal,
			duration
		);
		effect->SetColor(color.r, color.g, color.b);
		impl->transient_effects.push_back(effect);
		return effect;
	}

	std::shared_ptr<ArcadeText> Visualizer::AddArcadeTextEffect(
		const std::string& text,
		const glm::vec3&   position,
		float              radius,
		float              angle_degrees,
		const glm::vec3&   wrap_normal,
		const glm::vec3&   text_normal,
		float              duration,
		const std::string& font_path,
		float              font_size,
		float              depth,
		const glm::vec3&   color
	) {
		auto effect = std::make_shared<ArcadeText>(
			text,
			font_path,
			font_size,
			depth,
			position,
			radius,
			angle_degrees,
			wrap_normal,
			text_normal,
			duration
		);
		effect->SetColor(color.r, color.g, color.b);
		impl->transient_effects.push_back(effect);
		return effect;
	}

	void Visualizer::TriggerComplexExplosion(
		std::shared_ptr<Shape> shape,
		const glm::vec3&       direction,
		float                  intensity,
		FireEffectStyle        fire_style
	) {
		if (!shape)
			return;

		glm::vec3 position(shape->GetX(), shape->GetY(), shape->GetZ());
		glm::vec3 velocity = direction * 20.0f * intensity;

		// 1. Mesh explosion
		impl->mesh_explosion_manager->ExplodeShape(shape, intensity, velocity);

		// 2. Hide original shape
		shape->SetHidden(true);

		// 3. Fire/Glitter effect
		impl->fire_effect_manager->AddEffect(
			position,
			fire_style,
			direction,
			velocity * 0.5f,
			-1, // default particles
			0.5f * intensity
		);

		// 4. Shockwave
		glm::vec3 normal = (glm::length(direction) > 0.001f) ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
		impl->shockwave_manager->AddShockwave(
			position,
			normal,
			30.0f * intensity,
			1.2f * intensity,
			0.5f * intensity,
			4.0f * intensity,
			glm::vec3(shape->GetR(), shape->GetG(), shape->GetB())
		);
	}

	void Visualizer::CreateExplosion(const glm::vec3& position, float intensity) {
		// Add fire effect for the explosion visuals
		impl->fire_effect_manager->AddEffect(
			position,
			FireEffectStyle::Explosion,
			glm::vec3(0.0f), // No specific direction
			glm::vec3(0.0f), // No velocity
			-1,
			0.5f
		);

		// Add some sparks for extra drama
		impl->fire_effect_manager->AddEffect(
			position,
			FireEffectStyle::Sparks,
			glm::vec3(0.0f, 1.0f, 0.0f),
			glm::vec3(0.0f),
			static_cast<int>(300 * intensity),
			0.4f
		);

		// Add the shockwave effect
		float     max_radius = 30.0f * intensity;
		float     duration = Constants::Class::Shockwaves::DefaultDuration() * intensity;
		float     wave_intensity = Constants::Class::Shockwaves::DefaultIntensity() * intensity;
		float     ring_width = (Constants::Class::Shockwaves::DefaultRingWidth() + 1.0f) * intensity;
		glm::vec3 color = Constants::Class::Shockwaves::DefaultColor();

		// Assume explosion is on a flat surface for the shockwave plane
		glm::vec3 normal(0.0f, 1.0f, 0.0f);

		impl->shockwave_manager
			->AddShockwave(position, normal, max_radius, duration, wave_intensity, ring_width, color);
	}

	void Visualizer::CreateShockwave(
		const glm::vec3& center,
		float            intensity,
		float            max_radius,
		float            duration,
		const glm::vec3& normal,
		const glm::vec3& color,
		float            ring_width
	) {
		float wave_intensity = Constants::Class::Shockwaves::DefaultIntensity() * intensity;

		impl->shockwave_manager->AddShockwave(center, normal, max_radius, duration, wave_intensity, ring_width, color);
	}

	void Visualizer::SetTimeScale(float ts) {
		impl->time_scale = ts;
	}

	float Visualizer::GetTimeScale() {
		return impl->time_scale;
	}

	float Visualizer::GetRenderScale() const {
		return impl->render_scale;
	}

	void Visualizer::SetRenderScale(float scale) {
		impl->render_scale = std::clamp(scale, 0.1f, 1.0f);
		impl->ResizeInternalFramebuffers();
	}

	AudioManager& Visualizer::GetAudioManager() {
		return *impl->audio_manager;
	}

	bool Visualizer::IsRippleEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_ripple", false);
	}

	bool Visualizer::IsColorShiftEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_color_shift", false);
	}

	bool Visualizer::IsBlackAndWhiteEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_black_and_white", false);
	}

	bool Visualizer::IsNegativeEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_negative", false);
	}

	bool Visualizer::IsShimmeryEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_shimmery", false);
	}

	bool Visualizer::IsGlitchedEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_glitched", false);
	}

	bool Visualizer::IsWireframeEffectEnabled() const {
		return ConfigManager::GetInstance().GetAppSettingBool("artistic_effect_wireframe", false);
	}
} // namespace Boidsish
