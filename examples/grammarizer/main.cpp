#include <iostream>
#include <vector>
#include <string>

#include "grammar.h"

int main() {
    try {
        // 1. Initialize Grammar
        Grammar npc_speech("assets/dialogue_grammar.txt");
        WordInterner& interner = npc_speech.get_interner();

        // 2. Load Semantic Graph
        GraphCompiler::load_triples("assets/wordnet_subset.txt", interner);

        // 3. Define Quest Objectives (tags)
        // Let's say the quest is about a 'cat'
        uint32_t quest_target = interner.get_or_intern("cat");
        std::vector<uint32_t> quest_tags = { quest_target };

        // 4. Generate Semantic Heatmap
        std::vector<float> heatmap(interner.size(), 0.0f);
        GraphCompiler::compute_heatmap(quest_tags, heatmap.data(), heatmap.size(), 3);

        std::cout << "Heatmap for 'cat':\n";
        for (size_t i = 0; i < heatmap.size(); ++i) {
            if (heatmap[i] > 0) {
                std::cout << "  " << interner.get_string(i) << ": " << heatmap[i] << "\n";
            }
        }

        // 5. Generation Contexts
        GenerationContext normal_ctx;
        normal_ctx.semantic_heatmap = heatmap.data();

        GenerationContext frustrated_ctx;
        frustrated_ctx.semantic_heatmap = heatmap.data();
        frustrated_ctx.frustration_level = 3;
        frustrated_ctx.max_depth = 4;

        std::cout << "--- Normal Dialogue (Target: cat) ---\n";
        for (int i = 0; i < 3; ++i) {
            std::cout << npc_speech.generate(normal_ctx) << "\n";
        }

        std::cout << "\n--- Frustrated/Glitched Dialogue (Target: cat) ---\n";
        for (int i = 0; i < 3; ++i) {
            std::cout << npc_speech.generate(frustrated_ctx) << "\n";
        }

        // 6. Test with different quest tag (sword)
        uint32_t sword_target = interner.get_or_intern("sword");
        GraphCompiler::compute_heatmap({sword_target}, heatmap.data(), heatmap.size(), 3);

        std::cout << "\n--- Normal Dialogue (Target: sword) ---\n";
        for (int i = 0; i < 3; ++i) {
            std::cout << npc_speech.generate(normal_ctx) << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
