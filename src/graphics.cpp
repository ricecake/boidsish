#include "graphics.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <vector>

#include "ConfigManager.h"
#include "NoiseManager.h"
#include "SceneManager.h"
#include "UIManager.h"
#include "akira_effect.h"
#include "arcade_text.h"
#include "atmosphere_manager.h"
#include "audio_manager.h"
#include "checkpoint_ring.h"
#include "clone_manager.h"
#include "curved_text.h"
#include "decor_manager.h"
#include "dot.h"
#include "entity.h"
#include "fire_effect_manager.h"
#include "frame_data.h"
#include "hiz_manager.h"
#include "hud.h"
#include "hud_manager.h"
#include "light_manager.h"
#include "line.h"
#include "logger.h"
#include "mesh_explosion_manager.h"
#include "path.h"
#include "persistent_buffer.h"
#include "polyhedron.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include "post_processing/effects/BloomEffect.h"
#include "post_processing/effects/FilmGrainEffect.h"
#include "post_processing/effects/GlitchEffect.h"
#include "post_processing/effects/NegativeEffect.h"
#include "post_processing/effects/OpticalFlowEffect.h"
#include "post_processing/effects/SdfVolumeEffect.h"
#include "post_processing/effects/StrobeEffect.h"
#include "post_processing/effects/SuperSpeedEffect.h"
#include "post_processing/effects/UnifiedScreenSpaceEffect.h"
#include "post_processing/effects/TimeStutterEffect.h"
#include "post_processing/effects/ToneMappingEffect.h"
#include "post_processing/effects/WhispTrailEffect.h"
#include "profiler.h"
#include "render_passes.h"
#include "render_queue.h"
#include "scene_compositor.h"
#include "sdf_volume_manager.h"
#include "shader_table.h"
#include "shadow_manager.h"
#include "shadow_render_pass.h"
#include "shockwave_effect.h"
#include "sound_effect_manager.h"
#include "spline.h"
#include "task_thread_pool.hpp"
#include "temporal_data.h"
#include "terrain.h"
#include "terrain_generator.h"
#include "terrain_generator_interface.h"
#include "terrain_render_manager.h"
#include "trail.h"
#include "trail_render_manager.h"
#include "ui/EffectWidget.h"
#include "ui/EnvironmentWidget.h"
#include "ui/ProfilerWidget.h"
#include "ui/RenderWidget.h"
#include "ui/SystemWidget.h"
#include "ui/hud_widget.h"
#include "visual_effects.h"
#include "weather_manager.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_projection.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.h"

namespace Boidsish {

	/**
	 * @brief Registers core C++ constants for use within shaders.
	 * This ensures that shader-side buffers and loops always match C++ expectations.
	 */
	static void RegisterShaderConstants() {
		static bool registered = false;
		if (registered)
			return;

		ShaderBase::RegisterConstant("MAX_LIGHTS", Boidsish::Constants::Class::Shadows::MaxLights());
		ShaderBase::RegisterConstant("MAX_SHADOW_MAPS", Boidsish::Constants::Class::Shadows::MaxShadowMaps());
		ShaderBase::RegisterConstant("MAX_CASCADES", Boidsish::Constants::Class::Shadows::MaxCascades());
		ShaderBase::RegisterConstant("CHUNK_SIZE", Boidsish::Constants::Class::Terrain::ChunkSize());
		ShaderBase::RegisterConstant("CHUNK_SIZE_PLUS_1", Boidsish::Constants::Class::Terrain::ChunkSizePlus1());
		ShaderBase::RegisterConstant("MAX_SHOCKWAVES", Boidsish::Constants::Class::Shockwaves::MaxShockwaves());
		ShaderBase::RegisterConstant("TERRAIN_PROBES_BINDING", Boidsish::Constants::SsboBinding::TerrainProbes());
		ShaderBase::RegisterConstant("TERRAIN_DATA_BINDING", Boidsish::Constants::UboBinding::TerrainData());
		ShaderBase::RegisterConstant("BIOME_DATA_BINDING", Boidsish::Constants::UboBinding::Biomes());
		ShaderBase::RegisterConstant("WEATHER_UNIFORMS_BINDING", Boidsish::Constants::UboBinding::WeatherUniforms());
		ShaderBase::RegisterConstant("WEATHER_GRID_A_BINDING", Boidsish::Constants::SsboBinding::WeatherGridA());
		ShaderBase::RegisterConstant("WEATHER_GRID_B_BINDING", Boidsish::Constants::SsboBinding::WeatherGridB());

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

	/**
	 * @brief Concrete implementation of the Megabuffer interface using AZDO principles.
	 */
	class MegabufferImpl: public Megabuffer {
	public:
		MegabufferImpl(size_t max_vertices, size_t max_indices) {
			vbo_ = std::make_unique<PersistentBuffer<Vertex>>(GL_ARRAY_BUFFER, max_vertices, 3);
			ebo_ = std::make_unique<PersistentBuffer<uint32_t>>(GL_ELEMENT_ARRAY_BUFFER, max_indices, 3);

			glGenVertexArrays(1, &vao_);
			glBindVertexArray(vao_);

			glBindBuffer(GL_ARRAY_BUFFER, vbo_->GetBufferId());
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_->GetBufferId());

			// Set up attributes (matches Vertex)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Color));
			glEnableVertexAttribArray(8);

			// Bone IDs
			glEnableVertexAttribArray(9);
			glVertexAttribIPointer(9, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

			// Bone Weights
			glEnableVertexAttribArray(10);
			glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));

			glBindVertexArray(0);

			// Ensure all attribute arrays are disabled by default on the global VAO
			// (actually they are part of VAO state, so this is fine)

			static_v_limit_ = Constants::Class::Megabuffer::StaticVertexLimit();
			static_i_limit_ = Constants::Class::Megabuffer::StaticIndexLimit();

			dynamic_v_start_ = static_cast<uint32_t>(static_v_limit_);
			dynamic_i_start_ = static_cast<uint32_t>(static_i_limit_);

			dynamic_v_ptr_ = dynamic_v_start_;
			dynamic_i_ptr_ = dynamic_i_start_;
		}

		~MegabufferImpl() override {
			if (vao_)
				glDeleteVertexArrays(1, &vao_);
		}

		MegabufferAllocation AllocateStatic(uint32_t vertex_count, uint32_t index_count) override {
			std::lock_guard<std::mutex> lock(static_mutex_);
			if (static_v_ptr_ + vertex_count > static_v_limit_ || static_i_ptr_ + index_count > static_i_limit_) {
				return {};
			}

			MegabufferAllocation alloc;
			alloc.base_vertex = static_cast<uint32_t>(static_v_ptr_);
			alloc.first_index = static_cast<uint32_t>(static_i_ptr_);
			alloc.vertex_count = vertex_count;
			alloc.index_count = index_count;
			alloc.valid = true;

			static_v_ptr_ += vertex_count;
			static_i_ptr_ += index_count;

			return alloc;
		}

		MegabufferAllocation AllocateDynamic(uint32_t vertex_count, uint32_t index_count) override {
			uint32_t v_offset = dynamic_v_ptr_.fetch_add(vertex_count);
			uint32_t i_offset = dynamic_i_ptr_.fetch_add(index_count);

			if (v_offset + vertex_count > vbo_->GetElementCount() || i_offset + index_count > ebo_->GetElementCount()) {
				return {};
			}

			MegabufferAllocation alloc;
			alloc.base_vertex = v_offset;
			alloc.first_index = i_offset;
			alloc.vertex_count = vertex_count;
			alloc.index_count = index_count;
			alloc.valid = true;

			return alloc;
		}

		void Upload(
			const MegabufferAllocation& alloc,
			const Vertex*               vertices,
			uint32_t                    v_count,
			const uint32_t*             indices = nullptr,
			uint32_t                    i_count = 0
		) override {
			if (!alloc.valid)
				return;

			// Bounds checking to prevent buffer overflows
			assert(alloc.base_vertex + v_count <= vbo_->GetElementCount() && "Vertex buffer overflow in Upload()");
			if (indices && i_count > 0) {
				assert(alloc.first_index + i_count <= ebo_->GetElementCount() && "Index buffer overflow in Upload()");
			}

			// If it's a static allocation (base_vertex < dynamic_v_start_), upload to ALL 3 segments.
			// Otherwise upload only to the current frame's segment.

			bool is_static = (alloc.base_vertex < dynamic_v_start_);

			if (is_static) {
				for (int i = 0; i < 3; ++i) {
					Vertex* v_ptr = vbo_->GetFullBufferPtr() + (i * vbo_->GetElementCount()) + alloc.base_vertex;
					memcpy(v_ptr, vertices, v_count * sizeof(Vertex));

					if (indices && i_count > 0) {
						uint32_t* i_ptr = ebo_->GetFullBufferPtr() + (i * ebo_->GetElementCount()) + alloc.first_index;
						memcpy(i_ptr, indices, i_count * sizeof(uint32_t));
					}
				}
			} else {
				// Dynamic upload to current frame
				Vertex* v_ptr = vbo_->GetFrameDataPtr() + alloc.base_vertex;
				memcpy(v_ptr, vertices, v_count * sizeof(Vertex));

				if (indices && i_count > 0) {
					uint32_t* i_ptr = ebo_->GetFrameDataPtr() + alloc.first_index;
					memcpy(i_ptr, indices, i_count * sizeof(uint32_t));
				}
			}
		}

		uint32_t GetVAO() const override { return vao_; }

		void AdvanceFrame() {
			vbo_->AdvanceFrame();
			ebo_->AdvanceFrame();

			// Reset dynamic pointers for the new frame
			dynamic_v_ptr_ = dynamic_v_start_;
			dynamic_i_ptr_ = dynamic_i_start_;
		}

		uint32_t GetVertexFrameOffset() const { return vbo_->GetCurrentBufferIndex() * vbo_->GetElementCount(); }

		uint32_t GetIndexFrameOffset() const { return ebo_->GetCurrentBufferIndex() * ebo_->GetElementCount(); }

	private:
		std::unique_ptr<PersistentBuffer<Vertex>>   vbo_;
		std::unique_ptr<PersistentBuffer<uint32_t>> ebo_;
		GLuint                                      vao_ = 0;

		size_t     static_v_ptr_ = 0;
		size_t     static_i_ptr_ = 0;
		size_t     static_v_limit_;
		size_t     static_i_limit_;
		std::mutex static_mutex_;

		std::atomic<uint32_t> dynamic_v_ptr_;
		std::atomic<uint32_t> dynamic_i_ptr_;
		uint32_t              dynamic_v_start_;
		uint32_t              dynamic_i_start_;
	};

	struct Visualizer::VisualizerImpl {
		Visualizer*                                       parent;
		GLFWwindow*                                       window;
		int                                               width, height;
		Camera                                            camera;
		std::vector<ShapeFunction>                        shape_functions;
		std::vector<std::shared_ptr<Shape>>               shapes;            // Legacy shapes from callbacks
		std::map<int, std::shared_ptr<Shape>>             persistent_shapes; // New persistent shapes
		std::vector<std::shared_ptr<Shape>>               transient_effects; // Short-lived effects like CurvedText
		ConcurrentQueue<ShapeCommand>                     shape_command_queue;
		std::unique_ptr<CloneManager>                     clone_manager;
		std::unique_ptr<FireEffectManager>                fire_effect_manager;
		std::unique_ptr<NoiseManager>                     noise_manager;
		std::unique_ptr<MeshExplosionManager>             mesh_explosion_manager;
		std::unique_ptr<SoundEffectManager>               sound_effect_manager;
		std::unique_ptr<ShockwaveManager>                 shockwave_manager;
		std::unique_ptr<AkiraEffectManager>               akira_effect_manager;
		std::unique_ptr<SdfVolumeManager>                 sdf_volume_manager;
		std::unique_ptr<ShadowManager>                    shadow_manager;
		std::unique_ptr<AtmosphereManager>                atmosphere_manager;
		std::shared_ptr<PostProcessing::AtmosphereEffect> atmosphere_effect;
		std::unique_ptr<WeatherManager>                   weather_manager;
		std::unique_ptr<SceneManager>                     scene_manager;
		std::shared_ptr<DecorManager>                     decor_manager;
		std::map<int, std::shared_ptr<Trail>>             trails;
		std::map<int, float>                              trail_last_update;
		LightManager                                      light_manager;
		ShaderTable                                       shader_table;
		RenderQueue                                       render_queue;

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

		std::unique_ptr<MegabufferImpl> megabuffer;

		// Persistent buffers for MDI
		std::unique_ptr<PersistentBuffer<DrawElementsIndirectCommand>> indirect_elements_buffer;
		std::unique_ptr<PersistentBuffer<DrawArraysIndirectCommand>>   indirect_arrays_buffer;
		std::unique_ptr<PersistentBuffer<CommonUniforms>>              uniforms_ssbo;
		std::unique_ptr<PersistentBuffer<FrustumDataGPU>>              frustum_ssbo;
		std::unique_ptr<PersistentBuffer<glm::mat4>>                   bone_matrices_ssbo;
		GLsync                                                         mdi_fences[3]{0, 0, 0};

		// MDI offset tracking across layers within a single frame
		uint32_t mdi_elements_count = 0;
		uint32_t mdi_arrays_count = 0;
		uint32_t mdi_uniform_count = 0;
		uint32_t mdi_frustum_count = 0;
		uint32_t mdi_bone_count = 0;

		// Hi-Z occlusion culling
		std::unique_ptr<HiZManager>    hiz_manager;
		std::unique_ptr<ComputeShader> occlusion_cull_shader_;
		GLuint                         occlusion_visibility_ssbo_{0};
		bool                           enable_hiz_culling_{true};

		std::shared_ptr<Shader> shader;
		std::shared_ptr<Shader> plane_shader;
		std::shared_ptr<Shader> sky_shader;
		std::shared_ptr<Shader> trail_shader;
		ShaderHandle            trail_shader_handle;
		std::shared_ptr<Shader> postprocess_shader_;
		ShaderHandle            shadow_shader_handle{0};
		GLuint                  plane_vao{0}, plane_vbo{0}, sky_vao{0}, blur_quad_vao{0}, blur_quad_vbo{0};
		GLuint                  lighting_ubo{0};
		GLuint                  visual_effects_ubo{0};
		GLuint                  temporal_data_ubo{0};
		glm::mat4               projection;
		glm::mat4               prev_view_projection{1.0f};

		double last_mouse_x = 0.0, last_mouse_y = 0.0;
		bool   first_mouse = true;

		bool                                           paused = false;
		float                                          simulation_time = 0.0f;
		float                                          simulation_delta_time = 0.0f;
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

		std::vector<CameraState> camera_state_stack_;

		// Camera shake state
		float     shake_intensity = 0.0f;
		float     shake_timer = 0.0f;
		float     shake_duration = 0.0f;
		glm::vec3 shake_offset{0.0f};

		// Cached global settings
		float camera_roll_speed_;
		float camera_speed_step_;
		bool  enable_hdr_ = true;

		uint64_t frame_count_ = 0;

		// Performance optimization: batched UBO updates and config caching
		std::vector<LightGPU> gpu_lights_cache_;
		LightingUbo           lighting_ubo_data_;
		FrameConfigCache      frame_config_;

		bool last_render_terrain_ = true;
		bool last_render_floor_ = true;

		task_thread_pool::task_thread_pool thread_pool;
		std::unique_ptr<AudioManager>      audio_manager;

		// Canonical frame data — temporal chain lives here between frames
		FrameData current_frame_;

		// Extracted render passes and compositor
		std::unique_ptr<ShadowRenderPass>    shadow_pass_;
		std::unique_ptr<SceneCompositor>     compositor_;
		std::unique_ptr<OpaqueScenePass>     opaque_pass_;
		std::unique_ptr<EarlyEffectsPass>    early_effects_pass_;
		std::unique_ptr<ParticleEffectsPass> particle_pass_;
		std::unique_ptr<TransparentPass>     transparent_pass_;

		// Scene center computed by shadow pass, used by other systems
		glm::vec3                      scene_center{0.0f};
		Frustum                        generator_frustum;
		glm::mat4                      current_view_matrix{1.0f};
		std::vector<std::future<void>> pending_packet_futures;

		VisualizerImpl(Visualizer* p, int w, int h, const char* title): parent(p), width(w), height(h) {
			RegisterShaderConstants();

			ConfigManager::GetInstance().Initialize(title);
			enable_hdr_ = ConfigManager::GetInstance().GetAppSettingBool("enable_hdr", true);

			last_render_terrain_ = ConfigManager::GetInstance().GetAppSettingBool("render_terrain", true);
			last_render_floor_ = ConfigManager::GetInstance().GetAppSettingBool("render_floor", true);

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
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6); // Updated to 4.6 for MDI and gl_DrawID support
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
			trail_shader_handle = shader_table.Register(std::make_unique<RenderShader>(trail_shader));

			if (ConfigManager::GetInstance().GetAppSettingBool("enable_floor", true)) {
				plane_shader = std::make_shared<Shader>("shaders/plane.vert", "shaders/plane.frag");
				shader_table.Register(std::make_unique<RenderShader>(plane_shader));
			}
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_skybox", true)) {
				sky_shader = std::make_shared<Shader>("shaders/sky.vert", "shaders/sky.frag");
				shader_table.Register(std::make_unique<RenderShader>(sky_shader));
			}

			// Initialize persistent buffers for MDI
			// Capacity: 16384 commands/uniforms per frame (triple buffered)
			megabuffer = std::make_unique<MegabufferImpl>(
				Constants::Class::Megabuffer::MaxVertices(),
				Constants::Class::Megabuffer::MaxIndices()
			);

			indirect_elements_buffer = std::make_unique<PersistentBuffer<DrawElementsIndirectCommand>>(
				GL_DRAW_INDIRECT_BUFFER,
				65536
			);
			indirect_arrays_buffer = std::make_unique<PersistentBuffer<DrawArraysIndirectCommand>>(
				GL_DRAW_INDIRECT_BUFFER,
				65536
			);
			uniforms_ssbo = std::make_unique<PersistentBuffer<CommonUniforms>>(GL_SHADER_STORAGE_BUFFER, 65536);
			frustum_ssbo = std::make_unique<PersistentBuffer<FrustumDataGPU>>(GL_UNIFORM_BUFFER, 64);
			bone_matrices_ssbo = std::make_unique<PersistentBuffer<glm::mat4>>(GL_SHADER_STORAGE_BUFFER, 65536);

			// Hi-Z occlusion culling
			hiz_manager = std::make_unique<HiZManager>();
			occlusion_cull_shader_ = std::make_unique<ComputeShader>("shaders/occlusion_cull.comp");
			if (!occlusion_cull_shader_->isValid()) {
				logger::WARNING("Hi-Z occlusion culling shader failed to compile - disabling");
				enable_hiz_culling_ = false;
			}
			// Create visibility SSBO (GPU-only buffer, written by compute, read by vertex shader)
			glGenBuffers(1, &occlusion_visibility_ssbo_);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, occlusion_visibility_ssbo_);
			std::vector<uint32_t> initial_vis(65536, 1u); // All visible initially
			glBufferStorage(GL_SHADER_STORAGE_BUFFER, 65536 * sizeof(uint32_t), initial_vis.data(), 0);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_effects", true)) {
				postprocess_shader_ = std::make_shared<Shader>("shaders/postprocess.vert", "shaders/postprocess.frag");
				shader_table.Register(std::make_unique<RenderShader>(postprocess_shader_));
			}
			if (ConfigManager::GetInstance().GetAppSettingBool("enable_terrain", true)) {
				terrain_generator = std::make_shared<TerrainGenerator>();
				last_camera_yaw_ = camera.yaw;
				last_camera_pitch_ = camera.pitch;
			}
			noise_manager = std::make_unique<NoiseManager>();
			noise_manager->Initialize();
			clone_manager = std::make_unique<CloneManager>();
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
			atmosphere_manager = std::make_unique<AtmosphereManager>();
			atmosphere_manager->Initialize();
			weather_manager = std::make_unique<WeatherManager>(terrain_generator.get());
			audio_manager = std::make_unique<AudioManager>();
			sound_effect_manager = std::make_unique<SoundEffectManager>(audio_manager.get());
			trail_render_manager = std::make_unique<TrailRenderManager>();

			const int MAX_LIGHTS = 10;
			glGenBuffers(1, &lighting_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
			glBufferData(GL_UNIFORM_BUFFER, sizeof(LightingUbo), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			glBindBufferRange(
				GL_UNIFORM_BUFFER,
				Constants::UboBinding::Lighting(),
				lighting_ubo,
				0,
				sizeof(LightingUbo)
			);

			// Temporal Data UBO
			glGenBuffers(1, &temporal_data_ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, temporal_data_ubo);
			glBufferData(GL_UNIFORM_BUFFER, sizeof(TemporalUbo), NULL, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			glBindBufferRange(
				GL_UNIFORM_BUFFER,
				Constants::UboBinding::TemporalData(),
				temporal_data_ubo,
				0,
				sizeof(TemporalUbo)
			);

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

			shader->use();
			SetupShaderBindings(*shader);
			SetupShaderBindings(*trail_shader);

			CheckpointRingShape::SetShader(
				std::make_shared<Shader>("shaders/checkpoint.vert", "shaders/checkpoint.frag")
			);
			CheckpointRingShape::checkpoint_shader_handle = shader_table.Register(
				std::make_unique<RenderShader>(CheckpointRingShape::GetShader())
			);
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
				Terrain::terrain_shader_handle = shader_table.Register(
					std::make_unique<RenderShader>(Terrain::terrain_shader_)
				);
				SetupShaderBindings(*Terrain::terrain_shader_);

				// Create the terrain render manager
				// Query GPU for max texture array layers and use a safe initial allocation
				// Chunk size must match terrain_generator.h (32)
				GLint max_layers = 0;
				glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
				int initial_chunks = max_layers > 0 ? std::min(max_layers, 8192) : 1024;
				logger::LOG(
					"Terrain render manager: GPU supports " + std::to_string(max_layers) +
					" texture array layers, using " + std::to_string(initial_chunks)
				);
				terrain_render_manager = std::make_shared<TerrainRenderManager>(
					Constants::Class::Terrain::ChunkSize(),
					initial_chunks
				);
				terrain_generator->SetRenderManager(terrain_render_manager);
				terrain_render_manager->SetNoise(
					noise_manager->GetNoiseTexture(),
					noise_manager->GetCurlTexture(),
					noise_manager->GetExtraNoiseTexture()
				);

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

			Shape::InitSphereMesh(megabuffer.get());
			Line::InitLineMesh(megabuffer.get());
			CheckpointRingShape::InitQuadMesh(megabuffer.get());

			if (postprocess_shader_) {
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
			}

			// --- Main Scene Framebuffer (owned by SceneCompositor) ---
			compositor_ = std::make_unique<SceneCompositor>(render_width, render_height, enable_hdr_, blur_quad_vao);

			// FBOs now fully owned by compositor_

			// --- Shadow Manager (initialize unconditionally) ---
			shadow_manager->Initialize();
			SetupShaderBindings(*shadow_manager->GetShadowShaderPtr());
			shadow_shader_handle = shader_table.Register(
				std::make_unique<RenderShader>(shadow_manager->GetShadowShaderPtr())
			);

			// Shadow render pass — created here but decor/noise/terrain references
			// are set later via SetShadowPassDependencies() once those managers exist.

			// Initialize Hi-Z manager with render dimensions
			hiz_manager->Initialize(render_width, render_height);

			if (postprocess_shader_) {
				// --- Post Processing Manager ---
				post_processing_manager_ = std::make_unique<PostProcessing::PostProcessingManager>(
					render_width,
					render_height,
					blur_quad_vao
				);
				post_processing_manager_->Initialize();
				post_processing_manager_->SetSharedDepthTexture(compositor_->GetDepthTexture());

				// --- Shockwave Manager ---
				shockwave_manager->Initialize(render_width, render_height);

				auto unified_ss_effect = std::make_shared<PostProcessing::UnifiedScreenSpaceEffect>();
				unified_ss_effect->SetEnabled(true);
				if (noise_manager) {
					unified_ss_effect->SetBlueNoiseTexture(noise_manager->GetBlueNoiseTexture());
				}
				post_processing_manager_->AddEffect(unified_ss_effect);

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

				atmosphere_effect = std::make_shared<PostProcessing::AtmosphereEffect>();
				atmosphere_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(atmosphere_effect);

				auto bloom_effect = std::make_shared<PostProcessing::BloomEffect>(render_width, render_height);
				bloom_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(bloom_effect);

				auto sdf_volume_effect = std::make_shared<PostProcessing::SdfVolumeEffect>();
				sdf_volume_effect->SetEnabled(true);
				post_processing_manager_->AddEffect(sdf_volume_effect);

				if (enable_hdr_) {
					// Enable integrated tonemapping in bloom effect
					bloom_effect->SetToneMappingEnabled(true);
					bloom_effect->SetToneMappingMode(2); // Lottes default

					// Still create the standalone effect as a fallback or for when bloom is disabled
					auto tone_mapping_effect = std::make_shared<PostProcessing::ToneMappingEffect>();
					tone_mapping_effect->SetEnabled(true);
					bloom_effect->SetEnabled(true);
					post_processing_manager_->SetToneMappingEffect(tone_mapping_effect);
				}

				// --- UI ---
			}

			ui_manager->AddWidget(std::make_shared<UI::EnvironmentWidget>(*parent));
			ui_manager->AddWidget(std::make_shared<UI::EffectWidget>(*parent));
			ui_manager->AddWidget(std::make_shared<UI::RenderWidget>(*parent));
			ui_manager->AddWidget(std::make_shared<UI::SystemWidget>(*parent, *scene_manager));
			ui_manager->AddWidget(std::make_shared<UI::ProfilerWidget>());
		}

		void BindShadows(Shader& s) {
			s.use();
			if (terrain_render_manager) {
				terrain_render_manager->BindTerrainData(s);
			}

			if (atmosphere_manager) {
				atmosphere_manager->BindToShader(s);
			}

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

		void SetupShaderBindings(ShaderBase& shader_to_setup) {
			shader_to_setup.use();
			if (terrain_render_manager) {
				terrain_render_manager->BindTerrainData(shader_to_setup);
			}
			if (atmosphere_manager) {
				atmosphere_manager->BindToShader(shader_to_setup);
			}
			shader_to_setup.setBool("uUseMDI", false);
			shader_to_setup.setBool("useSSBOInstancing", false);
			shader_to_setup.setBool("use_skinning", false);
			shader_to_setup.setInt("bone_matrices_offset", -1);
			shader_to_setup.setMat4("model", glm::mat4(1.0f));
			shader_to_setup.setVec4("clipPlane", glm::vec4(0.0f));
			if (noise_manager) {
				noise_manager->BindDefault(shader_to_setup);
			}
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
			GLuint temporal_idx = glGetUniformBlockIndex(shader_to_setup.ID, "TemporalData");
			if (temporal_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, temporal_idx, Constants::UboBinding::TemporalData());
			}
			GLuint terrain_idx = glGetUniformBlockIndex(shader_to_setup.ID, "TerrainData");
			if (terrain_idx != GL_INVALID_INDEX) {
				glUniformBlockBinding(shader_to_setup.ID, terrain_idx, Constants::UboBinding::TerrainData());
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
			frame_config_.force_both_floor_and_terrain = cfg.GetAppSettingBool("force_both_floor_and_terrain", false);

			// Mutual exclusion logic
			if (!frame_config_.force_both_floor_and_terrain) {
				bool terrain_changed = (frame_config_.render_terrain != last_render_terrain_);
				bool floor_changed = (frame_config_.render_floor != last_render_floor_);

				if (frame_config_.render_terrain && frame_config_.render_floor) {
					if (terrain_changed) {
						frame_config_.render_floor = false;
						cfg.SetBool("render_floor", false);
					} else if (floor_changed) {
						frame_config_.render_terrain = false;
						cfg.SetBool("render_terrain", false);
					} else {
						// Both enabled but neither just changed (initial state or both forced on then force_both
						// disabled) Default to floor off if both are on (favor terrain)
						frame_config_.render_floor = false;
						cfg.SetBool("render_floor", false);
					}
				}
			}
			last_render_terrain_ = frame_config_.render_terrain;
			last_render_floor_ = frame_config_.render_floor;

			frame_config_.render_decor = cfg.GetAppSettingBool("render_decor", true);
			frame_config_.artistic_ripple = cfg.GetAppSettingBool("artistic_effect_ripple", false);
			frame_config_.artistic_color_shift = cfg.GetAppSettingBool("artistic_effect_color_shift", false);
			frame_config_.artistic_black_and_white = cfg.GetAppSettingBool("artistic_effect_black_and_white", false);
			frame_config_.artistic_negative = cfg.GetAppSettingBool("artistic_effect_negative", false);
			frame_config_.artistic_shimmery = cfg.GetAppSettingBool("artistic_effect_shimmery", false);
			frame_config_.artistic_glitched = cfg.GetAppSettingBool("artistic_effect_glitched", false);
			frame_config_.artistic_wireframe = cfg.GetAppSettingBool("artistic_effect_wireframe", false);
			frame_config_.erosion_enabled = cfg.GetAppSettingBool("erosion_enabled", true);
			frame_config_.erosion_strength = cfg.GetAppSettingFloat("erosion_strength", 0.12f);
			frame_config_.erosion_scale = cfg.GetAppSettingFloat("erosion_scale", 0.15f);
			frame_config_.erosion_detail = cfg.GetAppSettingFloat("erosion_detail", 1.5f);
			frame_config_.erosion_gully_weight = cfg.GetAppSettingFloat("erosion_gully_weight", 0.5f);
			frame_config_.erosion_max_dist = cfg.GetAppSettingFloat("erosion_max_dist", 450.0f);
			frame_config_.ambient_particle_density = cfg.GetAppSettingFloat(
				"ambient_particle_density",
				Constants::Class::Particles::DefaultAmbientDensity()
			);
			frame_config_.enable_shadows = cfg.GetAppSettingBool("enable_shadows", true);
			frame_config_.wind_strength = cfg.GetAppSettingFloat("wind_strength", 0.065f);
			frame_config_.wind_speed = cfg.GetAppSettingFloat("wind_speed", 0.075f);
			frame_config_.wind_frequency = cfg.GetAppSettingFloat("wind_frequency", 0.01f);

			if (decor_manager) {
				decor_manager->SetEnabled(frame_config_.render_decor);
			}
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
			Polyhedron::DestroyPolyhedronMeshes();
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

			// FBOs owned by compositor_ — cleaned up by its destructor
			compositor_.reset();

			if (lighting_ubo) {
				glDeleteBuffers(1, &lighting_ubo);
			}

			if (visual_effects_ubo) {
				glDeleteBuffers(1, &visual_effects_ubo);
			}

			if (temporal_data_ubo) {
				glDeleteBuffers(1, &temporal_data_ubo);
			}

			if (occlusion_visibility_ssbo_) {
				glDeleteBuffers(1, &occlusion_visibility_ssbo_);
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

		void UpdateFrustumUbo(const glm::mat4& view_mat, const glm::mat4& proj_mat, const glm::vec3& cam_pos) {
			if (mdi_frustum_count >= frustum_ssbo->GetElementCount())
				return;

			Frustum         pass_frustum = Frustum::FromViewProjection(view_mat, proj_mat);
			FrustumDataGPU* frustum_ptr = frustum_ssbo->GetFrameDataPtr();
			FrustumDataGPU& data = frustum_ptr[mdi_frustum_count];

			for (int i = 0; i < 6; ++i) {
				data.planes[i] = glm::vec4(pass_frustum.planes[i].normal, pass_frustum.planes[i].distance);
			}
			data.camera_pos = cam_pos;

			glBindBufferRange(
				GL_UNIFORM_BUFFER,
				Constants::UboBinding::FrustumData(),
				frustum_ssbo->GetBufferId(),
				frustum_ssbo->GetFrameOffset() + mdi_frustum_count * sizeof(FrustumDataGPU),
				sizeof(FrustumDataGPU)
			);
			mdi_frustum_count++;
		}

		glm::mat4 SetupMatrices(const Camera& cam_to_use) {
			float world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
			float far_plane = Constants::Project::Camera::DefaultFarPlane() * std::max(1.0f, world_scale);
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

		void ExecuteRenderQueue(
			RenderQueue&                       queue,
			const glm::mat4&                   view_mat,
			const glm::mat4&                   proj_mat,
			const glm::vec3&                   camera_pos,
			RenderLayer                        layer,
			const std::optional<ShaderHandle>& shader_override = std::nullopt,
			const std::optional<glm::mat4>&    light_space_mat = std::nullopt,
			const std::optional<glm::vec4>&    clip_plane = std::nullopt,
			bool                               is_shadow_pass = false,
			bool                               dispatch_hiz_occlusion = false
		) {
			PROJECT_PROFILE_SCOPE(
				layer == RenderLayer::Opaque ? "ExecuteRenderQueue::Opaque" : "ExecuteRenderQueue::Transparent"
			);
			// Integrate trails into the render queue for the transparent layer
			if (layer == RenderLayer::Transparent && trail_render_manager && !is_shadow_pass) {
				RenderContext context;
				context.view = view_mat;
				context.projection = proj_mat;
				context.view_pos = camera_pos;
				context.frustum = Frustum::FromViewProjection(view_mat, proj_mat);

				trail_render_manager
					->GetRenderPackets(queue.GetPacketsMutable(RenderLayer::Transparent), context, trail_shader_handle);
				// Re-sort might be needed but typically transparent is sorted anyway.
				// For now, assume it's fine or re-sort.
			}

			const auto& packets = queue.GetPackets(layer);
			if (packets.empty())
				return;

			// Update Frustum UBO for GPU-side culling for this specific pass
			if (!is_shadow_pass) {
				UpdateFrustumUbo(view_mat, proj_mat, camera_pos);
			}

			// Get pointers to persistent buffers
			DrawElementsIndirectCommand* elements_cmd_ptr = indirect_elements_buffer->GetFrameDataPtr();
			DrawArraysIndirectCommand*   arrays_cmd_ptr = indirect_arrays_buffer->GetFrameDataPtr();
			CommonUniforms*              uniforms_ptr = uniforms_ssbo->GetFrameDataPtr();
			glm::mat4*                   bones_ptr = bone_matrices_ssbo->GetFrameDataPtr();

			uint32_t max_elements = 65536; // Buffer capacity
			uint32_t frame_element_offset = uniforms_ssbo->GetCurrentBufferIndex() * max_elements;

			uint32_t vertex_frame_offset = megabuffer->GetVertexFrameOffset();
			uint32_t index_frame_offset = megabuffer->GetIndexFrameOffset();

			// We bind SSBO per-batch using glBindBufferRange to set the base uniform index
			// glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, uniforms_ssbo->GetBufferId());

			struct Batch {
				ShaderHandle                           shader_handle;
				unsigned int                           shader_id;
				unsigned int                           vao;
				unsigned int                           draw_mode;
				unsigned int                           index_type;
				bool                                   no_cull;
				std::vector<RenderPacket::TextureInfo> textures;
				uint32_t                               first_command;
				uint32_t                               command_count;
				bool                                   is_indexed;
				uint32_t                               base_uniform_index;
			};

			std::vector<Batch> batches;
			batches.reserve(packets.size() / 4); // Pre-allocate for typical batch reduction ratio

			auto can_batch = [&](const RenderPacket& a, const RenderPacket& b) {
				// 1. Mandatory breaks: VAO and Draw State
				if (a.vao != b.vao)
					return false;
				if (a.draw_mode != b.draw_mode)
					return false;
				if (a.index_type != b.index_type)
					return false;
				if ((a.index_count > 0) != (b.index_count > 0))
					return false;
				if (a.no_cull != b.no_cull)
					return false;

				// 2. Shader breaks (unless overridden)
				if (!shader_override.has_value()) {
					if (a.shader_id != b.shader_id)
						return false;
				}

				// 3. Uniform flags that affect vertex processing
				if (a.uniforms.is_colossal != b.uniforms.is_colossal)
					return false;
				if (a.uniforms.use_ssbo_instancing != b.uniforms.use_ssbo_instancing)
					return false;
				if (a.uniforms.use_skinning != b.uniforms.use_skinning)
					return false;
				if (a.uniforms.bone_matrices_offset != b.uniforms.bone_matrices_offset)
					return false;

				// 4. Textures (only if not a shadow pass)
				if (!is_shadow_pass) {
					if (a.textures.size() != b.textures.size())
						return false;
					for (size_t i = 0; i < a.textures.size(); ++i) {
						if (a.textures[i].id != b.textures[i].id)
							return false;
					}
				}

				return true;
			};

			// 1. Build batches and fill uniform/command buffers
			const RenderPacket* last_processed_packet = nullptr;

			for (size_t i = 0; i < packets.size(); ++i) {
				const auto& packet = packets[i];

				// Filter for shadow pass
				if (is_shadow_pass && !packet.casts_shadows)
					continue;

				// Skip packets with invalid shader or exhausted uniform buffer
				if (packet.shader_id == 0)
					continue;
				if (mdi_uniform_count >= max_elements) {
					static bool uniform_warning_logged = false;
					if (!uniform_warning_logged) {
						logger::WARNING("MDI uniform buffer exhausted - some objects may not render");
						uniform_warning_logged = true;
					}
					continue;
				}

				bool is_indexed = (packet.index_count > 0);

				// Safety check for command buffer capacity
				if (is_indexed && mdi_elements_count >= max_elements) {
					static bool elements_warning_logged = false;
					if (!elements_warning_logged) {
						logger::WARNING("MDI indexed command buffer exhausted");
						elements_warning_logged = true;
					}
					continue;
				}
				if (!is_indexed && mdi_arrays_count >= max_elements) {
					static bool arrays_warning_logged = false;
					if (!arrays_warning_logged) {
						logger::WARNING("MDI array command buffer exhausted");
						arrays_warning_logged = true;
					}
					continue;
				}

				// Copy uniforms to persistent SSBO
				uniforms_ptr[mdi_uniform_count] = packet.uniforms;

				// Handle skeletal animation data
				if (!packet.bone_matrices.empty()) {
					uint32_t bone_count = static_cast<uint32_t>(packet.bone_matrices.size());
					if (mdi_bone_count + bone_count <= 65536) {
						std::memcpy(
							&bones_ptr[mdi_bone_count],
							packet.bone_matrices.data(),
							bone_count * sizeof(glm::mat4)
						);
						uniforms_ptr[mdi_uniform_count].bone_matrices_offset = (int)mdi_bone_count;
						mdi_bone_count += bone_count;
					}
				}

				if (batches.empty() || !last_processed_packet || !can_batch(*last_processed_packet, packet)) {
					Batch new_batch;
					new_batch.shader_handle = shader_override.value_or(packet.shader_handle);
					new_batch.shader_id = is_shadow_pass ? 999999 : packet.shader_id; // Dummy ID if overridden
					new_batch.vao = packet.vao;
					new_batch.draw_mode = packet.draw_mode;
					new_batch.index_type = packet.index_type;
					new_batch.no_cull = packet.no_cull;
					new_batch.textures = packet.textures;
					new_batch.first_command = is_indexed ? mdi_elements_count : mdi_arrays_count;
					new_batch.command_count = 0;
					new_batch.is_indexed = is_indexed;
					new_batch.base_uniform_index = frame_element_offset + mdi_uniform_count;
					batches.push_back(new_batch);
				}

				auto& current_batch = batches.back();
				current_batch.command_count++;

				bool uses_megabuffer = (packet.vao == megabuffer->GetVAO());

				if (is_indexed) {
					DrawElementsIndirectCommand cmd{};
					bool                        use_shadow_indices = (is_shadow_pass && packet.shadow_index_count > 0);
					cmd.count = use_shadow_indices ? packet.shadow_index_count : packet.index_count;
					cmd.instanceCount = std::max(1, packet.instance_count);
					cmd.firstIndex = (use_shadow_indices ? packet.shadow_first_index : packet.first_index) +
						(uses_megabuffer ? index_frame_offset : 0);
					cmd.baseVertex = static_cast<int32_t>(
						packet.base_vertex + (uses_megabuffer ? vertex_frame_offset : 0)
					);
					cmd.baseInstance = 0;
					elements_cmd_ptr[mdi_elements_count++] = cmd;
				} else {
					DrawArraysIndirectCommand cmd{};
					cmd.count = packet.vertex_count;
					cmd.instanceCount = std::max(1, packet.instance_count);
					cmd.first = packet.base_vertex + (uses_megabuffer ? vertex_frame_offset : 0);
					cmd.baseInstance = 0;
					arrays_cmd_ptr[mdi_arrays_count++] = cmd;
				}

				last_processed_packet = &packet;
				mdi_uniform_count++;
			}

			// Ensure all CPU writes to persistent mapped buffers are visible to GPU
			glMemoryBarrier(
				GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT
			);

			// Hi-Z occlusion culling dispatch (between uniform fill and draw calls)
			if (dispatch_hiz_occlusion && occlusion_cull_shader_ && occlusion_cull_shader_->isValid() && hiz_manager &&
			    hiz_manager->IsInitialized() && mdi_uniform_count > 0) {
				occlusion_cull_shader_->use();

				// Bind uniforms SSBO (current frame's data) for compute to read AABBs
				glBindBufferRange(
					GL_SHADER_STORAGE_BUFFER,
					2,
					uniforms_ssbo->GetBufferId(),
					frame_element_offset * sizeof(CommonUniforms),
					mdi_uniform_count * sizeof(CommonUniforms)
				);

				// Bind visibility SSBO for compute to write
				glBindBufferBase(
					GL_SHADER_STORAGE_BUFFER,
					Constants::SsboBinding::OcclusionVisibility(),
					occlusion_visibility_ssbo_
				);

				// Bind Hi-Z texture
				glActiveTexture(GL_TEXTURE15);
				glBindTexture(GL_TEXTURE_2D, hiz_manager->GetHiZTexture());
				occlusion_cull_shader_->setInt("u_hizTexture", 15);

				// Set uniforms
				occlusion_cull_shader_->setInt("u_drawCount", static_cast<int>(mdi_uniform_count));
				glUniform2i(
					glGetUniformLocation(occlusion_cull_shader_->ID, "u_hizSize"),
					hiz_manager->GetWidth(),
					hiz_manager->GetHeight()
				);
				occlusion_cull_shader_->setInt("u_hizMipCount", hiz_manager->GetMipCount());
				occlusion_cull_shader_->setFloat("u_screenExpansion", 4.0f);

				glDispatchCompute((mdi_uniform_count + 63) / 64, 1, 1);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
			}

			// 2. Execute batches
			unsigned int          current_vao = 0;
			unsigned int          current_bound_shader_id = 0;
			std::set<ShaderBase*> used_shaders;

			for (const auto& batch : batches) {
				RenderShader* render_shader = shader_table.Get(batch.shader_handle);
				if (!render_shader)
					continue;

				ShaderBase* s = render_shader->GetBackingShader().get();
				used_shaders.insert(s);

				if (s->ID != current_bound_shader_id) {
					s->use();
					current_bound_shader_id = s->ID;
					s->setMat4("view", view_mat);
					s->setMat4("projection", proj_mat);
					s->setFloat("time", simulation_time);
					s->setBool("enableFrustumCulling", !is_shadow_pass);
					s->setBool("enableHiZCulling", dispatch_hiz_occlusion && !is_shadow_pass);
					if (light_space_mat) {
						s->setMat4("lightSpaceMatrix", *light_space_mat);
					}
					if (clip_plane) {
						s->setVec4("clipPlane", *clip_plane);
					} else {
						s->setVec4("clipPlane", glm::vec4(0, 0, 0, 0));
					}

					if (!is_shadow_pass) {
						// Bind refraction texture to a fixed unit (14) if not a shadow pass
						glActiveTexture(GL_TEXTURE14);
						glBindTexture(GL_TEXTURE_2D, compositor_->GetRefractionTexture());
						s->trySetInt("refractionTexture", 14);

						if (atmosphere_manager) {
							s->trySetInt("u_transmittanceLUT", 20);
							s->trySetInt("u_skyViewLUT", 22);
							s->trySetInt("u_aerialPerspectiveLUT", 23);
							s->trySetFloat("u_atmosphereHeight", atmosphere_manager->GetAtmosphereHeight());
						}
					}
				}

				// s->setBool("uUseMDI", true); // Moved below SSBO binding

				// Bind SSBO for this batch's uniforms (replaces uBaseUniformIndex)
				glBindBufferRange(
					GL_SHADER_STORAGE_BUFFER,
					2,
					uniforms_ssbo->GetBufferId(),
					batch.base_uniform_index * sizeof(CommonUniforms),
					batch.command_count * sizeof(CommonUniforms)
				);

				// Bind bone matrices SSBO
				glBindBufferRange(
					GL_SHADER_STORAGE_BUFFER,
					Constants::SsboBinding::BoneMatrix(),
					bone_matrices_ssbo->GetBufferId(),
					bone_matrices_ssbo->GetFrameOffset(),
					bone_matrices_ssbo->GetElementCount() * sizeof(glm::mat4)
				);
				s->setBool("uUseMDI", true);

				// Bind visibility SSBO for Hi-Z occlusion culling (matching uniform indexing)
				if (dispatch_hiz_occlusion && !is_shadow_pass) {
					glBindBufferRange(
						GL_SHADER_STORAGE_BUFFER,
						Constants::SsboBinding::OcclusionVisibility(),
						occlusion_visibility_ssbo_,
						(batch.base_uniform_index - frame_element_offset) * sizeof(uint32_t),
						batch.command_count * sizeof(uint32_t)
					);
				}

				if (!is_shadow_pass) {
					unsigned int diffuseNr = 1;
					unsigned int specularNr = 1;
					unsigned int normalNr = 1;
					unsigned int heightNr = 1;

					for (size_t i = 0; i < batch.textures.size(); ++i) {
						glActiveTexture(GL_TEXTURE0 + i);
						glBindTexture(GL_TEXTURE_2D, batch.textures[i].id);

						std::string number;
						std::string name = batch.textures[i].type;
						if (name == "texture_diffuse")
							number = std::to_string(diffuseNr++);
						else if (name == "texture_specular")
							number = std::to_string(specularNr++);
						else if (name == "texture_normal")
							number = std::to_string(normalNr++);
						else if (name == "texture_height")
							number = std::to_string(heightNr++);

						s->setInt((name + number).c_str(), i);
					}
					// Note: use_texture is now a bitmask handled in RenderPacket::uniforms (SSBO)
					// This uniform is only used for non-MDI fallback or special passes
					int use_texture_mask = 0;
					for (const auto& tex : batch.textures) {
						if (tex.type == "texture_diffuse")
							use_texture_mask |= 1;
						else if (tex.type == "texture_normal")
							use_texture_mask |= 2;
						else if (tex.type == "texture_metallic")
							use_texture_mask |= 4;
						else if (tex.type == "texture_roughness")
							use_texture_mask |= 8;
						else if (tex.type == "texture_ao")
							use_texture_mask |= 16;
						else if (tex.type == "texture_emissive")
							use_texture_mask |= 32;
					}
					s->setInt("use_texture", use_texture_mask);
				}

				if (batch.vao != current_vao) {
					glBindVertexArray(batch.vao);
					current_vao = batch.vao;
				}

				if (batch.no_cull) {
					glDisable(GL_CULL_FACE);
				} else {
					glEnable(GL_CULL_FACE);
				}

				if (batch.is_indexed) {
					glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_elements_buffer->GetBufferId());
					glMultiDrawElementsIndirect(
						batch.draw_mode,
						batch.index_type,
						(void*)(uintptr_t)(indirect_elements_buffer->GetFrameOffset() +
					                       batch.first_command * sizeof(DrawElementsIndirectCommand)),
						batch.command_count,
						sizeof(DrawElementsIndirectCommand)
					);
				} else {
					glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_arrays_buffer->GetBufferId());
					glMultiDrawArraysIndirect(
						batch.draw_mode,
						(void*)(uintptr_t)(indirect_arrays_buffer->GetFrameOffset() +
					                       batch.first_command * sizeof(DrawArraysIndirectCommand)),
						batch.command_count,
						sizeof(DrawArraysIndirectCommand)
					);
				}
			}

			if (current_vao != 0)
				glBindVertexArray(0);

			glEnable(GL_CULL_FACE); // Restore default

			for (auto* s : used_shaders) {
				s->use();
				s->setBool("uUseMDI", false);
				s->setBool("enableFrustumCulling", false);
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
			glActiveTexture(GL_TEXTURE0);
		}

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

			// Sync with render manager
			if (trail_render_manager) {
				for (auto& [trail_id, trail] : trails) {
					if (!trail_render_manager->HasTrail(trail_id)) {
						trail_render_manager->RegisterTrail(trail_id, trail->GetMaxPoints());
						trail->SetManagedByRenderManager(true);
					}

					trail_render_manager->SetTrailParams(
						trail_id,
						trail->GetIridescent(),
						trail->GetUseRocketTrail(),
						trail->GetUsePBR(),
						trail->GetRoughness(),
						trail->GetMetallic(),
						trail->GetBaseThickness()
					);

					if (trail->IsDirty()) {
						trail_render_manager->UpdateTrailData(
							trail_id,
							trail->GetPoints(),
							trail->GetHead(),
							trail->GetTail(),
							trail->IsFull(),
							trail->GetMinBound(),
							trail->GetMaxBound()
						);
						trail->ClearDirty();
					}
				}
				trail_render_manager->CommitUpdates();
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
			if (is_shadow_pass) {
				return;
			}
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
				float day_time = light_manager.GetDayNightCycle().time;
				terrain_render_manager->PrepareForRender(frustum, camera.pos(), world_scale, lighting_ubo, day_time);

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

			if (atmosphere_manager) {
				atmosphere_manager->BindToShader(*sky_shader);

				const auto& lights = light_manager.GetLights();
				if (!lights.empty()) {
					sky_shader->setVec3("u_sunRadiance", lights[0].color * lights[0].intensity);
					if (lights.size() >= 2) {
						sky_shader->setVec3("u_moonRadiance", lights[1].color * lights[1].intensity);
						sky_shader->setVec3("u_moonDir", glm::normalize(-lights[1].direction));
					} else {
						sky_shader->setVec3("u_moonRadiance", glm::vec3(0.0f));
					}
				}
			}

			glBindVertexArray(sky_vao);
			glDrawArrays(GL_TRIANGLES, 0, 3);
			glBindVertexArray(0);

			// Restore depth state
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
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

			float     world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
			glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(600.0f * world_scale));
			plane_shader->setMat4("model", model);
			plane_shader->setMat4("view", view);
			plane_shader->setMat4("projection", projection);
			glBindVertexArray(plane_vao);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glBindVertexArray(0);
		}

		void GatherShapes() {
			shapes.clear();

			// Update and collect transient effects
			auto it = transient_effects.begin();
			while (it != transient_effects.end()) {
				(*it)->Update(simulation_delta_time);
				if ((*it)->IsExpired()) {
					it = transient_effects.erase(it);
				} else {
					shapes.push_back(*it);
					++it;
				}
			}

			// Shape generation and updates (must happen before any rendering)
			if (!shape_functions.empty()) {
				for (const auto& func : shape_functions) {
					auto new_shapes = func(simulation_time);
					shapes.insert(shapes.end(), new_shapes.begin(), new_shapes.end());
				}
			}

			ShapeCommand command;
			while (shape_command_queue.try_pop(command)) {
				switch (command.type) {
				case ShapeCommandType::Add:
					persistent_shapes[command.shape->GetId()] = command.shape;
					break;
				case ShapeCommandType::Remove:
					persistent_shapes.erase(command.shape_id);
					break;
				case ShapeCommandType::Clear:
					persistent_shapes.clear();
					break;
				}
			}

			for (const auto& pair : persistent_shapes) {
				shapes.push_back(pair.second);
			}

			// Handle terrain clamping for shapes
			for (auto& shape : shapes) {
				if (shape->IsClampedToTerrain()) {
					float     height;
					glm::vec3 normal;
					std::tie(height, normal) = parent->GetTerrainPropertiesAtPoint(shape->GetX(), shape->GetZ());
					shape->SetPosition(shape->GetX(), height + shape->GetGroundOffset(), shape->GetZ());
				}
			}
		}

		void UpdateCamera() {
			if (camera_mode == CameraMode::TRACKING) {
				UpdateSingleTrackCamera(input_state.delta_time, shapes);
			} else if (camera_mode == CameraMode::AUTO) {
				UpdateAutoCamera(input_state.delta_time, shapes);
			} else if (camera_mode == CameraMode::CHASE) {
				UpdateChaseCamera(input_state.delta_time);
			} else if (camera_mode == CameraMode::PATH_FOLLOW) {
				UpdatePathFollowCamera(input_state.delta_time);
			}
		}

		void UpdateAudio() {
			audio_manager->UpdateListener(camera.pos(), camera.front(), camera.up(), camera_velocity_, camera.fov);
			audio_manager->SetGlobalPitch(time_scale);
			audio_manager->Update();
		}

		void UpdateAtmosphere() {
			if (!atmosphere_manager)
				return;

			glm::vec3 sun_dir = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)); // Default
			glm::vec3 sun_color = glm::vec3(1.0f);
			float     sun_intensity = 1.0f;

			if (!light_manager.GetLights().empty()) {
				// Choose the most dominant light (Sun or Moon) for atmospheric scattering
				const auto& lights = light_manager.GetLights();
				const auto& sun = lights[0];
				const auto& moon = (lights.size() >= 2) ? lights[1] : sun;

				const auto& primary_light = (sun.intensity >= moon.intensity) ? sun : moon;

				if (primary_light.type == DIRECTIONAL_LIGHT) {
					sun_dir = glm::normalize(-primary_light.direction);
				} else {
					sun_dir = glm::normalize(primary_light.position - camera.pos());
				}
				sun_color = primary_light.color;
				sun_intensity = primary_light.intensity;
			}

			if (atmosphere_effect) {
				atmosphere_manager->SetRayleighScale(atmosphere_effect->GetRayleighScale());
				atmosphere_manager->SetMieScale(atmosphere_effect->GetMieScale());
				atmosphere_manager->SetMieAnisotropy(atmosphere_effect->GetMieAnisotropy());
				atmosphere_manager->SetMultiScatteringScale(atmosphere_effect->GetMultiScatScale());
				atmosphere_manager->SetAmbientScatteringScale(atmosphere_effect->GetAmbientScatScale());

				atmosphere_manager->SetAtmosphereHeight(atmosphere_effect->GetAtmosphereHeight());
				atmosphere_manager->SetRayleighScattering(atmosphere_effect->GetRayleighScattering());
				atmosphere_manager->SetMieScattering(atmosphere_effect->GetMieScattering());
				atmosphere_manager->SetMieExtinction(atmosphere_effect->GetMieExtinction());
				atmosphere_manager->SetOzoneAbsorption(atmosphere_effect->GetOzoneAbsorption());
				atmosphere_manager->SetRayleighScaleHeight(atmosphere_effect->GetRayleighScaleHeight());
				atmosphere_manager->SetMieScaleHeight(atmosphere_effect->GetMieScaleHeight());
				atmosphere_manager->SetColorVarianceScale(atmosphere_effect->GetColorVarianceScale());
				atmosphere_manager->SetColorVarianceStrength(atmosphere_effect->GetColorVarianceStrength());
			}

			// Update the atmosphere model with the current sun/moon light
			atmosphere_manager->Update(sun_dir, sun_color, sun_intensity, camera.pos(), simulation_time);

			// Sync ambient light from atmosphere to ensure decor and world match
			glm::vec3 estimated_ambient = atmosphere_manager->GetAmbientEstimate();
			light_manager.SetAmbientLight(estimated_ambient);

			if (atmosphere_effect) {
				atmosphere_effect->SetAtmosphereLUTs(
					atmosphere_manager->GetTransmittanceLUT(),
					atmosphere_manager->GetMultiScatteringLUT(),
					atmosphere_manager->GetSkyViewLUT(),
					atmosphere_manager->GetAerialPerspectiveLUT()
				);
				if (noise_manager) {
					atmosphere_effect->SetNoiseTextures(noise_manager->GetTextures());
				}
			}
		}

		void UpdateTrailsLogical() { UpdateTrails(shapes, simulation_time); }

		void PopulateFrameData(FrameData& frame) {
			frame.view = SetupMatrices();
			frame.projection = projection;
			frame.view_projection = projection * frame.view;
			frame.inv_view = glm::inverse(frame.view);

			frame.camera_pos = camera.pos();
			frame.camera_front = camera.front();
			frame.camera_fov = camera.fov;

			frame.simulation_time = simulation_time;
			frame.simulation_delta_time = simulation_delta_time;
			frame.frame_count = frame_count_;

			frame.world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
			frame.far_plane = Constants::Project::Camera::DefaultFarPlane() * std::max(1.0f, frame.world_scale);

			frame.render_width = render_width;
			frame.render_height = render_height;
			frame.window_width = width;
			frame.window_height = height;

			frame.has_shockwaves = shockwave_manager && shockwave_manager->HasActiveShockwaves();
			frame.has_terrain = (terrain_generator != nullptr);

			frame.config = frame_config_;

			frame.camera_frustum = Frustum::FromViewProjection(frame.view, frame.projection);

			// Build predictive generator frustum (wider FOV + angular velocity lead)
			if (terrain_generator) {
				float generator_fov = camera.fov + 15.0f;
				float lead_time = 0.4f;
				float predicted_yaw = camera.yaw + camera_angular_velocity_.x * lead_time;
				float predicted_pitch = camera.pitch + camera_angular_velocity_.y * lead_time;

				glm::vec3 predicted_front;
				predicted_front.x = cos(glm::radians(predicted_pitch)) * sin(glm::radians(predicted_yaw));
				predicted_front.y = sin(glm::radians(predicted_pitch));
				predicted_front.z = -cos(glm::radians(predicted_pitch)) * cos(glm::radians(predicted_yaw));
				predicted_front = glm::normalize(predicted_front);

				glm::mat4 gen_view = glm::lookAt(frame.camera_pos, frame.camera_pos + predicted_front, camera.up());
				glm::mat4 gen_proj =
					glm::perspective(glm::radians(generator_fov), (float)width / (float)height, 0.1f, frame.far_plane);
				frame.generator_frustum = Frustum::FromViewProjection(gen_view, gen_proj);
			}
		}

		void PrepareFrame() {
			// Advance persistent buffers and handle synchronization
			indirect_elements_buffer->AdvanceFrame();
			indirect_arrays_buffer->AdvanceFrame();
			uniforms_ssbo->AdvanceFrame();
			frustum_ssbo->AdvanceFrame();
			bone_matrices_ssbo->AdvanceFrame();
			megabuffer->AdvanceFrame();

			int current_idx = uniforms_ssbo->GetCurrentBufferIndex();
			if (mdi_fences[current_idx]) {
				glClientWaitSync(mdi_fences[current_idx], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
				glDeleteSync(mdi_fences[current_idx]);
				mdi_fences[current_idx] = 0;
			}

			// Reset MDI offsets for the new frame
			mdi_elements_count = 0;
			mdi_arrays_count = 0;
			mdi_uniform_count = 0;
			mdi_frustum_count = 0;
			mdi_bone_count = 0;
		}

		void PrepareUBOs() {
			current_view_matrix = SetupMatrices();
			glm::mat4 current_vp = projection * current_view_matrix;

			// Update Temporal UBO for motion blur and reprojection
			TemporalUbo temporal_data;
			temporal_data.viewProjection = current_vp;
			temporal_data.prevViewProjection = prev_view_projection;
			temporal_data.uProjection = projection;
			temporal_data.invProjection = glm::inverse(projection);
			temporal_data.invView = glm::inverse(current_view_matrix);
			temporal_data.texelSize = glm::vec2(1.0f / render_width, 1.0f / render_height);
			temporal_data.frameIndex = static_cast<int>(frame_count_);
			temporal_data.padding = 0.0f;

			glBindBuffer(GL_UNIFORM_BUFFER, temporal_data_ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(TemporalUbo), &temporal_data);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			prev_view_projection = current_vp;

			if (atmosphere_manager) {
				atmosphere_manager->BindTextures();
			}

			// Resource Preparation (Main Thread)
			{
				PROJECT_PROFILE_SCOPE("PrepareResources");
				for (const auto& shape : shapes) {
					shape->PrepareResources(megabuffer.get());
				}
			}

			// Visual Effects UBO
			if (frame_config_.effects_enabled) {
				VisualEffectsUbo ubo_data{};
				for (const auto& shape : shapes) {
					for (const auto& effect : shape->GetActiveEffects()) {
						if (effect == VisualEffect::RIPPLE) {
							ubo_data.ripple_enabled = 1;
						} else if (effect == VisualEffect::COLOR_SHIFT) {
							ubo_data.color_shift_enabled = 1;
						} else if (effect == VisualEffect::FREEZE_FRAME_TRAIL) {
							clone_manager->CaptureClone(shape, simulation_time);
						}
					}
				}

				ubo_data.black_and_white_enabled = frame_config_.artistic_black_and_white;
				ubo_data.negative_enabled = frame_config_.artistic_negative;
				ubo_data.shimmery_enabled = frame_config_.artistic_shimmery;
				ubo_data.glitched_enabled = frame_config_.artistic_glitched;
				ubo_data.wireframe_enabled = frame_config_.artistic_wireframe;
				ubo_data.erosion_enabled = frame_config_.erosion_enabled;
				ubo_data.color_shift_enabled = ubo_data.color_shift_enabled || frame_config_.artistic_color_shift;
				ubo_data.wind_strength = frame_config_.wind_strength;
				ubo_data.wind_speed = frame_config_.wind_speed;
				ubo_data.wind_frequency = frame_config_.wind_frequency;
				ubo_data.erosion_strength = frame_config_.erosion_strength;
				ubo_data.erosion_scale = frame_config_.erosion_scale;
				ubo_data.erosion_detail = frame_config_.erosion_detail;
				ubo_data.erosion_gully_weight = frame_config_.erosion_gully_weight;
				ubo_data.erosion_max_dist = frame_config_.erosion_max_dist;
				if (frame_config_.artistic_ripple) {
					ubo_data.ripple_enabled = 1;
				}

				glBindBuffer(GL_UNIFORM_BUFFER, visual_effects_ubo);
				glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(VisualEffectsUbo), &ubo_data);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
			}

			// Lighting UBO
			{
				if (CheckpointRingShape::GetShader()) {
					CheckpointRingShape::GetShader()->use();
					CheckpointRingShape::GetShader()->setFloat("time", simulation_time);
				}
				const auto& lights = light_manager.GetLights();
				int         num_lights = std::min(static_cast<int>(lights.size()), 10);

				gpu_lights_cache_.clear();
				for (int i = 0; i < num_lights; ++i) {
					gpu_lights_cache_.push_back(lights[i].ToGPU());
				}

				std::memset(&lighting_ubo_data_, 0, sizeof(LightingUbo));
				std::memcpy(lighting_ubo_data_.lights, gpu_lights_cache_.data(), num_lights * sizeof(LightGPU));
				lighting_ubo_data_.num_lights = num_lights;
				lighting_ubo_data_.world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
				lighting_ubo_data_.day_time = light_manager.GetDayNightCycle().time;
				lighting_ubo_data_.night_factor = light_manager.GetDayNightCycle().night_factor;
				if (post_processing_manager_) {
					post_processing_manager_->SetNightFactor(lighting_ubo_data_.night_factor);
				}
				lighting_ubo_data_.view_pos = camera.pos();
				lighting_ubo_data_.ambient_light = light_manager.GetAmbientLight();
				lighting_ubo_data_.time = simulation_time;
				lighting_ubo_data_.view_dir = camera.front();

				if (atmosphere_effect) {
					auto& cfg = ConfigManager::GetInstance();
					lighting_ubo_data_.cloudShadowIntensity = cfg.GetAppSettingFloat("cloud_shadow_intensity", 0.5f);

					// Sync with atmosphere_effect based on ConfigManager
					atmosphere_effect->SetCloudPhaseG1(cfg.GetAppSettingFloat("cloud_phase_g1", 0.7f));
					atmosphere_effect->SetCloudPhaseG2(cfg.GetAppSettingFloat("cloud_phase_g2", -0.2f));
					atmosphere_effect->SetCloudPhaseAlpha(cfg.GetAppSettingFloat("cloud_phase_alpha", 0.15f));
					atmosphere_effect->SetCloudPhaseIsotropic(cfg.GetAppSettingFloat("cloud_phase_isotropic", 0.05f));
					atmosphere_effect->SetCloudPowderScale(cfg.GetAppSettingFloat("cloud_powder_scale", 0.35f));
					atmosphere_effect->SetCloudPowderMultiplier(
						cfg.GetAppSettingFloat("cloud_powder_multiplier", 0.4f)
					);
					atmosphere_effect->SetCloudPowderLocalScale(
						cfg.GetAppSettingFloat("cloud_powder_local_scale", 2.0f)
					);
					atmosphere_effect->SetCloudShadowOpticalDepthMultiplier(
						cfg.GetAppSettingFloat("cloud_shadow_optical_depth_multiplier", 0.1f)
					);
					atmosphere_effect->SetCloudShadowStepMultiplier(
						cfg.GetAppSettingFloat("cloud_shadow_step_multiplier", 0.1f)
					);
					atmosphere_effect->SetCloudSunLightScale(cfg.GetAppSettingFloat("cloud_sun_light_scale", 10.0f));
					atmosphere_effect->SetCloudMoonLightScale(cfg.GetAppSettingFloat("cloud_moon_light_scale", 2.0f));
					atmosphere_effect->SetCloudBeerPowderMix(cfg.GetAppSettingFloat("cloud_beer_powder_mix", 0.5f));

					lighting_ubo_data_.cloudAltitude = atmosphere_effect->GetCloudAltitude();
					lighting_ubo_data_.cloudThickness = atmosphere_effect->GetCloudThickness();
					lighting_ubo_data_.cloudDensity = atmosphere_effect->GetCloudDensity();
					lighting_ubo_data_.cloudCoverage = atmosphere_effect->GetCloudCoverage();
					lighting_ubo_data_.cloudWarp = atmosphere_effect->GetCloudWarp();
					lighting_ubo_data_.cloudPhaseG1 = atmosphere_effect->GetCloudPhaseG1();
					lighting_ubo_data_.cloudPhaseG2 = atmosphere_effect->GetCloudPhaseG2();
					lighting_ubo_data_.cloudPhaseAlpha = atmosphere_effect->GetCloudPhaseAlpha();
					lighting_ubo_data_.cloudPhaseIsotropic = atmosphere_effect->GetCloudPhaseIsotropic();
					lighting_ubo_data_.cloudPowderScale = atmosphere_effect->GetCloudPowderScale();
					lighting_ubo_data_.cloudPowderMultiplier = atmosphere_effect->GetCloudPowderMultiplier();
					lighting_ubo_data_.cloudPowderLocalScale = atmosphere_effect->GetCloudPowderLocalScale();
					lighting_ubo_data_.cloudShadowOpticalDepthMultiplier = atmosphere_effect
																			   ->GetCloudShadowOpticalDepthMultiplier();
					lighting_ubo_data_.cloudShadowStepMultiplier = atmosphere_effect->GetCloudShadowStepMultiplier();
					lighting_ubo_data_.cloudSunLightScale = atmosphere_effect->GetCloudSunLightScale();
					lighting_ubo_data_.cloudMoonLightScale = atmosphere_effect->GetCloudMoonLightScale();
					lighting_ubo_data_.cloudBeerPowderMix = atmosphere_effect->GetCloudBeerPowderMix();

					// Calculate cloud shadow matrix (world XZ to shadow map UV)
					float     mapSize = atmosphere_manager->GetCloudShadowWorldSize();
					glm::vec3 camPos = camera.pos();
					glm::mat4 shadowMat(1.0f);
					// 1. Move to camera-relative XZ
					shadowMat = glm::translate(shadowMat, glm::vec3(0.5f, 0.5f, 0.0f));
					// 2. Scale to [0, 1] UV space
					shadowMat = glm::scale(shadowMat, glm::vec3(1.0f / mapSize, 1.0f / mapSize, 1.0f));
					// 3. Center on camera
					shadowMat = glm::translate(shadowMat, glm::vec3(-camPos.x, -camPos.z, 0.0f));

					lighting_ubo_data_.cloudShadowMatrix = shadowMat;

				} else {
					lighting_ubo_data_.cloudShadowIntensity = 0.0f;
				}

				glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
				glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightingUbo), &lighting_ubo_data_);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);

				// GPU-side copy of SH coefficients from SSBO into the UBO (no CPU readback)
				if (atmosphere_manager) {
					static_assert(offsetof(LightingUbo, sh_coeffs) == 832, "SH offset mismatch");
					atmosphere_manager->CopySHToUBO(lighting_ubo, 832);
				}
			}

			// Frustum UBO for generic passes
			{
				glm::mat4 view_mat = glm::lookAt(camera.pos(), camera.pos() + camera.front(), camera.up());
				UpdateFrustumUbo(view_mat, projection, camera.pos());
			}
		}

		void GenerateRenderPacketsAsync() {
			render_queue.Clear();

			RenderContext context;
			context.view = current_view_matrix;
			context.projection = projection;
			context.view_pos = camera.pos();
			context.time = simulation_time;

			float world_scale = terrain_generator ? terrain_generator->GetWorldScale() : 1.0f;
			context.far_plane = Constants::Project::Camera::DefaultFarPlane() * std::max(1.0f, world_scale);
			context.frustum = Frustum::FromViewProjection(current_view_matrix, projection);
			context.shader_table = &shader_table;
			context.megabuffer = megabuffer.get();

			const size_t num_shapes = shapes.size();
			const size_t chunk_size = 64;
			pending_packet_futures.clear();

			for (size_t i = 0; i < num_shapes; i += chunk_size) {
				size_t end = std::min(i + chunk_size, num_shapes);
				pending_packet_futures.push_back(thread_pool.submit([this, i, end, context]() {
					PROJECT_PROFILE_SCOPE("PacketGenerationWorker");
					std::vector<RenderPacket> local_packets;
					local_packets.reserve(end - i);
					for (size_t j = i; j < end; ++j) {
						auto& shape = shapes[j];
						if (auto* cached = shape->GetCachedPackets(); cached && !cached->empty()) {
							for (auto packet : *cached) {
								glm::vec3   world_pos = glm::vec3(packet.uniforms.model[3]);
								float       normalized_depth = context.CalculateNormalizedDepth(world_pos);
								RenderLayer layer = (packet.uniforms.color.a < 0.99f) ? RenderLayer::Transparent
																					  : RenderLayer::Opaque;
								packet.sort_key = CalculateSortKey(
									layer,
									packet.shader_handle,
									packet.vao,
									packet.draw_mode,
									packet.index_count > 0,
									packet.material_handle,
									normalized_depth
								);
								local_packets.push_back(std::move(packet));
							}
						} else {
							std::vector<RenderPacket> new_packets;
							shape->GenerateRenderPackets(new_packets, context);
							for (const auto& packet : new_packets) {
								local_packets.push_back(packet);
							}
							shape->CachePackets(std::move(new_packets));
							shape->MarkClean();
						}
					}
					if (!local_packets.empty()) {
						render_queue.Submit(std::move(local_packets));
					}
				}));
			}
		}

		bool packets_synced_ = false;

		// Lazy sync — blocks on first call, no-op on subsequent calls.
		// Shadow decor rendering can proceed without packets; the shape
		// callback triggers this when packets are first needed.
		void EnsurePacketsSynced() {
			if (packets_synced_)
				return;
			{
				PROJECT_PROFILE_SCOPE("WaitForPackets");
				for (auto& f : pending_packet_futures) {
					f.get();
				}
				pending_packet_futures.clear();
			}
			{
				PROJECT_PROFILE_SCOPE("SortRenderQueue");
				render_queue.Sort(thread_pool);
			}
			packets_synced_ = true;
		}

		void UpdateSystems() {
			// Calculate frustum for terrain generation and decor placement
			if (terrain_generator) {
				float world_scale_val = terrain_generator->GetWorldScale();
				float far_plane_val = Constants::Project::Camera::DefaultFarPlane() * std::max(1.0f, world_scale_val);
				float generator_fov = camera.fov + 15.0f;
				glm::mat4 generator_proj =
					glm::perspective(glm::radians(generator_fov), (float)width / (float)height, 0.1f, far_plane_val);

				float lead_time = 0.4f;
				float predicted_yaw = camera.yaw + camera_angular_velocity_.x * lead_time;
				float predicted_pitch = camera.pitch + camera_angular_velocity_.y * lead_time;

				Camera predicted_cam = camera;
				predicted_cam.yaw = predicted_yaw;
				predicted_cam.pitch = predicted_pitch;

				glm::vec3 cameraPos(predicted_cam.x, predicted_cam.y, predicted_cam.z);
				glm::mat4 predicted_view = glm::lookAt(
					cameraPos,
					cameraPos + predicted_cam.front(),
					predicted_cam.up()
				);

				generator_frustum = CalculateFrustum(predicted_view, generator_proj);
				terrain_generator->Update(generator_frustum, camera);
			}

			clone_manager->Update(simulation_time, camera.pos());
			fire_effect_manager->Update(
				simulation_delta_time,
				simulation_time,
				frame_config_.ambient_particle_density,
				terrain_render_manager ? terrain_render_manager->GetChunkInfo(terrain_generator->GetWorldScale())
									   : std::vector<glm::vec4>{},
				terrain_render_manager ? terrain_render_manager->GetHeightmapTexture() : 0,
				noise_manager ? noise_manager->GetCurlTexture() : 0,
				terrain_render_manager ? terrain_render_manager->GetBiomeTexture() : 0,
				lighting_ubo,
				frustum_ssbo->GetBufferId(),
				frustum_ssbo->GetFrameOffset() + mdi_frustum_count * sizeof(FrustumDataGPU),
				noise_manager ? noise_manager->GetExtraNoiseTexture() : 0
			);
			mesh_explosion_manager->Update(simulation_delta_time, simulation_time);
			sound_effect_manager->Update(simulation_delta_time);
			shockwave_manager->Update(simulation_delta_time);
			if (akira_effect_manager && terrain_generator) {
				akira_effect_manager->Update(simulation_delta_time, *terrain_generator);
			}
			sdf_volume_manager->UpdateUBO();
			sdf_volume_manager->BindUBO(Constants::UboBinding::SdfVolumes());
			shockwave_manager->UpdateShaderData();
			shockwave_manager->BindUBO(Constants::UboBinding::Shockwaves());

			if (decor_manager && terrain_generator && terrain_render_manager) {
				decor_manager->SetMinPixelSize(
					ConfigManager::GetInstance().GetAppSettingFloat("foliage_culling_pixel_threshold", 8.0f)
				);
				if (atmosphere_manager) {
					decor_manager->SetAtmosphereManager(atmosphere_manager.get());
				}
				decor_manager->Update(
					simulation_delta_time,
					camera,
					generator_frustum,
					*terrain_generator,
					terrain_render_manager
				);
			}
		}

		void RenderShadowPasses(const FrameData& frame) {
			if (!shadow_pass_ || !shadow_pass_->HasUpdates())
				return;

			// The shadow pass needs a callback to render shapes because
			// ExecuteRenderQueue lives here with the MDI infrastructure.
			auto render_shapes = [this, &frame](const glm::mat4& light_space_matrix) {
				EnsurePacketsSynced(); // lazy sync — shadow decor runs without packets
				ExecuteRenderQueue(
					render_queue,
					frame.view,
					frame.projection,
					frame.camera_pos,
					RenderLayer::Opaque,
					shadow_shader_handle,
					light_space_matrix,
					std::nullopt,
					true
				);
			};

			shadow_pass_->Execute(frame, render_shapes);

			scene_center = shadow_pass_->GetSceneCenter();
		}

		RenderCallbacks MakeRenderCallbacks(const FrameData& frame) {
			return {
				.execute_queue =
					[this, &frame](RenderLayer layer, bool dispatch_hiz) {
						ExecuteRenderQueue(
							render_queue,
							frame.view,
							frame.projection,
							frame.camera_pos,
							layer,
							std::nullopt,
							std::nullopt,
							std::nullopt,
							false,
							dispatch_hiz
						);
					},
				.bind_shadows = [this](Shader& s) { BindShadows(s); },
				.update_frustum_ubo = [this,
			                           &frame]() { UpdateFrustumUbo(frame.view, frame.projection, frame.camera_pos); },
				.render_terrain =
					[this, &frame]() {
						PROJECT_PROFILE_SCOPE("RenderTerrain");
						RenderTerrain(frame.view, frame.projection, std::nullopt);
					},
				.render_plane = [this, &frame]() { RenderPlane(frame.view); },
				.render_sky =
					[this, &frame]() {
						PROJECT_PROFILE_SCOPE("RenderSky");
						RenderSky(frame.view);
					},
			};
		}

		void RenderOpaqueScene(const FrameData& frame) {
			if (opaque_pass_ && compositor_) {
				opaque_pass_->Execute(frame, *compositor_, render_scale, MakeRenderCallbacks(frame));

				// Generate Hi-Z pyramid from current depth buffer after opaque pass
				// so it's available for screen-space effects like SSGI or culling in the same frame
				if (hiz_manager && hiz_manager->IsInitialized() && frame_count_ > 0) {
					hiz_manager->GeneratePyramid(compositor_->GetDepthTexture());

					// Update Unified Screen-Space effect with current Hi-Z
					if (post_processing_manager_) {
						for (auto& effect : post_processing_manager_->GetPreToneMappingEffects()) {
							if (auto unified = std::dynamic_pointer_cast<PostProcessing::UnifiedScreenSpaceEffect>(effect)) {
								unified->SetHiZTexture(hiz_manager->GetHiZTexture(), hiz_manager->GetMipCount());
							}
						}
					}
				}
			}
		}

		void RenderTransparentScene(const FrameData& frame) {
			if (early_effects_pass_ && compositor_) {
				early_effects_pass_->Execute(frame, *compositor_);
			}

			if (particle_pass_) {
				particle_pass_->Execute(frame);
			}

			// Capture background for refraction
			if (compositor_) {
				bool   effects_enabled = frame.config.effects_enabled;
				GLuint post_fx_fbo = (effects_enabled && post_processing_manager_)
					? post_processing_manager_->GetCurrentFBO()
					: 0;
				compositor_->CaptureRefraction(frame, effects_enabled, post_fx_fbo);
			}

			if (transparent_pass_) {
				transparent_pass_->Execute(frame, MakeRenderCallbacks(frame));
			}
		}

		void RenderPostProcessing(const FrameData& frame) {
			PROJECT_PROFILE_SCOPE("PostProcessing");
			compositor_->ResolveToScreen(
				frame,
				render_scale,
				frame.config.effects_enabled ? post_processing_manager_.get() : nullptr,
				(frame.has_shockwaves && shockwave_manager) ? shockwave_manager.get() : nullptr
			);
		}

		void FinalizeFrame() {
			// Update shape positions for motion vectors / trail tracking
			for (const auto& shape : shapes) {
				shape->UpdateLastPosition();
			}

			ui_manager->Render();
			int current_idx = uniforms_ssbo->GetCurrentBufferIndex();
			if (mdi_fences[current_idx])
				glDeleteSync(mdi_fences[current_idx]);
			mdi_fences[current_idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			glfwSwapBuffers(window);
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

			// Resize compositor FBOs and refresh aliases
			if (compositor_) {
				compositor_->Resize(render_width, render_height, enable_hdr_);
			}

			// --- Resize Hi-Z pyramid ---
			if (hiz_manager && hiz_manager->IsInitialized()) {
				hiz_manager->Resize(render_width, render_height);
			}

			// --- Resize post-processing manager ---
			if (post_processing_manager_) {
				post_processing_manager_->Resize(render_width, render_height);
				post_processing_manager_->SetSharedDepthTexture(compositor_->GetDepthTexture());
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

	void Visualizer::ClearShapes() {
		impl->shape_command_queue.push({ShapeCommandType::Clear, nullptr, 0});
	}

	void Visualizer::ClearShapeHandlers() {
		impl->shape_functions.clear();
	}

	bool Visualizer::ShouldClose() const {
		return glfwWindowShouldClose(impl->window);
	}

	void Visualizer::Update() {
		PROJECT_PROFILE_SCOPE("Update");
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
		impl->simulation_delta_time = impl->paused ? 0.0f : impl->input_state.delta_time;

		Profiler::GetInstance().Update(delta_time);

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

		impl->light_manager.Update(impl->simulation_delta_time);

		// Update ambient weather
		if (impl->weather_manager && impl->weather_manager->IsEnabled()) {
			impl->weather_manager->Update(
				impl->simulation_delta_time,
				impl->simulation_time,
				impl->camera.pos(),
				impl->light_manager.GetDayNightCycle().time
			);

			const auto& w = impl->weather_manager->GetCurrentWeather();

			// Apply to primary lights (Sun and Moon)
			// We multiply instead of assigning to preserve the Day/Night cycle's intensity curves.
			if (!impl->light_manager.GetLights().empty()) {
				impl->light_manager.GetLights()[0].intensity *= w.sun_intensity;
			}
			if (impl->light_manager.GetLights().size() > 1 &&
			    impl->light_manager.GetLights()[1].type == DIRECTIONAL_LIGHT) {
				impl->light_manager.GetLights()[1].intensity *= w.sun_intensity;
			}

			// Apply to wind settings in Config (for shaders)
			auto& config = ConfigManager::GetInstance();
			config.SetFloat("wind_strength", w.wind_strength);
			config.SetFloat("wind_speed", w.wind_speed);
			config.SetFloat("wind_frequency", w.wind_frequency);

			// Apply to atmosphere effect
			if (impl->atmosphere_effect) {
				impl->atmosphere_effect->SetHazeDensity(w.haze_density);
				impl->atmosphere_effect->SetHazeHeight(w.haze_height);
				impl->atmosphere_effect->SetCloudDensity(w.cloud_density);
				impl->atmosphere_effect->SetCloudAltitude(w.cloud_altitude);
				impl->atmosphere_effect->SetCloudThickness(w.cloud_thickness);
				impl->atmosphere_effect->SetCloudCoverage(w.cloud_coverage);
				impl->atmosphere_effect->SetRayleighScale(w.rayleigh_scale);
				impl->atmosphere_effect->SetMieScale(w.mie_scale);

				// Atmosphere-specific attributes from weather
				impl->atmosphere_effect->SetAtmosphereHeight(w.atmosphere_height);
				impl->atmosphere_effect->SetRayleighScattering(w.rayleigh_scattering);
				impl->atmosphere_effect->SetMieScattering(w.mie_scattering);
				impl->atmosphere_effect->SetMieExtinction(w.mie_extinction);
				impl->atmosphere_effect->SetOzoneAbsorption(w.ozone_absorption);
				impl->atmosphere_effect->SetRayleighScaleHeight(w.rayleigh_scale_height);
				impl->atmosphere_effect->SetMieScaleHeight(w.mie_scale_height);
			}
		}

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
			impl->shake_timer -= impl->simulation_delta_time;
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

		// Logical updates migrated from Render
		impl->GatherShapes();
		impl->UpdateCamera();
		impl->UpdateAudio();
		impl->UpdateAtmosphere();
		impl->UpdateTrailsLogical();

		// Shadow scheduling needs FrameData, so we do a lightweight populate here.
		// The full PopulateFrameData in Render() will overwrite with final values.
		if (impl->shadow_pass_) {
			FrameData shadow_frame;
			shadow_frame.camera_pos = impl->camera.pos();
			shadow_frame.camera_front = impl->camera.front();
			shadow_frame.has_terrain = (impl->terrain_generator != nullptr);
			shadow_frame.config = impl->frame_config_;
			shadow_frame.frame_count = impl->frame_count_;
			impl->shadow_pass_->ScheduleUpdates(shadow_frame, impl->shapes);
		}
	}

	void Visualizer::Render() {
		PROJECT_PROFILE_SCOPE("Render");
		impl->RefreshFrameConfig();
		impl->PrepareFrame();

		// Build this frame's data from the temporal chain
		FrameData frame = impl->current_frame_.NextFrame();
		impl->PopulateFrameData(frame);

		impl->PrepareUBOs();
		impl->packets_synced_ = false;
		impl->GenerateRenderPacketsAsync();
		impl->UpdateSystems();

		// Shadow decor renders during the overlap window (no packets needed).
		// The shape callback triggers lazy sync when packets are first needed.
		impl->RenderShadowPasses(frame);
		impl->EnsurePacketsSynced(); // fallback if no shadow shapes triggered it

		impl->RenderOpaqueScene(frame);

		// Update terrain probes based on the opaque scene result
		if (impl->terrain_render_manager && impl->compositor_ && impl->atmosphere_manager) {
			impl->terrain_render_manager->DispatchProbeUpdate(
				impl->compositor_->GetColorTexture(),
				impl->compositor_->GetDepthTexture(),
				impl->compositor_->GetNormalTexture(),
				impl->compositor_->GetAlbedoTexture(),
				impl->compositor_->GetVelocityTexture(),
				impl->atmosphere_manager->GetSkyViewLUT(),
				frame.view,
				frame.projection,
				impl->lighting_ubo
			);
		}

		impl->RenderTransparentScene(frame);
		impl->RenderPostProcessing(frame);
		impl->FinalizeFrame();

		// Commit for next frame's temporal chain
		impl->current_frame_ = frame;
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
			float     far_plane = Constants::Project::Camera::DefaultFarPlane() * std::max(1.0f, world_scale);
			glm::mat4 proj = glm::perspective(
				glm::radians(impl->camera.fov),
				(float)impl->width / (float)impl->height,
				0.1f,
				far_plane
			);

			if (impl->decor_manager) {
				impl->decor_manager->PopulateDefaultDecor();
				impl->decor_manager->PrepareResources(impl->megabuffer.get());

				impl->decor_manager->Cull(view, impl->projection, impl->render_width, impl->render_height);
				impl->decor_manager->Render(view, impl->projection);
			}

			// Create render passes now that all dependencies are initialized
			if (impl->shadow_manager && impl->decor_manager && impl->noise_manager) {
				impl->shadow_pass_ = std::make_unique<ShadowRenderPass>(
					*impl->shadow_manager,
					impl->light_manager,
					*impl->decor_manager,
					*impl->noise_manager,
					impl->terrain_render_manager,
					impl->shadow_shader_handle
				);
			}

			if (impl->decor_manager && impl->hiz_manager && impl->shadow_manager && impl->shader) {
				impl->opaque_pass_ = std::make_unique<OpaqueScenePass>(
					*impl->decor_manager,
					*impl->hiz_manager,
					*impl->shadow_manager,
					*impl->shader,
					impl->terrain_render_manager
				);
			}

			if (impl->post_processing_manager_ && impl->shadow_manager && impl->shader) {
				impl->early_effects_pass_ = std::make_unique<EarlyEffectsPass>(
					*impl->post_processing_manager_,
					*impl->shadow_manager,
					*impl->shader
				);
			}

			if (impl->fire_effect_manager && impl->mesh_explosion_manager && impl->noise_manager) {
				impl->particle_pass_ = std::make_unique<ParticleEffectsPass>(
					*impl->fire_effect_manager,
					*impl->mesh_explosion_manager,
					impl->akira_effect_manager.get(),
					*impl->noise_manager
				);
			}

			impl->transparent_pass_ = std::make_unique<TransparentPass>();

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

	CameraState Visualizer::CaptureCameraState() const {
		CameraState state;
		state.camera = impl->camera;
		state.mode = impl->camera_mode;

		state.auto_camera_time = impl->auto_camera_time;
		state.auto_camera_angle = impl->auto_camera_angle;
		state.auto_camera_height_offset = impl->auto_camera_height_offset;
		state.auto_camera_distance = impl->auto_camera_distance;

		state.tracked_dot_index = impl->tracked_dot_index;
		state.single_track_orbit_yaw = impl->single_track_orbit_yaw;
		state.single_track_orbit_pitch = impl->single_track_orbit_pitch;
		state.single_track_distance = impl->single_track_distance;

		state.chase_target = impl->chase_target_;
		state.current_chase_target_index = impl->current_chase_target_index_;

		state.path_target = impl->path_target_;
		state.path_segment_index = impl->path_segment_index_;
		state.path_t = impl->path_t_;
		state.path_direction = impl->path_direction_;
		state.path_orientation = impl->path_orientation_;
		state.path_auto_bank_angle = impl->path_auto_bank_angle_;

		return state;
	}

	void Visualizer::PushCameraState() {
		impl->camera_state_stack_.push_back(CaptureCameraState());
	}

	void Visualizer::PopCameraState() {
		if (impl->camera_state_stack_.empty())
			return;

		CameraState state = std::move(impl->camera_state_stack_.back());
		impl->camera_state_stack_.pop_back();

		// Apply mode first
		SetCameraMode(state.mode);

		// Special logic for follow modes: don't restore position if it would cause a jump
		bool is_follow_mode =
			(state.mode == CameraMode::AUTO || state.mode == CameraMode::TRACKING || state.mode == CameraMode::CHASE ||
		     state.mode == CameraMode::PATH_FOLLOW);

		if (!is_follow_mode) {
			impl->camera = state.camera;
		} else {
			// Restore non-spatial camera properties even in follow mode
			impl->camera.fov = state.camera.fov;
			impl->camera.speed = state.camera.speed;
			impl->camera.follow_distance = state.camera.follow_distance;
			impl->camera.follow_elevation = state.camera.follow_elevation;
			impl->camera.follow_look_ahead = state.camera.follow_look_ahead;
			impl->camera.follow_responsiveness = state.camera.follow_responsiveness;
			impl->camera.path_smoothing = state.camera.path_smoothing;
			impl->camera.path_bank_factor = state.camera.path_bank_factor;
			impl->camera.path_bank_speed = state.camera.path_bank_speed;
		}

		// Restore internal mode states
		impl->auto_camera_time = state.auto_camera_time;
		impl->auto_camera_angle = state.auto_camera_angle;
		impl->auto_camera_height_offset = state.auto_camera_height_offset;
		impl->auto_camera_distance = state.auto_camera_distance;

		impl->tracked_dot_index = state.tracked_dot_index;
		impl->single_track_orbit_yaw = state.single_track_orbit_yaw;
		impl->single_track_orbit_pitch = state.single_track_orbit_pitch;
		impl->single_track_distance = state.single_track_distance;

		impl->chase_target_ = state.chase_target;
		impl->current_chase_target_index_ = state.current_chase_target_index;

		impl->path_target_ = state.path_target;
		impl->path_segment_index_ = state.path_segment_index;
		impl->path_t_ = state.path_t;
		impl->path_direction_ = state.path_direction;
		impl->path_orientation_ = state.path_orientation;
		impl->path_auto_bank_angle_ = state.path_auto_bank_angle;
	}

	void Visualizer::LookAt(const glm::vec3& target) {
		glm::vec3 pos = impl->camera.pos();
		if (glm::distance(pos, target) < 0.001f)
			return;
		glm::vec3 front = glm::normalize(target - pos);
		impl->camera.yaw = glm::degrees(atan2(front.x, -front.z));
		impl->camera.pitch = glm::degrees(asin(front.y));
		impl->camera.pitch = std::clamp(impl->camera.pitch, -89.0f, 89.0f);
		impl->camera.roll = 0.0f;
	}

	void Visualizer::LookAt(const glm::vec3& target, const glm::vec3& local_offset) {
		glm::vec3 eye = target + local_offset;
		impl->camera.x = eye.x;
		impl->camera.y = eye.y;
		impl->camera.z = eye.z;
		LookAt(target);
	}

	void Visualizer::LookAt(const glm::vec3& target, std::shared_ptr<Shape> eye_object) {
		if (!eye_object)
			return;
		impl->camera.x = eye_object->GetX();
		impl->camera.y = eye_object->GetY();
		impl->camera.z = eye_object->GetZ();
		LookAt(target);
	}

	void Visualizer::LookAt(const glm::vec3& target, std::shared_ptr<EntityBase> eye_entity) {
		if (!eye_entity)
			return;
		Vector3 pos = eye_entity->GetPosition();
		impl->camera.x = pos.x;
		impl->camera.y = pos.y;
		impl->camera.z = pos.z;
		LookAt(target);
	}

	void Visualizer::LookAt(std::shared_ptr<Shape> target) {
		if (!target)
			return;
		LookAt(glm::vec3(target->GetX(), target->GetY(), target->GetZ()));
	}

	void Visualizer::LookAt(std::shared_ptr<Shape> target, const glm::vec3& local_offset) {
		if (!target)
			return;
		glm::mat4 model = target->GetModelMatrix();
		glm::vec3 eye = glm::vec3(model * glm::vec4(local_offset, 1.0f));
		impl->camera.x = eye.x;
		impl->camera.y = eye.y;
		impl->camera.z = eye.z;
		LookAt(glm::vec3(target->GetX(), target->GetY(), target->GetZ()));
	}

	void Visualizer::LookAt(std::shared_ptr<Shape> target, std::shared_ptr<Shape> eye_object) {
		if (!target || !eye_object)
			return;
		impl->camera.x = eye_object->GetX();
		impl->camera.y = eye_object->GetY();
		impl->camera.z = eye_object->GetZ();
		LookAt(target);
	}

	void Visualizer::LookAt(std::shared_ptr<Shape> target, std::shared_ptr<EntityBase> eye_entity) {
		if (!target || !eye_entity)
			return;
		Vector3 pos = eye_entity->GetPosition();
		impl->camera.x = pos.x;
		impl->camera.y = pos.y;
		impl->camera.z = pos.z;
		LookAt(target);
	}

	void Visualizer::LookAt(std::shared_ptr<EntityBase> target) {
		if (!target)
			return;
		Vector3 pos = target->GetPosition();
		LookAt(glm::vec3(pos.x, pos.y, pos.z));
	}

	void Visualizer::LookAt(std::shared_ptr<EntityBase> target, const glm::vec3& local_offset) {
		if (!target)
			return;
		glm::vec3 pos = glm::vec3(target->GetPosition().x, target->GetPosition().y, target->GetPosition().z);
		glm::quat ori = target->GetOrientation();
		glm::vec3 eye = pos + (ori * local_offset);
		impl->camera.x = eye.x;
		impl->camera.y = eye.y;
		impl->camera.z = eye.z;
		LookAt(pos);
	}

	void Visualizer::LookAt(std::shared_ptr<EntityBase> target, std::shared_ptr<Shape> eye_object) {
		if (!target || !eye_object)
			return;
		impl->camera.x = eye_object->GetX();
		impl->camera.y = eye_object->GetY();
		impl->camera.z = eye_object->GetZ();
		LookAt(target);
	}

	void Visualizer::LookAt(std::shared_ptr<EntityBase> target, std::shared_ptr<EntityBase> eye_entity) {
		if (!target || !eye_entity)
			return;
		Vector3 pos = eye_entity->GetPosition();
		impl->camera.x = pos.x;
		impl->camera.y = pos.y;
		impl->camera.z = pos.z;
		LookAt(target);
	}

	void Visualizer::AddInputCallback(InputCallback callback) {
		impl->input_callbacks.push_back(callback);
	}

	Ray Visualizer::GetRayFromScreen(double screen_x, double screen_y) const {
		glm::vec3 screen_pos(screen_x, impl->height - screen_y, 0.0f);
		glm::vec4 viewport(0.0f, 0.0f, impl->width, impl->height);

		glm::mat4   view = impl->SetupMatrices(impl->camera);
		const auto& cam = impl->camera;
		glm::vec3   ray_origin = cam.pos();

		screen_pos.z = 1.0f;
		glm::vec3 far_plane_pos = glm::unProject(screen_pos, view, impl->projection, viewport);

		glm::vec3 ray_dir = glm::normalize(far_plane_pos - ray_origin);

		return Ray(ray_origin, ray_dir);
	}

	std::optional<glm::vec3> Visualizer::ScreenToWorld(double screen_x, double screen_y) const {
		glm::vec3 screen_pos(screen_x, impl->height - screen_y, 0.0f);

		glm::vec4 viewport(0.0f, 0.0f, impl->width, impl->height);

		glm::mat4 view = impl->SetupMatrices(impl->camera);

		Ray       ray = GetRayFromScreen(screen_x, screen_y);
		glm::vec3 ray_origin = ray.origin;
		glm::vec3 ray_dir = ray.direction;

		float world_scale = impl->terrain_generator ? impl->terrain_generator->GetWorldScale() : 1.0f;
		float max_ray_dist = Constants::Project::Camera::DefaultFarPlane() * std::max(1.0f, world_scale);

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

		if (impl->weather_manager) {
			impl->weather_manager->SetTerrainGenerator(impl->terrain_generator.get());
		}

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
		float            lifetime,
		EmitterType      type,
		const glm::vec3& dimensions,
		float            sweep
	) {
		return impl->fire_effect_manager
			->AddEffect(position, style, direction, velocity, max_particles, lifetime, type, dimensions, sweep);
	}

	FireEffectManager* Visualizer::GetFireEffectManager() {
		return impl->fire_effect_manager.get();
	}

	DecorManager* Visualizer::GetDecorManager() {
		return impl->decor_manager.get();
	}

	void Visualizer::SetDecorManager(std::shared_ptr<DecorManager> decor_manager) {
		impl->decor_manager = decor_manager;
	}

	WeatherManager* Visualizer::GetWeatherManager() {
		return impl->weather_manager.get();
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

	void Visualizer::SetFireEffectSourceModel(
		const std::shared_ptr<FireEffect>& effect,
		const std::shared_ptr<Model>&      model
	) const {
		if (effect) {
			effect->SetSourceModel(model);
		}
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

		// 5. Flash light
		Light flash = Light::CreateFlash(
			position,
			25.0f * intensity,
			glm::vec3(1.0f, 0.7f, 0.3f), // Warm orange flash
			25.0f * intensity
		);
		flash.auto_remove = true;
		flash.behavior.loop = false;
		flash.SetEaseOut(0.5f * intensity);
		impl->light_manager.AddLight(flash);
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

		// Add a flash light
		Light flash = Light::CreateFlash(
			position,
			45.0f * intensity,
			glm::vec3(1.0f, 0.8f, 0.5f), // Bright yellow-orange flash
			45.0f * intensity
		);
		flash.auto_remove = true;
		flash.behavior.loop = false;
		flash.SetEaseOut(0.4f * intensity);
		impl->light_manager.AddLight(flash);
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

	UI::UIManager& Visualizer::GetUIManager() {
		return *impl->ui_manager;
	}
} // namespace Boidsish
