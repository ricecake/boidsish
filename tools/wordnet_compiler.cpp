#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <cctype>

/**
 * A robust WordNet JSON parser for the Open English WordNet format.
 * This tool parses 'entries' and 'synsets' to generate semantic triples.
 * Since we don't have a JSON library, we use a custom state-machine parser.
 */

struct Entry {
    std::string lemma;
    std::vector<std::string> synset_ids;
};

struct Synset {
    std::string id;
    std::unordered_map<std::string, std::vector<std::string>> relations;
};

class SimpleJsonParser {
public:
    explicit SimpleJsonParser(const std::string& content) : content_(content), pos_(0) {}

    void parse(std::unordered_map<std::string, Entry>& entries,
               std::unordered_map<std::string, Synset>& synsets) {
        skip_whitespace();
        if (peek() != '{') return;
        consume('{');

        while (pos_ < content_.size() && peek() != '}') {
            std::string key = parse_string();
            consume(':');
            if (key == "entries") {
                parse_entries(entries);
            } else if (key == "synsets") {
                parse_synsets(synsets);
            } else {
                skip_value();
            }
            if (peek() == ',') consume(',');
        }
    }

private:
    std::string content_;
    size_t pos_;

    char peek() { skip_whitespace(); return pos_ < content_.size() ? content_[pos_] : '\0'; }
    void consume(char expected) { if (peek() == expected) pos_++; }

    void skip_whitespace() {
        while (pos_ < content_.size() && std::isspace(content_[pos_])) pos_++;
    }

    std::string parse_string() {
        skip_whitespace();
        if (peek() != '"') return "";
        pos_++; // consume "
        size_t start = pos_;
        while (pos_ < content_.size() && content_[pos_] != '"') {
            if (content_[pos_] == '\\') pos_++;
            pos_++;
        }
        std::string res = content_.substr(start, pos_ - start);
        if (pos_ < content_.size()) pos_++; // consume "
        return res;
    }

    void skip_value() {
        char c = peek();
        if (c == '"') parse_string();
        else if (c == '{') {
            consume('{');
            int depth = 1;
            while (depth > 0 && pos_ < content_.size()) {
                if (content_[pos_] == '{') depth++;
                else if (content_[pos_] == '}') depth--;
                pos_++;
            }
        } else if (c == '[') {
            consume('[');
            int depth = 1;
            while (depth > 0 && pos_ < content_.size()) {
                if (content_[pos_] == '[') depth++;
                else if (content_[pos_] == ']') depth--;
                pos_++;
            }
        } else {
            while (pos_ < content_.size() && content_[pos_] != ',' && content_[pos_] != '}' && content_[pos_] != ']') pos_++;
        }
    }

    void parse_entries(std::unordered_map<std::string, Entry>& entries) {
        consume('[');
        while (peek() != ']') {
            consume('{');
            Entry entry;
            std::string entry_id;
            while (peek() != '}') {
                std::string key = parse_string();
                consume(':');
                if (key == "lemma") entry.lemma = parse_string();
                else if (key == "senses") {
                    consume('[');
                    while (peek() != ']') {
                        consume('{');
                        while (peek() != '}') {
                            std::string s_key = parse_string();
                            consume(':');
                            if (s_key == "synset") entry.synset_ids.push_back(parse_string());
                            else skip_value();
                            if (peek() == ',') consume(',');
                        }
                        consume('}');
                        if (peek() == ',') consume(',');
                    }
                    consume(']');
                } else skip_value();
                if (peek() == ',') consume(',');
            }
            if (!entry.lemma.empty()) entries[entry.lemma] = entry;
            consume('}');
            if (peek() == ',') consume(',');
        }
        consume(']');
    }

    void parse_synsets(std::unordered_map<std::string, Synset>& synsets) {
        consume('[');
        while (peek() != ']') {
            consume('{');
            Synset synset;
            while (peek() != '}') {
                std::string key = parse_string();
                consume(':');
                if (key == "id") synset.id = parse_string();
                else if (key == "relations") {
                    consume('[');
                    while (peek() != ']') {
                        consume('{');
                        std::string rel_type, target;
                        while (peek() != '}') {
                            std::string r_key = parse_string();
                            consume(':');
                            if (r_key == "relType") rel_type = parse_string();
                            else if (r_key == "target") target = parse_string();
                            else skip_value();
                            if (peek() == ',') consume(',');
                        }
                        if (!rel_type.empty() && !target.empty()) synset.relations[rel_type].push_back(target);
                        consume('}');
                        if (peek() == ',') consume(',');
                    }
                    consume(']');
                } else skip_value();
                if (peek() == ',') consume(',');
            }
            if (!synset.id.empty()) synsets[synset.id] = synset;
            consume('}');
            if (peek() == ',') consume(',');
        }
        consume(']');
    }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <input_wordnet_json> <output_triples_txt>\n";
        return 1;
    }

    std::ifstream infile(argv[1]);
    if (!infile.is_open()) {
        std::cerr << "Error opening input file: " << argv[1] << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << infile.rdbuf();
    std::string content = buffer.str();

    std::unordered_map<std::string, Entry> entries;
    std::unordered_map<std::string, Synset> synsets;

    SimpleJsonParser parser(content);
    parser.parse(entries, synsets);

    std::ofstream outfile(argv[2]);
    if (!outfile.is_open()) {
        std::cerr << "Error opening output file: " << argv[2] << "\n";
        return 1;
    }

    // Step 1: Mapping synsets to lemmas (reverse map)
    std::unordered_map<std::string, std::vector<std::string>> synset_to_lemmas;
    for (const auto& [lemma, entry] : entries) {
        for (const auto& sid : entry.synset_ids) {
            synset_to_lemmas[sid].push_back(lemma);
        }
    }

    // Step 2: Generate Triples
    for (const auto& [sid, synset] : synsets) {
        auto it_s = synset_to_lemmas.find(sid);
        if (it_s == synset_to_lemmas.end()) continue;

        for (const auto& [rel_type, targets] : synset.relations) {
            for (const auto& target_sid : targets) {
                auto it_t = synset_to_lemmas.find(target_sid);
                if (it_t == synset_to_lemmas.end()) continue;

                // For each lemma in source synset and each lemma in target synset, create a triple
                for (const auto& s_lemma : it_s->second) {
                    for (const auto& t_lemma : it_t->second) {
                        if (s_lemma == t_lemma) continue;

                        // Map internal relation types to our simple types
                        std::string p = rel_type;
                        if (p == "hypernym" || p == "instance_hypernym") p = "is_a";
                        else if (p == "mero_part" || p == "mero_member" || p == "mero_substance") p = "part_of";

                        outfile << s_lemma << " " << p << " " << t_lemma << "\n";
                    }
                }
            }
        }
    }

    std::cout << "Successfully compiled " << entries.size() << " entries and " << synsets.size() << " synsets to " << argv[2] << "\n";
    return 0;
}
