#ifndef GRAMMAR_HPP
#define GRAMMAR_HPP

#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>

class WordInterner {
private:
	std::unordered_map<std::string, uint32_t> string_to_id_;
	std::vector<std::string>                  id_to_string_;

public:
	uint32_t get_or_intern(const std::string& str) {
		auto it = string_to_id_.find(str);
		if (it != string_to_id_.end()) {
			return it->second;
		}
		uint32_t id = static_cast<uint32_t>(id_to_string_.size());
		string_to_id_[str] = id;
		id_to_string_.push_back(str);
		return id;
	}

	const std::string& get_string(uint32_t id) const {
		if (id >= id_to_string_.size()) {
			static const std::string empty = "";
			return empty;
		}
		return id_to_string_[id];
	}

	size_t size() const { return id_to_string_.size(); }
};

// 1 byte per edge type saves memory (e.g., 0 = synonym, 1 = hypernym)
struct Edge {
	uint32_t target_id;
	uint8_t  edge_type;
};

struct NodeData {
	uint32_t edge_offset; // Index into the global edge array
	uint32_t edge_count;
};

// Global tightly packed structures
inline std::vector<NodeData> graph_nodes; // Index matches the word's integer ID
inline std::vector<Edge>     graph_edges; // All edges packed linearly

class GraphCompiler {
public:
	static void load_triples(const std::string& filename, WordInterner& interner) {
		std::ifstream file(filename);
		if (!file.is_open())
			return;

		// Temporary structure to hold the graph before packing
		std::unordered_map<uint32_t, std::vector<Edge>> adj;

		std::string line;
		while (std::getline(file, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream iss(line);
			std::string        subject, predicate, object;
			if (!(iss >> subject >> predicate >> object))
				continue;

			uint32_t s_id = interner.get_or_intern(subject);
			uint32_t o_id = interner.get_or_intern(object);

			// Edge types could be mapped from predicate strings
			uint8_t type = 0;
			if (predicate == "is_a")
				type = 1;
			else if (predicate == "part_of")
				type = 2;

			adj[s_id].push_back({o_id, type});
		}

		// Pack into contiguous arrays
		uint32_t max_id = 0;
		for (const auto& [id, _] : adj)
			max_id = std::max(max_id, id);

		graph_nodes.assign(max_id + 1, {0, 0});
		graph_edges.clear();

		for (uint32_t i = 0; i <= max_id; ++i) {
			auto it = adj.find(i);
			if (it != adj.end()) {
				graph_nodes[i].edge_offset = static_cast<uint32_t>(graph_edges.size());
				graph_nodes[i].edge_count = static_cast<uint32_t>(it->second.size());
				for (const auto& edge : it->second) {
					graph_edges.push_back(edge);
				}
			}
		}
	}

	static void compute_heatmap(const std::vector<uint32_t>& targets, float* heatmap, size_t heatmap_size, int max_bfs_depth = 3) {
		if (!heatmap)
			return;
		std::fill(heatmap, heatmap + heatmap_size, 0.0f);

		struct BFSNode {
			uint32_t id;
			int      depth;
		};
		std::queue<BFSNode> q;
		for (uint32_t target : targets) {
			if (target < heatmap_size) {
				heatmap[target] = 1.0f;
				q.push({target, 0});
			}
		}

		while (!q.empty()) {
			BFSNode current = q.front();
			q.pop();

			if (current.depth >= max_bfs_depth)
				continue;

			if (current.id >= graph_nodes.size())
				continue;

			const NodeData& node = graph_nodes[current.id];
			for (uint32_t i = 0; i < node.edge_count; ++i) {
				const Edge& edge = graph_edges[node.edge_offset + i];
				if (edge.target_id < heatmap_size && heatmap[edge.target_id] == 0.0f) {
					// Decay based on depth
					heatmap[edge.target_id] = 1.0f / (current.depth + 2.0f);
					q.push({edge.target_id, current.depth + 1});
				}
			}
		}
	}
};

// Context passed down the recursion tree
struct GenerationContext {
	int    depth = 0;
	int    max_depth = 8;         // Fallback to terminals if exceeded
	int    frustration_level = 0; // 0 = normal, higher = shorter/ruder
	float* semantic_heatmap = nullptr;
};

class Grammar {
private:
	struct RuleSet {
		std::vector<double>                base_weights;
		std::vector<std::vector<uint32_t>> productions;
	};

	std::unordered_map<uint32_t, RuleSet> grammar_;
	WordInterner                          interner_;
	std::mt19937                          rng_;

	void generate_recursive(uint32_t symbol_id, GenerationContext& ctx, std::string& output) {
		auto it = grammar_.find(symbol_id);
		if (it == grammar_.end()) {
			// Terminal symbol
			if (!output.empty() && output.back() != ' ') {
				output += ' ';
			}
			output += interner_.get_string(symbol_id);
			return;
		}

		// Calculate dynamic weights
		double              total_weight = 0.0;
		std::vector<double> current_weights;
		current_weights.reserve(it->second.base_weights.size());

		for (size_t i = 0; i < it->second.base_weights.size(); ++i) {
			double weight = it->second.base_weights[i];

			// Apply semantic heatmap boost if any word in production matches
			if (ctx.semantic_heatmap) {
				for (uint32_t word_id : it->second.productions[i]) {
					if (word_id < interner_.size() && ctx.semantic_heatmap[word_id] > 0) {
						weight *= (1.0 + ctx.semantic_heatmap[word_id] * 100.0);
					}
				}
			}

			// Dynamic Adjustments:
			if (ctx.depth >= ctx.max_depth - ctx.frustration_level) {
				// Heuristic: shorter productions are usually terminals
				if (it->second.productions[i].size() > 1) {
					weight *= 0.1; // Aggressive decay
				}
			}

			current_weights.push_back(weight);
			total_weight += weight;
		}

		// Inline weighted random selection
		std::uniform_real_distribution<double> dist(0.0, total_weight);
		double                                 roll = dist(rng_);
		size_t                                 idx = 0;

		for (size_t i = 0; i < current_weights.size(); ++i) {
			roll -= current_weights[i];
			if (roll <= 0.0) {
				idx = i;
				break;
			}
		}

		// Recurse
		const auto& production = it->second.productions[idx];
		ctx.depth++;
		for (uint32_t s_id : production) {
			generate_recursive(s_id, ctx, output);
		}
		ctx.depth--;
	}

	void apply_fast_punctuation_formatting(std::string& text) {
		// Linear pass state machine replacing the expensive std::regex chain.
		// Needs expansion based on your specific punctuation rules, but avoids
		// evaluating regex ASTs at runtime.
		std::string cleaned;
		cleaned.reserve(text.size());
		bool last_was_space = false;

		for (size_t i = 0; i < text.size(); ++i) {
			char c = text[i];
			// Collapse spaces
			if (c == ' ') {
				if (!last_was_space)
					cleaned += c;
				last_was_space = true;
				continue;
			}
			// Attach punctuation directly to previous word
			if (c == ',' || c == '.' || c == '?' || c == '!') {
				if (!cleaned.empty() && cleaned.back() == ' ') {
					cleaned.pop_back();
				}
			}
			cleaned += c;
			last_was_space = false;
		}
		text = cleaned;
	}

public:
	explicit Grammar(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open())
			throw std::runtime_error("Could not open grammar: " + filename);
		std::random_device rd;
		rng_ = std::mt19937(rd());

		std::string line;
		while (std::getline(file, line)) {
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream       iss(line);
			std::vector<std::string> tokens;
			std::string              token;

			while (iss >> token) {
				size_t comment_pos = token.find('#');
				if (comment_pos != std::string::npos) {
					if (comment_pos > 0)
						tokens.push_back(token.substr(0, comment_pos));
					break;
				}
				tokens.push_back(token);
			}

			if (tokens.size() < 2)
				continue;

			double               weight = std::stod(tokens[0]);
			uint32_t             lhs_id = interner_.get_or_intern(tokens[1]);
			std::vector<uint32_t> rhs_ids;
			for (size_t i = 2; i < tokens.size(); ++i) {
				rhs_ids.push_back(interner_.get_or_intern(tokens[i]));
			}

			grammar_[lhs_id].base_weights.push_back(weight);
			grammar_[lhs_id].productions.push_back(rhs_ids);
		}
	}

	std::string generate(GenerationContext ctx, const std::string& start = "ROOT") {
		std::string output;
		output.reserve(256);

		uint32_t start_id = interner_.get_or_intern(start);
		generate_recursive(start_id, ctx, output);
		apply_fast_punctuation_formatting(output);

		return output;
	}

	std::string generate(const std::string& start = "ROOT") { return generate(GenerationContext(), start); }

	WordInterner& get_interner() { return interner_; }
};

#endif // GRAMMAR_HPP