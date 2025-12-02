#pragma once

#include "Core/Core.h"
#include "Renderer/PostProcess/SelectionOutlinePass.h"
#include <imgui.h>

namespace Lunex {
	class OutlineSettingsPanel {
	public:
		OutlineSettingsPanel() = default;
		~OutlineSettingsPanel() = default;

		void SetOutlinePass(Ref<SelectionOutlinePass> outlinePass) {
			m_OutlinePass = outlinePass;
		}

		void OnImGuiRender() {
			if (!m_OutlinePass || !m_Show)
				return;

			ImGui::Begin("Outline Settings", &m_Show);

			auto& settings = m_OutlinePass->GetSettings();

			// Color picker
			ImGui::ColorEdit4("Outline Color", &settings.OutlineColor.r, ImGuiColorEditFlags_AlphaBar);

			// Thickness/Blur radius
			ImGui::SliderFloat("Thickness", &settings.Thickness, 1.0f, 10.0f, "%.1f");
			ImGui::SliderFloat("Blur Radius", &settings.BlurRadius, 1.0f, 15.0f, "%.1f");

			// Blend mode
			const char* blendModes[] = { "Alpha Blend", "Additive", "Screen" };
			ImGui::Combo("Blend Mode", &settings.BlendMode, blendModes, 3);

			// Depth test
			ImGui::Checkbox("Depth Test", &settings.DepthTest);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Discard outline if occluded by geometry");
			}
			
			// ? DEBUG MODE
			ImGui::Separator();
			ImGui::Text("Debug Visualization:");
			const char* debugModes[] = { "Final Composite", "Mask Only", "Blur Only", "Edge Only" };
			ImGui::Combo("Debug Mode", &settings.DebugMode, debugModes, 4);
			
			if (settings.DebugMode > 0) {
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "? Debug mode active!");
			}

			// Presets
			ImGui::Separator();
			ImGui::Text("Presets:");
			
			if (ImGui::Button("Default Orange")) {
				settings.OutlineColor = glm::vec4(1.0f, 0.6f, 0.0f, 1.0f);
				settings.Thickness = 3.0f;
				settings.BlurRadius = 5.0f;
				settings.BlendMode = 0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Bright Cyan")) {
				settings.OutlineColor = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);
				settings.Thickness = 4.0f;
				settings.BlurRadius = 6.0f;
				settings.BlendMode = 1; // Additive
			}
			ImGui::SameLine();
			if (ImGui::Button("Soft White")) {
				settings.OutlineColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f);
				settings.Thickness = 2.0f;
				settings.BlurRadius = 8.0f;
				settings.BlendMode = 0;
			}

			ImGui::End();
		}

		void Show() { m_Show = true; }
		void Hide() { m_Show = false; }
		void Toggle() { m_Show = !m_Show; }
		bool IsVisible() const { return m_Show; }

	private:
		Ref<SelectionOutlinePass> m_OutlinePass;
		bool m_Show = false;
	};
}
