#include "render_queue.h"
#include <algorithm>

namespace Boidsish {

    void RenderQueue::Submit(const RenderPacket& packet) {
        m_packets.push_back(packet);
    }

    void RenderQueue::Sort() {
        // Sort packets ascending by sort_key.
        // Higher bits in the key represent higher priority sorting criteria.
        std::sort(m_packets.begin(), m_packets.end(), [](const RenderPacket& a, const RenderPacket& b) {
            return a.sort_key < b.sort_key;
        });
    }

    void RenderQueue::Clear() {
        m_packets.clear();
    }

} // namespace Boidsish
