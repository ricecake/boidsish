#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "handle.h"
#include "material.h"
#include "pool.h"

namespace Boidsish {

	/**
	 * @brief A registry for Material objects, indexed by MaterialHandle.
	 *
	 * Similar to ShaderTable, this provides a centralized place to manage
	 * material data, allowing materials to be shared and sorted efficiently.
	 */
	class MaterialTable {
	public:
		MaterialTable() = default;
		~MaterialTable() = default;

		/**
		 * @brief Register a new Material in the table.
		 * @return A unique handle for the registered material.
		 */
		MaterialHandle Register(std::unique_ptr<Material> material) {
			std::string name = material->name;
			auto handle = m_materialPool.Allocate(std::move(material));
			m_nameToId[name] = handle.GetId();
			return MaterialHandle{handle.GetId()};
		}

		/**
		 * @brief Get a pointer to a registered material by its handle.
		 * @return Pointer to the material, or nullptr if not found.
		 */
		Material* Get(MaterialHandle handle) const {
			if (auto m = m_materialPool.Get(handle.id)) {
				return m->get();
			}
			return nullptr;
		}

		/**
		 * @brief Find a material by its name.
		 * @return Handle to the material, or an invalid handle if not found.
		 */
		MaterialHandle FindByName(const std::string& name) const {
			// Since Pool doesn't easily expose IDs during ForEach, we have to iterate
			// and GetById if we want to return a handle.
			// In practice, FindByName is only called during initialization/loading.
			// However, to keep it efficient, we can maintain a Name -> ID map.
			auto it = m_nameToId.find(name);
			return it != m_nameToId.end() ? MaterialHandle{it->second} : MaterialHandle();
		}

		/**
		 * @brief Unregister and destroy a material by its handle.
		 */
		void Unregister(MaterialHandle handle) {
			if (auto m = m_materialPool.Get(handle.id)) {
				m_nameToId.erase((*m)->name);
				m_materialPool.FreeById(handle.id);
			}
		}

		/**
		 * @brief Clear all registered materials.
		 */
		void Clear() {
			m_materialPool.Clear();
			m_nameToId.clear();
		}

	private:
		Pool<std::unique_ptr<Material>> m_materialPool;
		std::unordered_map<std::string, uint32_t> m_nameToId;
	};

} // namespace Boidsish
