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

	void RenderQueue::BuildBatches(ShaderHandle shadow_shader_override) {
		m_shadow_batches.clear();
		m_shadow_indices.clear();
		const auto& opaque_packets = m_layers[static_cast<size_t>(RenderLayer::Opaque)];

		auto can_batch = [](const RenderPacket& a, const RenderPacket& b, bool is_shadow, const std::optional<ShaderHandle>& override_shader) {
			if (a.vao != b.vao)
				return false;
			if (a.draw_mode != b.draw_mode)
				return false;
			if (a.index_type != b.index_type)
				return false;
			if ((a.index_count > 0) != (b.index_count > 0))
				return false;
			if (a.no_cull != b.no_cull)
				return false;

			if (!override_shader.has_value()) {
				if (a.shader_id != b.shader_id)
					return false;
			}

			if (a.uniforms.is_colossal != b.uniforms.is_colossal)
				return false;
			if (a.uniforms.use_ssbo_instancing != b.uniforms.use_ssbo_instancing)
				return false;
			if (a.uniforms.use_skinning != b.uniforms.use_skinning)
				return false;
			if (a.uniforms.bone_matrices_offset != b.uniforms.bone_matrices_offset)
				return false;

			if (!is_shadow) {
				if (a.textures.size() != b.textures.size())
					return false;
				for (size_t j = 0; j < a.textures.size(); ++j) {
					if (a.textures[j].id != b.textures[j].id || a.textures[j].type != b.textures[j].type)
						return false;
				}
			}

			return true;
		};

		// 1. Build shadow batches (from opaque layer)
		{
			const RenderPacket* last_processed_packet = nullptr;
			uint32_t            mdi_elements_count = 0;
			uint32_t            mdi_arrays_count = 0;

			for (size_t j = 0; j < opaque_packets.size(); ++j) {
				const auto& packet = opaque_packets[j];
				if (!packet.casts_shadows || packet.shader_id == 0)
					continue;

				bool is_indexed = (packet.index_count > 0);

				if (m_shadow_batches.empty() || !last_processed_packet || !can_batch(*last_processed_packet, packet, true, shadow_shader_override)) {
					Batch new_batch;
					new_batch.shader_handle = shadow_shader_override;
					new_batch.shader_id = 999999;
					new_batch.vao = packet.vao;
					new_batch.draw_mode = packet.draw_mode;
					new_batch.index_type = packet.index_type;
					new_batch.no_cull = packet.no_cull;
					new_batch.textures = {}; // Not used in shadow pass
					new_batch.first_command = is_indexed ? mdi_elements_count : mdi_arrays_count;
					new_batch.command_count = 0;
					new_batch.is_indexed = is_indexed;
					new_batch.base_uniform_index = static_cast<uint32_t>(m_shadow_indices.size());
					m_shadow_batches.push_back(new_batch);
				}

				m_shadow_batches.back().command_count++;
				m_shadow_indices.push_back(static_cast<uint32_t>(j));
				if (is_indexed)
					mdi_elements_count++;
				else
					mdi_arrays_count++;

				last_processed_packet = &packet;
			}
		}

		// 2. Build layer batches
		for (int i = 0; i < 5; ++i) {
			m_batches[i].clear();
			m_valid_indices[i].clear();
			const auto& packets = m_layers[i];
			if (packets.empty())
				continue;

			const RenderPacket* last_processed_packet = nullptr;
			uint32_t            mdi_elements_count = 0;
			uint32_t            mdi_arrays_count = 0;

			for (size_t j = 0; j < packets.size(); ++j) {
				const auto& packet = packets[j];
				if (packet.shader_id == 0)
					continue;

				bool is_indexed = (packet.index_count > 0);

				if (m_batches[i].empty() || !last_processed_packet || !can_batch(*last_processed_packet, packet, false, std::nullopt)) {
					Batch new_batch;
					new_batch.shader_handle = packet.shader_handle;
					new_batch.shader_id = packet.shader_id;
					new_batch.vao = packet.vao;
					new_batch.draw_mode = packet.draw_mode;
					new_batch.index_type = packet.index_type;
					new_batch.no_cull = packet.no_cull;
					new_batch.textures = packet.textures;
					new_batch.first_command = is_indexed ? mdi_elements_count : mdi_arrays_count;
					new_batch.command_count = 0;
					new_batch.is_indexed = is_indexed;
					new_batch.base_uniform_index = static_cast<uint32_t>(m_valid_indices[i].size());
					m_batches[i].push_back(new_batch);
				}

				m_batches[i].back().command_count++;
				m_valid_indices[i].push_back(static_cast<uint32_t>(j));
				if (is_indexed)
					mdi_elements_count++;
				else
					mdi_arrays_count++;

				last_processed_packet = &packet;
			}
		}
	}

	void RenderQueue::Clear() {
		for (int i = 0; i < 5; ++i) {
			m_layers[i].clear();
			m_batches[i].clear();
			m_valid_indices[i].clear();
			m_layer_preps[i] = {};
		}
		m_shadow_batches.clear();
		m_shadow_indices.clear();
		m_shadow_prep = {};
	}

} // namespace Boidsish
