#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include "material.h"
#include "handle.h"

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
            MaterialHandle handle(++m_nextId);
            m_materials[handle] = std::move(material);
            return handle;
        }

        /**
         * @brief Get a pointer to a registered material by its handle.
         * @return Pointer to the material, or nullptr if not found.
         */
        Material* Get(MaterialHandle handle) const {
            auto it = m_materials.find(handle);
            return it != m_materials.end() ? it->second.get() : nullptr;
        }

        /**
         * @brief Find a material by its name.
         * @return Handle to the material, or an invalid handle if not found.
         */
        MaterialHandle FindByName(const std::string& name) const {
            for (const auto& [handle, material] : m_materials) {
                if (material->name == name) {
                    return handle;
                }
            }
            return MaterialHandle();
        }

        /**
         * @brief Unregister and destroy a material by its handle.
         */
        void Unregister(MaterialHandle handle) {
            m_materials.erase(handle);
        }

        /**
         * @brief Clear all registered materials.
         */
        void Clear() {
            m_materials.clear();
            m_nextId = 0;
        }

    private:
        uint32_t m_nextId = 0;
        std::unordered_map<MaterialHandle, std::unique_ptr<Material>> m_materials;
    };

} // namespace Boidsish
