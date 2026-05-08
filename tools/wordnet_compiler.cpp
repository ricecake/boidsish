#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

/**
 * A robust WordNet JSON parser for the distributed synset-keyed format.
 * This tool parses synset objects to generate semantic triples.
 * Uses C++17 std::filesystem for cross-platform directory iteration.
 */

namespace fs = std::filesystem;

struct Synset {
    std::string id;
    std::vector<std::string> members;
    std::unordered_map<std::string, std::vector<std::string>> relations;
};

class WordNetParser {
public:
    explicit WordNetParser(const std::string& content) : content_(content), pos_(0) {}

    void parse(std::unordered_map<std::string, Synset>& synsets) {
        skip_whitespace();
        if (peek() != '{') return;
        consume('{');

        while (pos_ < content_.size() && peek() != '}') {
            std::string synset_id = parse_string();
            consume(':');
            consume('{');

            Synset synset;
            synset.id = synset_id;

            while (pos_ < content_.size() && peek() != '}') {
                std::string key = parse_string();
                consume(':');

                if (key == "members") {
                    parse_array(synset.members);
                } else if (key == "definition" || key == "ili" || key == "partOfSpeech") {
                    skip_value();
                } else {
                    // Assume it's a relation (e.g., hypernym)
                    parse_array(synset.relations[key]);
                }

                if (peek() == ',') consume(',');
            }
            consume('}');
            if (!synset.id.empty()) synsets[synset.id] = synset;

            if (peek() == ',') consume(',');
        }
        consume('}');
    }

private:
    std::string content_;
    size_t pos_;

    char peek() { skip_whitespace(); return pos_ < content_.size() ? content_[pos_] : '\0'; }
    void consume(char expected) { if (peek() == expected) pos_++; }

    void skip_whitespace() {
        while (pos_ < content_.size() && std::isspace(static_cast<unsigned char>(content_[pos_]))) pos_++;
    }

    std::string parse_string() {
        skip_whitespace();
        if (peek() != '"') return "";
        pos_++;
        size_t start = pos_;
        while (pos_ < content_.size() && content_[pos_] != '"') {
            if (content_[pos_] == '\\') pos_++;
            pos_++;
        }
        std::string res = content_.substr(start, pos_ - start);
        if (pos_ < content_.size()) pos_++;
        return res;
    }

    void parse_array(std::vector<std::string>& vec) {
        skip_whitespace();
        if (peek() != '[') {
            if (peek() == '"') vec.push_back(parse_string());
            else skip_value();
            return;
        }
        consume('[');
        while (pos_ < content_.size() && peek() != ']') {
            vec.push_back(parse_string());
            if (peek() == ',') consume(',');
        }
        consume(']');
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
};

void process_file(const fs::path& filepath, std::unordered_map<std::string, Synset>& synsets) {
    std::ifstream infile(filepath);
    if (!infile.is_open()) return;
    std::stringstream buffer;
    buffer << infile.rdbuf();
    WordNetParser parser(buffer.str());
    parser.parse(synsets);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <input_json_or_dir> <output_triples_txt>\n";
        return 1;
    }

    std::unordered_map<std::string, Synset> synsets;
    fs::path input_path(argv[1]);

    if (fs::is_directory(input_path)) {
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (entry.path().extension() == ".json") {
                process_file(entry.path(), synsets);
            }
        }
    } else {
        process_file(input_path, synsets);
    }

    std::ofstream outfile(argv[2]);
    if (!outfile.is_open()) return 1;

    for (const auto& [sid, synset] : synsets) {
        for (const auto& [rel_type, targets] : synset.relations) {
            for (const auto& target_sid : targets) {
                auto it_t = synsets.find(target_sid);
                if (it_t == synsets.end()) continue;

                for (const auto& s_member : synset.members) {
                    for (const auto& t_member : it_t->second.members) {
                        if (s_member == t_member) continue;

                        std::string p = rel_type;
                        if (p == "hypernym" || p == "instance_hypernym") p = "is_a";
                        else if (p == "mero_part" || p == "mero_member" || p == "mero_substance") p = "part_of";

                        outfile << s_member << " " << p << " " << t_member << "\n";
                    }
                }
            }
        }
    }

    std::cout << "Successfully compiled " << synsets.size() << " synsets to " << argv[2] << "\n";
    return 0;
}
