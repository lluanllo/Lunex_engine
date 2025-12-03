#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Lunex {

	class GridRenderer {
	public:
		static void Init();
		static void Shutdown();

		// Renderiza el infinite grid usando la cámara del editor
		static void DrawGrid(const EditorCamera& camera);

		struct GridSettings {
			glm::vec3 XAxisColor = { 0.8f, 0.3f, 0.3f };  // Rojo suave
			glm::vec3 ZAxisColor = { 0.3f, 0.3f, 0.8f };  // Azul suave
			glm::vec3 GridColor = { 0.5f, 0.5f, 0.5f };   // Gris para líneas normales
			float GridScale = 1.0f;                        // 1 unidad = 1 metro
			float MinorLineThickness = 0.01f;
			float MajorLineThickness = 0.02f;
			float FadeDistance = 100.0f;
		};

		static GridSettings& GetSettings() { return s_Settings; }

	private:
		static GridSettings s_Settings;
	};

}
