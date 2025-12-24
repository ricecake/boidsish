#include <iostream>

#include "entity.h"
#include "graphics.h"
#include "path.h"
#include <imgui.h>
#include "IWidget.h"

using namespace Boidsish;

class PathDemoEntity: public Entity<Dot> {
public:
	PathDemoEntity(int id): Entity(id) {}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) override {
		// Nothing to do here, the path following is handled by the EntityHandler
	}
};

class PathWidget : public UI::IWidget {
public:
    PathWidget(std::shared_ptr<Path> path) : m_path(path) {}

    void Draw() override {
        ImGui::Begin("Path Controls");

        bool visible = m_path->IsVisible();
        if (ImGui::Checkbox("Visible", &visible)) {
            m_path->SetVisible(visible);
        }

        const char* modes[] = {"Once", "Loop", "Reverse"};
        int         current_mode = static_cast<int>(m_path->GetMode());
        if (ImGui::Combo("Mode", &current_mode, modes, IM_ARRAYSIZE(modes))) {
            m_path->SetMode(static_cast<PathMode>(current_mode));
        }

        int waypoint_idx = 0;
        for (auto& waypoint : m_path->GetWaypoints()) {
            ImGui::PushID(waypoint_idx++);
            ImGui::DragFloat3("Position", &waypoint.position.x, 0.1f);
            ImGui::DragFloat3("Up", &waypoint.up.x, 0.1f);
            ImGui::PopID();
        }
        ImGui::End();
    }

private:
    std::shared_ptr<Path> m_path;
};

int main(int argc, char** argv) {
	try {
		Visualizer vis;

		auto path_handler = std::make_shared<PathHandler>();
		auto path = path_handler->AddPath();
		path->SetMode(PathMode::LOOP);
		path->SetVisible(false);

		path->AddWaypoint({-10, 0, 0}, {0, 1, 0}, 10.0f, 1, 0, 0, 1);
		path->AddWaypoint({0, 5, 5}, {0, 0, 1}, 5.0f, 0, 1, 0, 1);
		path->AddWaypoint({10, 0, 0}, {0, 1, 0}, 10.0f, 0, 0, 1, 1);
		path->AddWaypoint({0, -5, -5}, {0, 0, -1}, 5.0f, 1, 1, 0, 1);

		auto entity_handler = std::make_shared<EntityHandler>(vis.GetThreadPool());
		for (int i = 0; i < 10; ++i) {
			auto entity = std::make_shared<PathDemoEntity>(i);
			entity->SetPosition(-10 + i * 2, 0, 0);
			entity->SetPath(path, 5.0f + (float)i / 2.0f);
			entity_handler->AddEntity(i, entity);
		}

		vis.AddShapeHandler([entity_handler](float time) { return (*entity_handler)(time); });

		vis.AddShapeHandler([path_handler](float time) { return path_handler->GetShapes(); });

		vis.AddWidget(std::make_shared<PathWidget>(path));

		vis.SetCameraMode(CameraMode::AUTO);
		vis.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
