#ifndef GRAMMAR_HPP
#define GRAMMAR_HPP

#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
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

	struct BinarizedRule {
		uint32_t lhs;
		uint32_t rhs1;
		uint32_t rhs2;
		double   log_prob;
		bool     is_terminal;
		uint32_t original_lhs;
		int      original_idx;
	};

	std::unordered_map<uint32_t, RuleSet> grammar_;
	std::vector<BinarizedRule>            binarized_grammar_;
	bool                                  needs_binarization_ = true;
	WordInterner                          interner_;
	std::mt19937                          rng_;

	std::vector<uint32_t> tokenize(const std::string& text) {
		std::vector<uint32_t> tokens;
		std::string           current;
		for (size_t i = 0; i < text.size(); ++i) {
			unsigned char c = static_cast<unsigned char>(text[i]);
			if (std::isspace(c)) {
				if (!current.empty()) {
					tokens.push_back(interner_.get_or_intern(current));
					current.clear();
				}
			} else if (std::ispunct(c)) {
				if (!current.empty()) {
					tokens.push_back(interner_.get_or_intern(current));
					current.clear();
				}
				tokens.push_back(interner_.get_or_intern(std::string(1, static_cast<char>(c))));
			} else {
				current += static_cast<char>(std::tolower(c));
			}
		}
		if (!current.empty()) {
			tokens.push_back(interner_.get_or_intern(current));
		}
		return tokens;
	}

	void binarize() {
		if (!needs_binarization_)
			return;
		binarized_grammar_.clear();

		std::unordered_map<uint32_t, uint32_t> term_to_nt;

		for (auto& [lhs, ruleset] : grammar_) {
			double total_weight = 0;
			for (double w : ruleset.base_weights)
				total_weight += w;
			if (total_weight <= 0)
				continue;

			for (size_t i = 0; i < ruleset.productions.size(); ++i) {
				double      log_prob = std::log(ruleset.base_weights[i] / total_weight);
				const auto& prod = ruleset.productions[i];
				if (prod.empty())
					continue;

				if (prod.size() == 1) {
					bool is_terminal = (grammar_.find(prod[0]) == grammar_.end());
					binarized_grammar_.push_back({lhs, prod[0], 0, log_prob, is_terminal, lhs, static_cast<int>(i)});
				} else {
					std::vector<uint32_t> nts;
					for (uint32_t id : prod) {
						if (grammar_.find(id) != grammar_.end()) {
							nts.push_back(id);
						} else {
							if (term_to_nt.find(id) == term_to_nt.end()) {
								uint32_t nt_id = interner_.get_or_intern("__term_" + std::to_string(id));
								term_to_nt[id] = nt_id;
								binarized_grammar_.push_back({nt_id, id, 0, 0.0, true, 0, -1});
							}
							nts.push_back(term_to_nt[id]);
						}
					}

					uint32_t current_lhs = lhs;
					for (size_t j = 0; j < nts.size() - 2; ++j) {
						uint32_t synth_id = interner_.get_or_intern("__synth_" + std::to_string(lhs) + "_" + std::to_string(i) + "_" + std::to_string(j));
						// Only the first binarized segment carries the original rule link to avoid exponential weighting
						binarized_grammar_.push_back({current_lhs, nts[j], synth_id, (j == 0 ? log_prob : 0.0), false, (j == 0 ? lhs : 0), (j == 0 ? static_cast<int>(i) : -1)});
						current_lhs = synth_id;
					}
					// Only carry link if it was a 2-symbol production
					binarized_grammar_.push_back({current_lhs, nts[nts.size() - 2], nts[nts.size() - 1], (nts.size() == 2 ? log_prob : 0.0), false, (nts.size() == 2 ? lhs : 0), (nts.size() == 2 ? static_cast<int>(i) : -1)});
				}
			}
		}
		needs_binarization_ = false;
	}

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

	std::vector<uint32_t> parse_and_modify(const std::string& sentence, const std::string& start = "ROOT") {
		binarize();
		std::vector<uint32_t> tokens = tokenize(sentence);
		if (tokens.empty())
			return {};

		size_t n = tokens.size();
		// table[i][j][lhs] = {log_prob, rule_ptr}
		struct Entry {
			double               log_prob = -std::numeric_limits<double>::infinity();
			const BinarizedRule* rule = nullptr;
			size_t               split = 0;
			uint32_t             rhs1_entry_id = 0;
			uint32_t             rhs2_entry_id = 0;
		};

		std::vector<std::vector<std::unordered_map<uint32_t, Entry>>> table(n, std::vector<std::unordered_map<uint32_t, Entry>>(n));

		// Leaf nodes
		for (size_t i = 0; i < n; ++i) {
			for (const auto& rule : binarized_grammar_) {
				if (rule.is_terminal && rule.rhs1 == tokens[i]) {
					if (rule.log_prob > table[i][i][rule.lhs].log_prob) {
						table[i][i][rule.lhs] = {rule.log_prob, &rule};
					}
				}
			}
			// Handle unit productions (A -> B) at leaf level
			bool changed = true;
			while (changed) {
				changed = false;
				for (const auto& rule : binarized_grammar_) {
					if (!rule.is_terminal && rule.rhs2 == 0) {
						auto it = table[i][i].find(rule.rhs1);
						if (it != table[i][i].end()) {
							double lp = rule.log_prob + it->second.log_prob;
							if (lp > table[i][i][rule.lhs].log_prob) {
								table[i][i][rule.lhs] = {lp, &rule};
								changed = true;
							}
						}
					}
				}
			}
		}

		// Fill table
		for (size_t len = 1; len <= n; ++len) {
			for (size_t i = 0; i <= n - len; ++i) {
				size_t j = i + len - 1;

				if (len > 1) {
					for (size_t k = i; k < j; ++k) {
						for (const auto& rule : binarized_grammar_) {
							if (!rule.is_terminal && rule.rhs2 != 0) {
								auto it1 = table[i][k].find(rule.rhs1);
								auto it2 = table[k + 1][j].find(rule.rhs2);
								if (it1 != table[i][k].end() && it2 != table[k + 1][j].end()) {
									double lp = rule.log_prob + it1->second.log_prob + it2->second.log_prob;
									if (lp > table[i][j][rule.lhs].log_prob) {
										table[i][j][rule.lhs] = {lp, &rule, k};
									}
								}
							}
						}
					}
				}

				// Handle unit productions for this span
				bool changed = true;
				while (changed) {
					changed = false;
					for (const auto& rule : binarized_grammar_) {
						if (!rule.is_terminal && rule.rhs2 == 0) {
							auto it = table[i][j].find(rule.rhs1);
							if (it != table[i][j].end()) {
								double lp = rule.log_prob + it->second.log_prob;
								if (lp > table[i][j][rule.lhs].log_prob) {
									table[i][j][rule.lhs] = {lp, &rule};
									changed = true;
								}
							}
						}
					}
				}
			}
		}

		uint32_t start_id = interner_.get_or_intern(start);
		if (table[0][n - 1].count(start_id)) {
			// Backtrack and modify weights
			std::vector<std::pair<size_t, size_t>> q;
			std::vector<uint32_t>                  syms;
			q.push_back({0, n - 1});
			syms.push_back(start_id);

			while (!q.empty()) {
				auto [i, j] = q.back();
				uint32_t sym = syms.back();
				q.pop_back();
				syms.pop_back();

				const Entry& entry = table[i][j][sym];
				if (!entry.rule)
					continue;

				if (entry.rule->original_idx != -1) {
					grammar_[entry.rule->original_lhs].base_weights[entry.rule->original_idx] *= 2.0;
					needs_binarization_ = true;
				}

				if (!entry.rule->is_terminal) {
					if (entry.rule->rhs2 != 0) {
						q.push_back({entry.split + 1, j});
						syms.push_back(entry.rule->rhs2);
						q.push_back({i, entry.split});
						syms.push_back(entry.rule->rhs1);
					} else {
						q.push_back({i, j});
						syms.push_back(entry.rule->rhs1);
					}
				}
			}
		}

		return tokens;
	}

	WordInterner& get_interner() { return interner_; }
};

#endif // GRAMMAR_HPP