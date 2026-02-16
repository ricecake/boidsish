#pragma once

#include <vector>
#include <algorithm>
#include "geometry.h"

namespace Boidsish {

    /**
     * @brief A queue that collects RenderPackets and sorts them for efficient rendering.
     *
     * The RenderQueue acts as an intermediate storage between objects (Geometry)
     * and the low-level renderer. It allows for batching, sorting by state,
     * and minimizing OpenGL state changes.
     */
    class RenderQueue {
    public:
        RenderQueue() = default;
        ~RenderQueue() = default;

        /**
         * @brief Submit a RenderPacket to the queue.
         * The packet is automatically routed to the correct layer bucket.
         */
        void Submit(const RenderPacket& packet);

        /**
         * @brief Sort the submitted packets in each layer based on their sort_key.
         */
        void Sort();

        /**
         * @brief Get the list of sorted packets for a specific layer.
         */
        const std::vector<RenderPacket>& GetPackets(RenderLayer layer) const {
            return m_layers[static_cast<size_t>(layer)];
        }

        /**
         * @brief Clear the queue for the next frame.
         */
        void Clear();

    private:
        // Use separate buckets for each RenderLayer to sort "as they come in"
        // RenderLayer: Background=0, Opaque=1, Transparent=2, UI=3, Overlay=4
        std::vector<RenderPacket> m_layers[5];
    };

} // namespace Boidsish
