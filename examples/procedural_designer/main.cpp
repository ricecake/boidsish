#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <fstream>
#include <iostream>

#include "ConfigManager.h"
#include "IWidget.h"
#include "graphics.h"
#include "imgui.h"
#include "logger.h"
#include "procedural_generator.h"

using namespace Boidsish;

class DesignerWidget: public UI::IWidget {
public:
	DesignerWidget(Visualizer& vis): m_vis(vis) {
		strncpy(m_axiomBuf, "F", sizeof(m_axiomBuf));
		strncpy(m_rulesBuf, "F=FF-[+F+F]", sizeof(m_rulesBuf));
		m_type = 0; // Flower
		m_gridSize = 3;
		m_spacing = 5.0f;
		m_iterations = 3;
		m_seedOffset = 0;

		Generate(); // Initial generation
	}

	void Draw() override {
		if (ImGui::Begin("Procedural Designer")) {
			const char* types[] = {"Rock", "Grass", "Flower", "Tree", "SC Tree", "Critter"};
			ImGui::Combo("Model Type", &m_type, types, IM_ARRAYSIZE(types));

			ImGui::InputText("Axiom", m_axiomBuf, sizeof(m_axiomBuf));

			ImGui::InputTextMultiline(
				"Rules (one per line)",
				m_rulesBuf,
				sizeof(m_rulesBuf),
				ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5)
			);

			ImGui::SliderInt("Iterations", &m_iterations, 1, 6);
			ImGui::SliderInt("Grid Size", &m_gridSize, 1, 10);
			ImGui::SliderFloat("Spacing", &m_spacing, 1.0f, 20.0f);
			ImGui::InputInt("Seed Offset", &m_seedOffset);

			if (ImGui::Button("Generate")) {
				m_seedOffset += m_gridSize * m_gridSize;
				Generate();
			}

			ImGui::SameLine();
			if (ImGui::Button("Save Grid to OBJ")) {
				ExportGridToObj(m_lastModelData, "exported_grid.obj");
			}

			ImGui::Separator();
			ImGui::Text("L-System Symbol Guide:");
			ImGui::BulletText("'F': Move forward and draw tube");
			ImGui::BulletText("'+'/'-': Pitch up/down");
			ImGui::BulletText("'&'/'^': Roll left/right");
			ImGui::BulletText("'\\'/'/': Yaw left/right");
			ImGui::BulletText("'['/']': Push/Pop turtle state");
			ImGui::BulletText("'L': Add leaf (uses current variant)");
			ImGui::BulletText("'P': Add puffball (round)");
			ImGui::BulletText("'B': Add button (squashed sphere)");
			ImGui::BulletText("'\'': Cycle color from palette");
			ImGui::BulletText("'!': Decrease thickness");
			ImGui::BulletText("'0'-'9': Set shape variant");
		}
		ImGui::End();
	}

private:
	void ExportGridToObj(const std::vector<std::shared_ptr<ModelData>>& gridData, const std::string& filename) {
		if (gridData.empty())
			return;

		std::ofstream file(filename);
		if (!file.is_open()) {
			logger::ERROR("Failed to open file for export: {}", filename);
			return;
		}

		file << "# Exported from Boidsish Procedural Designer\n";
		int vertex_offset = 1;

		for (size_t g = 0; g < gridData.size(); ++g) {
			const auto& data = gridData[g];
			if (!data)
				continue;

			for (size_t m = 0; m < data->meshes.size(); ++m) {
				const auto& mesh = data->meshes[m];
				file << "o Model_" << g << "_Mesh_" << m << "\n";

				for (const auto& v : mesh.vertices) {
					// We should probably apply the model's grid position here if we want the OBJ to match the visualizer layout
					// But usually users want the model at origin. Let's export at origin for now.
					file << "v " << v.Position.x << " " << v.Position.y << " " << v.Position.z << "\n";
				}
				for (const auto& v : mesh.vertices) {
					file << "vn " << v.Normal.x << " " << v.Normal.y << " " << v.Normal.z << "\n";
				}
				for (const auto& v : mesh.vertices) {
					file << "vt " << v.TexCoords.x << " " << v.TexCoords.y << "\n";
				}

				for (size_t i = 0; i < mesh.indices.size(); i += 3) {
					file << "f " << vertex_offset + mesh.indices[i] << "/" << vertex_offset + mesh.indices[i] << "/"
						 << vertex_offset + mesh.indices[i] << " " << vertex_offset + mesh.indices[i + 1] << "/"
						 << vertex_offset + mesh.indices[i + 1] << "/" << vertex_offset + mesh.indices[i + 1] << " "
						 << vertex_offset + mesh.indices[i + 2] << "/" << vertex_offset + mesh.indices[i + 2] << "/"
						 << vertex_offset + mesh.indices[i + 2] << "\n";
				}
				vertex_offset += (int)mesh.vertices.size();
			}
		}

		file.close();
		logger::LOG("Model exported to {}", filename);
	}

	void Generate() {
		m_vis.ClearShapes();
		m_lastModelData.clear();

		std::string              axiom = m_axiomBuf;
		std::string              rulesStr = m_rulesBuf;
		std::vector<std::string> ruleList;
		std::stringstream        ss(rulesStr);
		std::string              line;
		while (std::getline(ss, line)) {
			if (!line.empty()) {
				ruleList.push_back(line);
			}
		}

		for (int i = 0; i < m_gridSize; ++i) {
			for (int j = 0; j < m_gridSize; ++j) {
				unsigned int           seed = i * m_gridSize + j + 12345 + m_seedOffset;
				std::shared_ptr<Model> model;

				switch (m_type) {
				case 0: model = ProceduralGenerator::GenerateRock(seed); break;
				case 1: model = ProceduralGenerator::GenerateGrass(seed); break;
				case 2: model = ProceduralGenerator::GenerateFlower(seed, axiom, ruleList, m_iterations); break;
				case 3: model = ProceduralGenerator::GenerateTree(seed, axiom, ruleList, m_iterations); break;
				case 4: model = ProceduralGenerator::GenerateSpaceColonizationTree(seed); break;
				case 5: model = ProceduralGenerator::GenerateCritter(seed, axiom, ruleList, m_iterations); break;
				}

				if (model) {
					float x = (i - (m_gridSize - 1) * 0.5f) * m_spacing;
					float z = (j - (m_gridSize - 1) * 0.5f) * m_spacing;
					model->SetPosition(x, 0.0f, z);
					m_vis.AddShape(model);

					// Use internal access to get ModelData (since we are in same namespace or can use friend)
					// Actually, DesignerWidget is in Boidsish namespace if we wrap it, but main.cpp might not be.
					// Let's use a public accessor or just reach in if possible.
					// Model::getMeshes() exists but doesn't give the whole ModelData.
					// We'll have to add a way to get it or store it.
					m_lastModelData.push_back(model->GetData());
				}
			}
		}
	}

	Visualizer&                             m_vis;
	std::vector<std::shared_ptr<ModelData>> m_lastModelData;
	char                                    m_axiomBuf[256];
	char                                    m_rulesBuf[2048];
	int                                     m_type;
	int                                     m_gridSize;
	float                                   m_spacing;
	int                                     m_iterations;
	int                                     m_seedOffset;
};

int main(int argc, char** argv) {
	Visualizer vis(1280, 960, "Procedural Decor Designer");

	auto& config = ConfigManager::GetInstance();
	config.SetBool("render_skybox", false);
	config.SetBool("render_terrain", false);
	config.SetBool("render_decor", false);
	config.SetBool("day_night_cycle", false);
	config.SetBool("enable_floor", true);

	auto designer = std::make_shared<DesignerWidget>(vis);
	vis.AddWidget(designer);

	// Set a reasonable initial camera position
	Camera cam = vis.GetCamera();
	cam.x = 0;
	cam.y = 10;
	cam.z = 20;
	cam.pitch = -20;
	cam.yaw = 0;
	vis.SetCamera(cam);

	vis.Run();

	return 0;
}
