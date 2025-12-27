#include <memory>
#include <vector>

#include "graphics.h"
#include "procedural_head.h"
#include "IWidget.h"
#include "imgui.h"
#include "UIManager.h"

class HeadWidget : public Boidsish::UI::IWidget {
public:
    HeadWidget(std::shared_ptr<Boidsish::ProceduralHead> head) : head_(head) {}

    void Draw() override {
        ImGui::Begin("Head Controls");

        bool changed = false;
        changed |= ImGui::SliderFloat("Eye Size", &head_->eye_size, 0.5f, 2.0f);
        changed |= ImGui::SliderFloat("Eye Separation", &head_->eye_separation, -1.0f, 1.0f);
        changed |= ImGui::SliderFloat("Chin Size", &head_->chin_size, 0.5f, 2.0f);
        changed |= ImGui::SliderFloat("Nose Size", &head_->nose_size, 0.5f, 2.0f);
        changed |= ImGui::SliderFloat("Nose Length", &head_->nose_length, -1.0f, 1.0f);
        changed |= ImGui::SliderFloat("Cheek Depth", &head_->cheek_depth, -1.0f, 1.0f);
        changed |= ImGui::SliderFloat("Ear Height", &head_->ear_height, -1.0f, 1.0f);
        changed |= ImGui::SliderFloat("Brow Height", &head_->brow_height, -1.0f, 1.0f);
        changed |= ImGui::SliderFloat("Brow Width", &head_->brow_width, 0.5f, 2.0f);

        if (changed) {
            head_->deform_mesh();
        }

        ImGui::End();
    }

private:
    std::shared_ptr<Boidsish::ProceduralHead> head_;
};

int main(int argc, char** argv) {
    Boidsish::Visualizer vis;

    auto head = std::make_shared<Boidsish::ProceduralHead>();

    std::vector<std::shared_ptr<Boidsish::Shape>> shapes;
    shapes.push_back(head);

    vis.AddShapeHandler([&](float time) {
        return shapes;
    });

    vis.AddWidget(std::make_shared<HeadWidget>(head));

    vis.Run();

    return 0;
}
