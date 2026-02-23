#include "render_queue.h"

#include <algorithm>

namespace Boidsish {

	void RenderQueue::Submit(const RenderPacket& packet) {
		// Extract layer from the highest 8 bits of the sort key
		RenderLayer layer = static_cast<RenderLayer>(packet.sort_key >> 56);
		size_t      layer_idx = static_cast<size_t>(layer);

		if (layer_idx < 5) {
			std::lock_guard<std::mutex> lock(m_mutex);
			m_layers[layer_idx].push_back(packet);
		}
	}

	void RenderQueue::Submit(std::vector<RenderPacket>&& packets) {
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& packet : packets) {
			RenderLayer layer = static_cast<RenderLayer>(packet.sort_key >> 56);
			size_t      layer_idx = static_cast<size_t>(layer);
			if (layer_idx < 5) {
				m_layers[layer_idx].push_back(std::move(packet));
			}
		}
	}

	void RenderQueue::Sort(task_thread_pool::task_thread_pool& pool) {
		// THREAD SAFETY: Hold mutex during sort to prevent concurrent Submit()
		// from modifying layers while we're sorting them.
		std::lock_guard<std::mutex> lock(m_mutex);

		// Sort packets in each layer bucket ascending by sort_key.
		// Higher bits in the key represent higher priority sorting criteria.
		std::vector<std::future<void>> futures;
		for (int i = 0; i < 5; ++i) {
			futures.push_back(pool.submit([this, i]() {
				std::sort(m_layers[i].begin(), m_layers[i].end(), [](const RenderPacket& a, const RenderPacket& b) {
					return a.sort_key < b.sort_key;
				});
			}));
		}

		for (auto& f : futures) {
			f.get();
		}
	}

	void RenderQueue::Clear() {
		for (int i = 0; i < 5; ++i) {
			m_layers[i].clear();
		}
	}

} // namespace Boidsish
