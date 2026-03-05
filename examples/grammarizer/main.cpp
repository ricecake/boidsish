#include <iostream>

#include "grammar.h"

int main() {
	try {
		Grammar plant("plant_grammar.txt");

		// Generate 5 random sentences
		for (int i = 0; i < 5; ++i) {
			std::cout << plant.generate() << "\n";
		}
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
	return 0;
}