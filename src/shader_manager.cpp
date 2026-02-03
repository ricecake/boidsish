#include "shader_manager.h"

#include <glm/gtc/type_ptr.hpp>

namespace Boidsish {

	ShaderManager& ShaderManager::GetInstance() {
		static ShaderManager instance;
		return instance;
	}

	bool ShaderManager::Use(GLuint program_id) {
		if (program_id == current_program_) {
			stats_.shader_cache_hits++;
			return false;
		}

		glUseProgram(program_id);
		current_program_ = program_id;
		stats_.shader_switches++;
		return true;
	}

	GLint ShaderManager::GetUniformLocation(GLuint program, const std::string& name) {
		auto& program_cache = location_cache_[program];
		auto  it = program_cache.find(name);
		if (it != program_cache.end()) {
			return it->second;
		}

		GLint location = glGetUniformLocation(program, name.c_str());
		program_cache[name] = location;
		return location;
	}

	void ShaderManager::SetUniform(const std::string& name, int value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<int>(it->second) && std::get<int>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniform1i(location, value);
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	void ShaderManager::SetUniform(const std::string& name, float value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<float>(it->second) &&
		    std::get<float>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniform1f(location, value);
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	void ShaderManager::SetUniform(const std::string& name, const glm::vec2& value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::vec2>(it->second) &&
		    std::get<glm::vec2>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniform2fv(location, 1, glm::value_ptr(value));
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	void ShaderManager::SetUniform(const std::string& name, const glm::vec3& value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::vec3>(it->second) &&
		    std::get<glm::vec3>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniform3fv(location, 1, glm::value_ptr(value));
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	void ShaderManager::SetUniform(const std::string& name, const glm::vec4& value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::vec4>(it->second) &&
		    std::get<glm::vec4>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniform4fv(location, 1, glm::value_ptr(value));
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	void ShaderManager::SetUniform(const std::string& name, const glm::mat3& value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::mat3>(it->second) &&
		    std::get<glm::mat3>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	void ShaderManager::SetUniform(const std::string& name, const glm::mat4& value) {
		if (current_program_ == 0)
			return;

		auto& shader_cache = uniform_cache_[current_program_];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::mat4>(it->second) &&
		    std::get<glm::mat4>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return;
		}

		GLint location = GetUniformLocation(current_program_, name);
		if (location >= 0) {
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
			shader_cache[name] = value;
			stats_.uniform_sets++;
		}
	}

	bool ShaderManager::BindTexture(int unit, GLenum target, GLuint texture_id) {
		if (unit < 0 || unit >= kMaxTextureUnits)
			return false;

		auto& binding = texture_bindings_[unit];
		if (binding.target == target && binding.texture_id == texture_id) {
			stats_.texture_cache_hits++;
			return false;
		}

		// Only switch active texture unit if needed
		if (active_texture_unit_ != unit) {
			glActiveTexture(GL_TEXTURE0 + unit);
			active_texture_unit_ = unit;
		}

		glBindTexture(target, texture_id);
		binding.target = target;
		binding.texture_id = texture_id;
		stats_.texture_binds++;
		return true;
	}

	void ShaderManager::InvalidateCache() {
		current_program_ = 0;
		uniform_cache_.clear();
		for (auto& binding : texture_bindings_) {
			binding = {};
		}
		active_texture_unit_ = -1;
	}

	void ShaderManager::InvalidateShaderCache(GLuint program_id) {
		uniform_cache_.erase(program_id);
		location_cache_.erase(program_id);
	}

	// IsCached implementations - return true if cached AND matches, update cache if not
	bool ShaderManager::IsCached(GLuint program, const std::string& name, int value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<int>(it->second) && std::get<int>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

	bool ShaderManager::IsCached(GLuint program, const std::string& name, float value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<float>(it->second) &&
		    std::get<float>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

	bool ShaderManager::IsCached(GLuint program, const std::string& name, const glm::vec2& value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::vec2>(it->second) &&
		    std::get<glm::vec2>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

	bool ShaderManager::IsCached(GLuint program, const std::string& name, const glm::vec3& value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::vec3>(it->second) &&
		    std::get<glm::vec3>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

	bool ShaderManager::IsCached(GLuint program, const std::string& name, const glm::vec4& value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::vec4>(it->second) &&
		    std::get<glm::vec4>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

	bool ShaderManager::IsCached(GLuint program, const std::string& name, const glm::mat3& value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::mat3>(it->second) &&
		    std::get<glm::mat3>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

	bool ShaderManager::IsCached(GLuint program, const std::string& name, const glm::mat4& value) {
		auto& shader_cache = uniform_cache_[program];
		auto  it = shader_cache.find(name);
		if (it != shader_cache.end() && std::holds_alternative<glm::mat4>(it->second) &&
		    std::get<glm::mat4>(it->second) == value) {
			stats_.uniform_cache_hits++;
			return true;
		}
		shader_cache[name] = value;
		stats_.uniform_sets++;
		return false;
	}

} // namespace Boidsish
