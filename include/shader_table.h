#pragma once

#include <memory>
#include <unordered_map>

#include "handle.h"
#include "render_shader.h"

namespace Boidsish {

	/**
	 * @brief A registry for RenderShader objects, indexed by ShaderHandle.
	 *
	 * This class provides a centralized place to store and manage shaders,
	 * allowing them to be referred to by handles rather than raw pointers.
	 */
	class ShaderTable {
	public:
		ShaderTable() = default;
		~ShaderTable() = default;

		/**
		 * @brief Register a new RenderShader in the table.
		 * @return A unique handle for the registered shader.
		 */
		ShaderHandle Register(std::unique_ptr<RenderShader> shader) {
			ShaderHandle handle(++m_nextId);
			m_shaders[handle] = std::move(shader);
			return handle;
		}

		/**
		 * @brief Get a pointer to a registered shader by its handle.
		 * @return Pointer to the shader, or nullptr if not found.
		 */
		RenderShader* Get(ShaderHandle handle) const {
			auto it = m_shaders.find(handle);
			return it != m_shaders.end() ? it->second.get() : nullptr;
		}

		/**
		 * @brief Unregister and destroy a shader by its handle.
		 */
		void Unregister(ShaderHandle handle) { m_shaders.erase(handle); }

		/**
		 * @brief Flush all shaders in the table, applying pending uniform changes.
		 */
		void FlushAll() {
			for (auto& [handle, shader] : m_shaders) {
				shader->Flush();
			}
		}

	private:
		uint32_t                                                        m_nextId = 0;
		std::unordered_map<ShaderHandle, std::unique_ptr<RenderShader>> m_shaders;
	};

} // namespace Boidsish
