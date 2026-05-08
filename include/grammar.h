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
std::vector<NodeData> graph_nodes; // Index matches the word's integer ID
std::vector<Edge>     graph_edges; // All edges packed linearly

// Context passed down the recursion tree
struct GenerationContext {
	int depth = 0;
	int max_depth = 8;         // Fallback to terminals if exceeded
	int frustration_level = 0; // 0 = normal, higher = shorter/ruder

	// Future: pointer to current quest's active WordNet modifiers
	// const std::unordered_map<std::string, double>* semantic_weights = nullptr;
};

class Grammar {
private:
	struct RuleSet {
		std::vector<double>                   base_weights;
		std::vector<std::vector<std::string>> productions;
		// Removed std::discrete_distribution
	};

	std::unordered_map<std::string, RuleSet> grammar_;
	std::mt19937                             rng_;

	void generate_recursive(const std::string& symbol, GenerationContext& ctx, std::string& output) {
		auto it = grammar_.find(symbol);
		if (it == grammar_.end()) {
			// Terminal symbol
			if (!output.empty() && output.back() != ' ') {
				output += ' ';
			}
			output += symbol;
			return;
		}

		// Calculate dynamic weights
		double              total_weight = 0.0;
		std::vector<double> current_weights;
		current_weights.reserve(it->second.base_weights.size());

		for (size_t i = 0; i < it->second.base_weights.size(); ++i) {
			double weight = it->second.base_weights[i];

			// Dynamic Adjustments Example:
			// If depth is high, penalize rules that expand into multiple non-terminals.
			// If frustration is high, heavily boost rules tagged for brevity/hostility.
			if (ctx.depth >= ctx.max_depth - ctx.frustration_level) {
				// Heuristic: shorter productions are usually terminals
				if (it->second.productions[i].size() > 1) {
					weight *= 0.1; // Aggressive decay
				}
			}

			current_weights.push_back(weight);
			total_weight += weight;
		}

		// Inline weighted random selection (avoids allocation overhead)
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
		for (const auto& s : production) {
			generate_recursive(s, ctx, output);
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
		// [Initialization code remains largely the same, loading into base_weights]
		std::ifstream file(filename);
		if (!file.is_open())
			throw std::runtime_error("Could not open grammar");
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

			double                   weight = std::stod(tokens[0]);
			std::string              lhs = tokens[1];
			std::vector<std::string> rhs(tokens.begin() + 2, tokens.end());

			grammar_[lhs].base_weights.push_back(weight);
			grammar_[lhs].productions.push_back(rhs);
		}
	}

	std::string generate(GenerationContext ctx, const std::string& start = "ROOT") {
		std::string output;
		output.reserve(256); // Pre-allocate sensible buffer to minimize reallocations

		generate_recursive(start, ctx, output);
		apply_fast_punctuation_formatting(output);

		return output;
	}

	std::string generate(const std::string& start = "ROOT") { return generate(GenerationContext(), start); }
};

#endif // GRAMMAR_HPP