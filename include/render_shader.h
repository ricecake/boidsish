#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "handle.h"
#include "shader.h"
#include <glm/glm.hpp>

namespace Boidsish {

	/**
	 * @brief Abstract base class for shaders in the data-driven rendering system.
	 *
	 * This class represents a shader program and its required inputs (uniforms).
	 * It wraps the low-level Shader object and provides a mechanism to track
	 * and flush uniform changes.
	 */
	class RenderShader {
	public:
		struct Field {
			std::string name;
			// Optional: add type information here if needed for validation
		};

		explicit RenderShader(std::shared_ptr<ShaderBase> backingShader): m_backingShader(std::move(backingShader)) {}

		virtual ~RenderShader() = default;

		/**
		 * @brief Get the list of fields (uniforms) required by this shader.
		 */
		virtual const std::vector<Field>& GetRequiredFields() const {
			static std::vector<Field> empty;
			return empty;
		}

		/**
		 * @brief Queue a uniform update.
		 * Changes are not applied to the GPU until Flush() is called.
		 */
		void SetUniform(const std::string& name, const ShaderBase::UniformValue& value) {
			m_pendingUniforms[name] = value;
		}

		/**
		 * @brief Apply all pending uniform changes to the backing shader.
		 */
		void Flush() {
			if (!m_backingShader || m_pendingUniforms.empty())
				return;

			m_backingShader->use();
			for (const auto& [name, value] : m_pendingUniforms) {
				ApplyUniform(name, value);
			}
			m_pendingUniforms.clear();
		}

		/**
		 * @brief Get the underlying low-level shader object.
		 */
		std::shared_ptr<ShaderBase> GetBackingShader() const { return m_backingShader; }

	protected:
		/**
		 * @brief Helper to apply a UniformValue to the backing shader.
		 */
		void ApplyUniform(const std::string& name, const ShaderBase::UniformValue& value) {
			std::visit(
				[&](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, std::monostate>)
						return;
					else if constexpr (std::is_same_v<T, bool>)
						m_backingShader->setBool(name, arg);
					else if constexpr (std::is_same_v<T, int>)
						m_backingShader->setInt(name, arg);
					else if constexpr (std::is_same_v<T, float>)
						m_backingShader->setFloat(name, arg);
					else if constexpr (std::is_same_v<T, glm::vec2>)
						m_backingShader->setVec2(name, arg);
					else if constexpr (std::is_same_v<T, glm::vec3>)
						m_backingShader->setVec3(name, arg);
					else if constexpr (std::is_same_v<T, glm::vec4>)
						m_backingShader->setVec4(name, arg);
					else if constexpr (std::is_same_v<T, glm::mat2>)
						m_backingShader->setMat2(name, arg);
					else if constexpr (std::is_same_v<T, glm::mat3>)
						m_backingShader->setMat3(name, arg);
					else if constexpr (std::is_same_v<T, glm::mat4>)
						m_backingShader->setMat4(name, arg);
					else if constexpr (std::is_same_v<T, std::vector<int>>)
						m_backingShader->setIntArray(name, arg.data(), static_cast<int>(arg.size()));
				},
				value.value
			);
		}

		std::shared_ptr<ShaderBase>                               m_backingShader;
		std::unordered_map<std::string, ShaderBase::UniformValue> m_pendingUniforms;
	};

	// Shader handle type using the generic Handle system
	using ShaderHandle = Handle<RenderShader>;

} // namespace Boidsish
