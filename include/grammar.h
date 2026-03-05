#ifndef GRAMMAR_HPP
#define GRAMMAR_HPP

#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class Grammar {
private:
	struct RuleSet {
		std::vector<double>                   weights;
		std::vector<std::vector<std::string>> productions;
		std::discrete_distribution<size_t>    distribution;
	};

	std::unordered_map<std::string, RuleSet> grammar_;
	std::mt19937                             rng_;

	void generate_recursive(const std::string& symbol, std::vector<std::string>& sentence) {
		auto it = grammar_.find(symbol);
		if (it == grammar_.end()) {
			// Terminal symbol: not found in keys, so output it directly
			sentence.push_back(symbol);
			return;
		}

		// Non-terminal: pick a production based on parsed weights
		size_t      idx = it->second.distribution(rng_);
		const auto& production = it->second.productions[idx];

		for (const auto& s : production) {
			generate_recursive(s, sentence);
		}
	}

public:
	explicit Grammar(const std::string& filename) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			throw std::runtime_error("Could not open grammar file: " + filename);
		}

		// Seed the random number generator
		std::random_device rd;
		rng_ = std::mt19937(rd());

		std::string line;
		while (std::getline(file, line)) {
			// Ignore empty lines and full-line comments
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream       iss(line);
			std::vector<std::string> tokens;
			std::string              token;

			while (iss >> token) {
				// Strip inline comments
				size_t comment_pos = token.find('#');
				if (comment_pos != std::string::npos) {
					if (comment_pos > 0) {
						tokens.push_back(token.substr(0, comment_pos));
					}
					break; // Ignore the rest of the line
				}
				tokens.push_back(token);
			}

			// A valid rule needs at least a weight and a LHS symbol
			if (tokens.size() < 2)
				continue;

			double                   weight = std::stod(tokens[0]);
			std::string              lhs = tokens[1];
			std::vector<std::string> rhs(tokens.begin() + 2, tokens.end());

			grammar_[lhs].weights.push_back(weight);
			grammar_[lhs].productions.push_back(rhs);
		}

		// Initialize distributions once all rules are loaded
		for (auto& [lhs, ruleset] : grammar_) {
			ruleset.distribution = std::discrete_distribution<size_t>(ruleset.weights.begin(), ruleset.weights.end());
		}
	}

	std::string generate(const std::string& start = "ROOT") {
		std::vector<std::string> sentence;
		generate_recursive(start, sentence);

		// Join the tokens with spaces
		std::string text;
		for (size_t i = 0; i < sentence.size(); ++i) {
			text += sentence[i];
			if (i < sentence.size() - 1)
				text += " ";
		}

		// Post-processing regex cleanup to match Python functionality
		text = std::regex_replace(text, std::regex(",(\\s*,)+"), ",");
		text = std::regex_replace(text, std::regex("[,]+"), ",");
		text = std::regex_replace(text, std::regex("([\\[({])\\s+"), "$1");
		text = std::regex_replace(text, std::regex("\\s+([\\])},])"), "$1");
		text = std::regex_replace(text, std::regex("([\\w\\d])\\s+([^\\w\\d])"), "$1$2");
		text = std::regex_replace(text, std::regex("[ ]+"), " ");

		return text;
	}
};

#endif // GRAMMAR_HPP