#include "ui/ProfilerWidget.h"

#include <algorithm>
#include <cinttypes>
#include <vector>

#include "imgui.h"
#include "profiler.h"

namespace Boidsish {
	namespace UI {

		ProfilerWidget::ProfilerWidget() {}

		void ProfilerWidget::Draw() {
			if (!m_show)
				return;

			ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Profiler", &m_show)) {
				float fps = Profiler::GetInstance().GetFPS();
				ImGui::Text("FPS: %.1f (%.2f ms)", fps, fps > 0 ? 1000.0f / fps : 0.0f);

				ImGui::Separator();

#ifdef PROFILING_ENABLED
				auto statsMap = Profiler::GetInstance().GetStats();

				if (ImGui::Button("Clear Stats")) {
					Profiler::GetInstance().ClearStats();
				}

				if (ImGui::BeginTable(
						"ProfilerStats",
						8,
						ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
							ImGuiTableFlags_Sortable
					)) {
					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort);
					ImGui::TableSetupColumn("Count");
					ImGui::TableSetupColumn("Avg (us)");
					ImGui::TableSetupColumn("EMA (us)");
					ImGui::TableSetupColumn("Min (us)");
					ImGui::TableSetupColumn("Max (us)");
					ImGui::TableSetupColumn("Avg Calls/F");
					ImGui::TableSetupColumn("EMA Calls/F");
					ImGui::TableHeadersRow();

					std::vector<std::pair<std::string, ProfileStats>> sortedStats(statsMap.begin(), statsMap.end());

					if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
						if (sort_specs->SpecsCount > 0) {
							std::sort(sortedStats.begin(), sortedStats.end(), [&](const auto& a, const auto& b) {
								for (int n = 0; n < sort_specs->SpecsCount; n++) {
									const ImGuiTableColumnSortSpecs* spec = &sort_specs->Specs[n];
									int                              delta = 0;
									switch (spec->ColumnIndex) {
									case 0:
										delta = a.first.compare(b.first);
										break;
									case 1:
										delta = (a.second.count < b.second.count) ? -1
											: (a.second.count > b.second.count)   ? 1
																				  : 0;
										break;
									case 2:
										delta = (a.second.GetAverageUs() < b.second.GetAverageUs()) ? -1
											: (a.second.GetAverageUs() > b.second.GetAverageUs())   ? 1
																									: 0;
										break;
									case 3:
										delta = (a.second.emaTimeUs < b.second.emaTimeUs) ? -1
											: (a.second.emaTimeUs > b.second.emaTimeUs)   ? 1
																						  : 0;
										break;
									case 4:
										delta = (a.second.minTimeUs < b.second.minTimeUs) ? -1
											: (a.second.minTimeUs > b.second.minTimeUs)   ? 1
																						  : 0;
										break;
									case 5:
										delta = (a.second.maxTimeUs < b.second.maxTimeUs) ? -1
											: (a.second.maxTimeUs > b.second.maxTimeUs)   ? 1
																						  : 0;
										break;
									case 6:
										delta = (a.second.avgCallsPerFrame < b.second.avgCallsPerFrame) ? -1
											: (a.second.avgCallsPerFrame > b.second.avgCallsPerFrame)   ? 1
																										: 0;
										break;
									case 7:
										delta = (a.second.emaCallsPerFrame < b.second.emaCallsPerFrame) ? -1
											: (a.second.emaCallsPerFrame > b.second.emaCallsPerFrame)   ? 1
																										: 0;
										break;
									}
									if (delta != 0) {
										if (spec->SortDirection == ImGuiSortDirection_Ascending)
											return delta < 0;
										return delta > 0;
									}
								}
								return false;
							});
						}
					}

					for (const auto& [name, stats] : sortedStats) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", name.c_str());
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("%" PRIu64, stats.count);
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%.2f", stats.GetAverageUs());
						ImGui::TableSetColumnIndex(3);
						ImGui::Text("%.2f", stats.emaTimeUs);
						ImGui::TableSetColumnIndex(4);
						ImGui::Text("%.2f", stats.minTimeUs);
						ImGui::TableSetColumnIndex(5);
						ImGui::Text("%.2f", stats.maxTimeUs);
						ImGui::TableSetColumnIndex(6);
						ImGui::Text("%.2f", stats.avgCallsPerFrame);
						ImGui::TableSetColumnIndex(7);
						ImGui::Text("%.2f", stats.emaCallsPerFrame);
					}
					ImGui::EndTable();
				}
#else
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Profiling disabled.");
				ImGui::Text("Rebuild with 'make profile' to see detailed stats.");
#endif
			}
			ImGui::End();
		}

		void ProfilerWidget::SetVisible(bool visible) {
			m_show = visible;
		}

		bool ProfilerWidget::IsVisible() const {
			return m_show;
		}
	} // namespace UI
} // namespace Boidsish
