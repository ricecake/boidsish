#pragma once

#include <algorithm>
#include <mutex>
#include <vector>

#include "geometry.h"
#include "task_thread_pool.hpp"

namespace Boidsish {

	/**
	 * @brief Represents a batch of RenderPackets with identical state.
	 * Allows for single-bind, multi-draw indirect (MDI) execution.
	 */
	struct Batch {
		ShaderHandle                           shader_handle;
		unsigned int                           shader_id;
		unsigned int                           vao;
		unsigned int                           draw_mode;
		unsigned int                           index_type;
		bool                                   no_cull;
		std::vector<RenderPacket::TextureInfo> textures;
		uint32_t                               first_command;
		uint32_t                               command_count;
		bool                                   is_indexed;
		uint32_t                               base_uniform_index;
	};

	/**
	 * @brief Result of preparing draw data for a specific layer.
	 */
	struct LayerDrawPrep {
		uint32_t uniform_start = 0;
		uint32_t uniform_count = 0;
		uint32_t elements_start = 0;
		uint32_t elements_count = 0;
		uint32_t arrays_start = 0;
		uint32_t arrays_count = 0;
		uint32_t bone_start = 0;
		uint32_t bone_count = 0;
		uint32_t frustum_index = 0;
	};

	/**
	 * @brief A queue that collects RenderPackets and sorts them for efficient rendering.
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
		 * @brief Get a mutable reference to the list of packets for a specific layer.
		 */
		std::vector<RenderPacket>& GetPacketsMutable(RenderLayer layer) { return m_layers[static_cast<size_t>(layer)]; }

		/**
		 * @brief Get the pre-built batches for a specific layer.
		 */
		const std::vector<Batch>& GetBatches(RenderLayer layer) const {
			return m_batches[static_cast<size_t>(layer)];
		}

		/**
		 * @brief Get the pre-built batches for the shadow pass.
		 */
		const std::vector<Batch>& GetShadowBatches() const { return m_shadow_batches; }

		/**
		 * @brief Get the indices of valid packets for a specific layer.
		 */
		const std::vector<uint32_t>& GetValidIndices(RenderLayer layer) const {
			return m_valid_indices[static_cast<size_t>(layer)];
		}

		/**
		 * @brief Get the indices of valid shadow-casting packets.
		 */
		const std::vector<uint32_t>& GetShadowIndices() const { return m_shadow_indices; }

		/**
		 * @brief Build MDI batches for all layers and the shadow pass.
		 * @param shadow_shader_override Shader handle to use for shadow packets.
		 */
		void BuildBatches(ShaderHandle shadow_shader_override);

		/**
		 * @brief Preparation results for the current frame's prepared data.
		 */
		const LayerDrawPrep& GetPrep(RenderLayer layer) const { return m_layer_preps[static_cast<size_t>(layer)]; }
		const LayerDrawPrep& GetShadowPrep() const { return m_shadow_prep; }
		void                 SetPrep(RenderLayer layer, const LayerDrawPrep& prep) { m_layer_preps[static_cast<size_t>(layer)] = prep; }
		void                 SetShadowPrep(const LayerDrawPrep& prep) { m_shadow_prep = prep; }

		/**
		 * @brief Clear the queue for the next frame.
		 * @note Should only be called from main thread after rendering completes.
		 */
		void Clear();

	private:
		// Use separate buckets for each RenderLayer
		// RenderLayer: Background=0, Opaque=1, Transparent=2, UI=3, Overlay=4
		std::vector<RenderPacket> m_layers[5];
		std::vector<Batch>        m_batches[5];
		std::vector<Batch>        m_shadow_batches;

		// Parallel arrays to track valid (unskipped) packet indices within each layer
		std::vector<uint32_t> m_valid_indices[5];
		std::vector<uint32_t> m_shadow_indices;

		// Preparation results
		LayerDrawPrep m_layer_preps[5];
		LayerDrawPrep m_shadow_prep;

		mutable std::mutex m_mutex;
	};

} // namespace Boidsish
