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
         */
        void Submit(const RenderPacket& packet);

        /**
         * @brief Sort the submitted packets based on their sort_key.
         */
        void Sort();

        /**
         * @brief Get the list of sorted packets.
         */
        const std::vector<RenderPacket>& GetPackets() const { return m_packets; }

        /**
         * @brief Get the list of packets for modification (e.g., updating sort keys).
         */
        std::vector<RenderPacket>& GetPacketsMutable() { return m_packets; }

        /**
         * @brief Clear the queue for the next frame.
         */
        void Clear();

        /**
         * @brief Reserve capacity in the internal vector to avoid reallocations.
         */
        void Reserve(size_t capacity) { m_packets.reserve(capacity); }

    private:
        std::vector<RenderPacket> m_packets;
    };

} // namespace Boidsish
