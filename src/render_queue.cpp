#include "render_queue.h"
#include <algorithm>

namespace Boidsish {

    void RenderQueue::Submit(const RenderPacket& packet) {
        // Extract layer from the highest 8 bits of the sort key
        RenderLayer layer = static_cast<RenderLayer>(packet.sort_key >> 56);
        size_t layer_idx = static_cast<size_t>(layer);

        if (layer_idx < 5) {
            m_layers[layer_idx].push_back(packet);
        }
    }

    void RenderQueue::Sort() {
        // Sort packets in each layer bucket ascending by sort_key.
        // Higher bits in the key represent higher priority sorting criteria.
        for (int i = 0; i < 5; ++i) {
            std::sort(m_layers[i].begin(), m_layers[i].end(), [](const RenderPacket& a, const RenderPacket& b) {
                return a.sort_key < b.sort_key;
            });
        }
    }

    void RenderQueue::Clear() {
        for (int i = 0; i < 5; ++i) {
            m_layers[i].clear();
        }
    }

} // namespace Boidsish
