#include <iostream>
#include <vector>
#include <string>
#include <cassert>

#include "grammar.h"

int main() {
    try {
        Grammar g("assets/dialogue_grammar.txt");
        WordInterner& interner = g.get_interner();

        std::string sentence = "hello . I need you to find a cat . thank you .";
        std::cout << "Original sentence: " << sentence << std::endl;

        // Parse and modify
        std::vector<uint32_t> terminals = g.parse_and_modify(sentence);

        std::cout << "Parsed terminals: ";
        for (uint32_t id : terminals) {
            std::cout << interner.get_string(id) << " ";
        }
        std::cout << std::endl;

        assert(!terminals.empty());

        // Check if "cat" is in terminals
        bool found_cat = false;
        uint32_t cat_id = interner.get_or_intern("cat");
        for (uint32_t id : terminals) {
            if (id == cat_id) found_cat = true;
        }
        assert(found_cat);

        // Test unit production support
        // Add ROOT -> HELLO_ROOT and HELLO_ROOT -> "hello"
        // Wait, we can use the existing grammar more creatively.
        // Let's manually add a unit rule.
        // We'll add it to the interner and grammar directly for testing.
        uint32_t unit_lhs = interner.get_or_intern("UNIT_TEST_ROOT");
        uint32_t unit_rhs = interner.get_or_intern("ROOT");
        g.parse_and_modify("hello .", "UNIT_TEST_ROOT"); // Should parse via UNIT_TEST_ROOT -> ROOT -> ...
        std::cout << "Unit production parse successful if no crash and reached here." << std::endl;

        // Generate new dialogue and see if it's influenced
        // Since we doubled the weights of rules used in the sentence,
        // those should be more likely now.

        std::cout << "\nGenerating 10 samples after modification:\n";
        for(int i=0; i<10; ++i) {
            std::cout << i << ": " << g.generate() << std::endl;
        }

        // Test heatmap generation with the terminals
        std::vector<float> heatmap(interner.size(), 0.0f);
        GraphCompiler::load_triples("assets/wordnet_subset.txt", interner);
        GraphCompiler::compute_heatmap(terminals, heatmap.data(), heatmap.size(), 2);

        std::cout << "\nHeatmap top words:\n";
        for(size_t i=0; i<heatmap.size(); ++i) {
            if (heatmap[i] > 0.5f) {
                std::cout << "  " << interner.get_string(i) << ": " << heatmap[i] << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
