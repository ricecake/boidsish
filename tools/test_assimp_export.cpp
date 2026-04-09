#include <assimp/Exporter.hpp>
#include <iostream>

int main() {
    Assimp::Exporter exporter;
    size_t count = exporter.GetExportFormatCount();
    for (size_t i = 0; i < count; ++i) {
        const aiExportFormatDesc* desc = exporter.GetExportFormatDescription(i);
        std::cout << "Format " << i << ": " << desc->id << " (" << desc->description << ") ." << desc->fileExtension << std::endl;
    }
    return 0;
}
