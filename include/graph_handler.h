#pragma once

#include <memory>
#include <vector>
#include "graph.h"

namespace Boidsish {

class GraphHandler {
public:
    GraphHandler() = default;

    std::shared_ptr<Graph> AddGraph() {
        auto graph = std::make_shared<Graph>(graphs_.size());
        graphs_.push_back(graph);
        return graph;
    }

    const std::vector<std::shared_ptr<Graph>>& GetGraphs() const { return graphs_; }

    std::vector<std::shared_ptr<Shape>> GetShapes() {
        std::vector<std::shared_ptr<Shape>> shapes;
        for (const auto& graph : graphs_) {
            shapes.push_back(graph);
        }
        return shapes;
    }

private:
    std::vector<std::shared_ptr<Graph>> graphs_;
};

} // namespace Boidsish
