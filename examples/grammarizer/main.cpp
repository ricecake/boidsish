#include <iostream>
#include <vector>
#include <string>

#include "grammar.h"

int main() {
    try {
        Grammar npc_speech("assets/dialogue_grammar.txt", "assets/dialogue_terminals.txt");
        WordInterner& interner = npc_speech.get_interner();

        GraphCompiler::load_triples("assets/wordnet_subset.txt", interner);
        std::vector<uint32_t> quest_tags;

        auto x = npc_speech.parse_and_modify("I will give you a big reward if you bring me a cup of tea.");
        for (auto i: x) {
            quest_tags.push_back(i);
        }

        std::vector<float> heatmap(interner.size(), 0.0f);
        GraphCompiler::compute_heatmap(quest_tags, heatmap.data(), heatmap.size(), 8);

        for (size_t i = 0; i < heatmap.size(); ++i) {
            if (heatmap[i] > 0) {
                std::cout << "  " << interner.get_string(i) << ": " << heatmap[i] << "\n";
            }
        }

        GenerationContext normal_ctx;
        normal_ctx.semantic_heatmap = heatmap.data();

        for (int i = 0; i < 3; ++i) {
            std::cout << npc_speech.generate(normal_ctx) << "\n";
        }

        npc_speech.dump_rules("test_rule_modify.txt");

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
