#pragma once

#include "Core/Core.h"
#include "Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Lunex {

	class GridRenderer {
	public:
		static void Init();
		static void Shutdown();

		// Renderiza el grid usando la cámara del editor
		static void DrawGrid(const EditorCamera& camera);

		struct GridSettings {
			glm::vec3 GridColor = { 0.35f, 0.35f, 0.35f };  // Gris sutil
			float GridScale = 1.0f;                         // 1 unidad por celda
			float MinorLineThickness = 0.8f;                // Líneas finas
			float MajorLineThickness = 1.2f;                // Líneas mayores (cada 10)
			float FadeDistance = 20.0f;                     // 20 unidades de extensión
		};

		static GridSettings& GetSettings() { return s_Settings; }

	private:
		static GridSettings s_Settings;
	};

}
