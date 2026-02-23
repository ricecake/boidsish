#pragma once

#include <algorithm>
#include <mutex>
#include <vector>

#include "geometry.h"
#include "task_thread_pool.hpp"

namespace Boidsish {

	/**
	 * @brief A queue that collects RenderPackets and sorts them for efficient rendering.
	 *
	 * The RenderQueue acts as an intermediate storage between objects (Geometry)
	 * and the low-level renderer. It allows for batching, sorting by state,
	 * and minimizing OpenGL state changes.
	 *
	 * ## Thread Safety Contract
	 * - Submit() is thread-safe (mutex protected) and can be called from multiple threads
	 * - Sort() acquires the mutex and should not be called concurrently with Submit()
	 * - GetPackets() returns a reference that is valid until the next Submit() or Clear()
	 *   call; caller must ensure no concurrent modifications
	 * - Clear() should only be called from the main thread after rendering is complete
	 *
	 * Typical frame sequence:
	 * 1. Clear() - main thread
	 * 2. Submit() - worker threads (parallel packet generation)
	 * 3. Sort() - main thread (after all Submit() calls complete)
	 * 4. GetPackets() - main thread (during rendering)
	 */
	class RenderQueue {
	public:
		RenderQueue() = default;
		~RenderQueue() = default;

		/**
		 * @brief Submit a RenderPacket to the queue (thread-safe).
		 * The packet is automatically routed to the correct layer bucket.
		 */
		void Submit(const RenderPacket& packet);

		/**
		 * @brief Batch submit RenderPackets to the queue (thread-safe).
		 */
		void Submit(std::vector<RenderPacket>&& packets);

		/**
		 * @brief Sort the submitted packets in each layer based on their sort_key.
		 * Parallelizes sorting across layer buckets using the provided thread pool.
		 * @note Acquires mutex; do not call concurrently with Submit().
		 */
		void Sort(task_thread_pool::task_thread_pool& pool);

		/**
		 * @brief Get the list of sorted packets for a specific layer.
		 * @warning The returned reference is invalidated by Submit() or Clear().
		 *          Only call after Sort() and before the next frame's Submit().
		 */
		const std::vector<RenderPacket>& GetPackets(RenderLayer layer) const {
			return m_layers[static_cast<size_t>(layer)];
		}

		/**
		 * @brief Clear the queue for the next frame.
		 * @note Should only be called from main thread after rendering completes.
		 */
		void Clear();

	private:
		// Use separate buckets for each RenderLayer
		// RenderLayer: Background=0, Opaque=1, Transparent=2, UI=3, Overlay=4
		std::vector<RenderPacket> m_layers[5];
		mutable std::mutex        m_mutex;
	};

} // namespace Boidsish
