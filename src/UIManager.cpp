#include "UIManager.h"

#include "IWidget.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace Boidsish {
	namespace UI {
		UIManager::UIManager(GLFWwindow* window) {
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			(void)io;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			// io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

			ImGui::LoadIniSettingsFromDisk("imgui.ini");

			ImGui::StyleColorsDark();

			ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 130");
		}

		UIManager::~UIManager() {
			ImGui::SaveIniSettingsToDisk("imgui.ini");
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}

		void UIManager::AddWidget(std::shared_ptr<IWidget> widget) {
			m_widgets.push_back(widget);
		}

		void UIManager::Render() {
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			for (const auto& widget : m_widgets) {
				widget->Draw();
			}

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}
	} // namespace UI
} // namespace Boidsish
