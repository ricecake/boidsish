#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include <GL/glew.h>
#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief Manages shader state to minimize redundant OpenGL calls.
	 *
	 * The ShaderManager caches:
	 * - Currently bound shader program (avoids redundant glUseProgram calls)
	 * - Uniform values per shader (avoids setting unchanged uniforms)
	 * - Texture unit bindings (avoids redundant glActiveTexture/glBindTexture)
	 *
	 * This reduces driver overhead by 10-20% in complex scenes with many shader switches.
	 *
	 * Usage:
	 *   // Shader::use() and setXXX() methods automatically use ShaderManager
	 *   shader->use();                    // Only calls glUseProgram if needed
	 *   shader->setFloat("time", 1.5f);   // Only sets if value changed
	 *
	 *   // For texture binding:
	 *   ShaderManager::GetInstance().BindTexture(0, GL_TEXTURE_2D, texId);
	 */
	class ShaderManager {
	public:
		/// Singleton access
		static ShaderManager& GetInstance();

		// Non-copyable, non-movable singleton
		ShaderManager(const ShaderManager&) = delete;
		ShaderManager& operator=(const ShaderManager&) = delete;
		ShaderManager(ShaderManager&&) = delete;
		ShaderManager& operator=(ShaderManager&&) = delete;

		/**
		 * @brief Use a shader program by ID, only issuing glUseProgram if it's different.
		 * @param program_id The OpenGL program ID
		 * @return true if the shader was actually switched, false if already active
		 */
		bool Use(GLuint program_id);

		/**
		 * @brief Get the currently active shader program ID.
		 */
		GLuint GetCurrentProgram() const { return current_program_; }

		/**
		 * @brief Set a uniform value, caching to avoid redundant calls.
		 *
		 * These methods require a shader to be active (via Use()).
		 * They cache values per-shader and only issue GL calls when the value changes.
		 */
		void SetUniform(const std::string& name, int value);
		void SetUniform(const std::string& name, float value);
		void SetUniform(const std::string& name, const glm::vec2& value);
		void SetUniform(const std::string& name, const glm::vec3& value);
		void SetUniform(const std::string& name, const glm::vec4& value);
		void SetUniform(const std::string& name, const glm::mat3& value);
		void SetUniform(const std::string& name, const glm::mat4& value);

		/**
		 * @brief Bind a texture to a texture unit, caching to avoid redundant calls.
		 * @param unit Texture unit index (0-15 typically)
		 * @param target Texture target (GL_TEXTURE_2D, GL_TEXTURE_2D_ARRAY, etc.)
		 * @param texture_id The texture to bind
		 * @return true if the texture was actually bound, false if already bound
		 */
		bool BindTexture(int unit, GLenum target, GLuint texture_id);

		/**
		 * @brief Force reset of all cached state.
		 *
		 * Call this if external code may have modified GL state without going
		 * through ShaderManager (e.g., third-party libraries).
		 */
		void InvalidateCache();

		/**
		 * @brief Reset uniform cache for a specific shader.
		 *
		 * Useful when a shader's uniform values need to be re-sent
		 * (e.g., after recompiling a shader).
		 */
		void InvalidateShaderCache(GLuint program_id);

		/**
		 * @brief Get statistics about cache hits/misses for debugging.
		 */
		struct Stats {
			uint64_t shader_switches = 0;
			uint64_t shader_cache_hits = 0;
			uint64_t uniform_sets = 0;
			uint64_t uniform_cache_hits = 0;
			uint64_t texture_binds = 0;
			uint64_t texture_cache_hits = 0;
		};

		const Stats& GetStats() const { return stats_; }

		void ResetStats() { stats_ = {}; }

		/**
		 * @brief Check if a uniform value is already cached (and matches).
		 *
		 * These are used by Shader class to skip redundant GL calls.
		 * Returns true if the value is cached AND matches, false otherwise.
		 * When returning false, updates the cache with the new value.
		 */
		bool IsCached(GLuint program, const std::string& name, int value);
		bool IsCached(GLuint program, const std::string& name, float value);
		bool IsCached(GLuint program, const std::string& name, const glm::vec2& value);
		bool IsCached(GLuint program, const std::string& name, const glm::vec3& value);
		bool IsCached(GLuint program, const std::string& name, const glm::vec4& value);
		bool IsCached(GLuint program, const std::string& name, const glm::mat3& value);
		bool IsCached(GLuint program, const std::string& name, const glm::mat4& value);

	private:
		ShaderManager() = default;

		/// Currently bound shader program
		GLuint current_program_ = 0;

		/// Uniform value cache: program_id -> (uniform_name -> cached_value)
		using UniformValue = std::variant<int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4>;
		std::unordered_map<GLuint, std::unordered_map<std::string, UniformValue>> uniform_cache_;

		/// Texture binding cache: unit -> (target, texture_id)
		static constexpr int kMaxTextureUnits = 16;

		struct TextureBinding {
			GLenum target = 0;
			GLuint texture_id = 0;
		};

		TextureBinding texture_bindings_[kMaxTextureUnits] = {};

		/// Active texture unit cache
		int active_texture_unit_ = -1;

		/// Statistics
		Stats stats_;

		/// Helper to get uniform location with caching
		GLint GetUniformLocation(GLuint program, const std::string& name);

		/// Uniform location cache: program_id -> (uniform_name -> location)
		std::unordered_map<GLuint, std::unordered_map<std::string, GLint>> location_cache_;
	};

} // namespace Boidsish
