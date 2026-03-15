#pragma once

#include <memory>
#include <unordered_map>

#include "handle.h"
#include "render_shader.h"
#include "pool.h"

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
			auto handle = m_shaderPool.Allocate(std::move(shader));
			return ShaderHandle{handle.GetId()};
		}

		/**
		 * @brief Get a pointer to a registered shader by its handle.
		 * @return Pointer to the shader, or nullptr if not found.
		 */
		RenderShader* Get(ShaderHandle handle) const {
			if (auto s = m_shaderPool.Get(handle.id)) {
				return s->get();
			}
			return nullptr;
		}

		/**
		 * @brief Unregister and destroy a shader by its handle.
		 */
		void Unregister(ShaderHandle handle) {
			m_shaderPool.FreeById(handle.id);
		}

		/**
		 * @brief Flush all shaders in the table, applying pending uniform changes.
		 */
		void FlushAll() {
			m_shaderPool.ForEach([](std::unique_ptr<RenderShader>& shader) {
				shader->Flush();
			});
		}

	private:
		Pool<std::unique_ptr<RenderShader>> m_shaderPool;
	};

} // namespace Boidsish
