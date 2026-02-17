#include "logger.h"
#include <iostream>

int main() {
    logger::INFO("Testing interpolation: {} and {}", "first", 42, "tag");
    logger::INFO("Testing no interpolation", "tag1", "tag2");
    logger::INFO("More {} than args", "one");
    logger::INFO("More args than {}", "one", "two", "three");
    logger::INFO("Nested {}: {}", "{}", "value");
    return 0;
}
