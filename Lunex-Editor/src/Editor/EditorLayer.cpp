#include "EditorLayer.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Scene/SceneSerializer.h"

#include "Utils/PlatformUtils.h"

#include "ImGuizmo.h"

#include "Math/Math.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f), m_SquareColor({ 0.2f, 0.3f, 0.8f, 1.0f })
	{
	}

	void EditorLayer::OnAttach() {
		LNX_PROFILE_FUNCTION();

		m_CheckerboardTexture = Texture2D::Create("assets/textures/Checkerboard.png");

		Lunex::FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;

		m_SceneHierarchyPanel.SetContext(m_ActiveScene);

		// Cargar escena inicial si se proporcionó
		if (!m_InitialScenePath.empty()) {
			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(m_InitialScenePath.string());
		}

		// Setup ViewportPanel
		m_ViewportPanel.SetFramebuffer(m_Framebuffer);
		m_ViewportPanel.SetActiveScene(m_ActiveScene);

		ViewportPanelCallbacks viewportCallbacks;
		viewportCallbacks.OnSceneDropped = [this](const std::filesystem::path& path) {
			OpenScene(path);
			};
		viewportCallbacks.GetSelectedEntity = [this]() {
			return m_SceneHierarchyPanel.GetSelectedEntity();
			};
		viewportCallbacks.GetEditorCamera = [this]() -> const EditorCamera& {
			return m_EditorCamera;
			};
		m_ViewportPanel.SetCallbacks(viewportCallbacks);

		// Setup ToolbarPanel
		ToolbarCallbacks toolbarCallbacks;
		toolbarCallbacks.OnPlay = [this]() { OnScenePlay(); };
		toolbarCallbacks.OnSimulate = [this]() { OnSceneSimulate(); };
		toolbarCallbacks.OnStop = [this]() { OnSceneStop(); };
		m_ToolbarPanel.SetCallbacks(toolbarCallbacks);

		Renderer2D::SetLineWidth(4.0f);
	}

	void EditorLayer::OnDetach() {
		LNX_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Lunex::Timestep ts) {
		LNX_PROFILE_FUNCTION();

		// Resize
		glm::vec2 viewportSize = m_ViewportPanel.GetViewportSize();
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			viewportSize.x > 0.0f && viewportSize.y > 0.0f &&
			(spec.Width != viewportSize.x || spec.Height != viewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
			m_CameraController.OnResize(viewportSize.x, viewportSize.y);

			m_EditorCamera.SetViewportSize(viewportSize.x, viewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
		}

		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();

		// Clear entity ID attachment
		m_Framebuffer->ClearAttachment(1, -1);

		switch (m_SceneState) {
		case SceneState::Edit: {
			if (m_ViewportPanel.IsViewportFocused())
				m_CameraController.OnUpdate(ts);

			m_EditorCamera.OnUpdate(ts);
			m_ActiveScene->OnUpdateEditor(ts, m_EditorCamera);
			break;
		}
		case SceneState::Simulate: {
			m_EditorCamera.OnUpdate(ts);
			m_ActiveScene->OnUpdateSimulation(ts, m_EditorCamera);
			break;
		}
		case SceneState::Play: {
			m_ActiveScene->OnUpdateRuntime(ts);
			break;
		}
		}

		// Mouse picking
		auto [mx, my] = ImGui::GetMousePos();
		glm::vec2* viewportBounds = m_ViewportPanel.GetViewportBounds();
		mx -= viewportBounds[0].x;
		my -= viewportBounds[0].y;
		glm::vec2 viewportSizePicking = viewportBounds[1] - viewportBounds[0];
		my = viewportSizePicking.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSizePicking.x && mouseY < (int)viewportSizePicking.y) {
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity((entt::entity)pixelData, m_ActiveScene.get());
		}

		// Update panels
		m_StatsPanel.SetHoveredEntity(m_HoveredEntity);

		OnOverlayRender();

		m_Framebuffer->Unbind();
	}

	void EditorLayer::OnImGuiRender() {
		LNX_PROFILE_FUNCTION();

		static bool dockspaceOpen = true;
		static bool opt_fullscreen_persistant = true;
		bool opt_fullscreen = opt_fullscreen_persistant;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen) {
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->Pos);
			ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = minWinSizeX;

		// Menu Bar
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Save", "Ctrl+S"))
					SaveScene();

				if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
					SaveSceneAs();

				if (ImGui::MenuItem("New", "Ctrl+N"))
					NewScene();

				if (ImGui::MenuItem("Open...", "Ctrl+O"))
					OpenScene();

				if (ImGui::MenuItem("Exit"))
					Application::Get().Close();

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		// Render all panels
		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();
		m_StatsPanel.OnImGuiRender();
		m_SettingsPanel.OnImGuiRender();

		// Viewport panel
		m_ViewportPanel.SetGizmoType(m_GizmoType);
		m_ViewportPanel.OnImGuiRender();

		// Block events if viewport is not focused/hovered
		Application::Get().GetImGuiLayer()->BlockEvents(
			!m_ViewportPanel.IsViewportFocused() && !m_ViewportPanel.IsViewportHovered()
		);

		// Toolbar panel
		m_ToolbarPanel.SetSceneState(m_SceneState);
		m_ToolbarPanel.SetToolbarEnabled((bool)m_ActiveScene);
		m_ToolbarPanel.OnImGuiRender();

		ImGui::End();
	}

	void EditorLayer::OnEvent(Lunex::Event& e) {
		m_CameraController.OnEvent(e);
		if (m_SceneState == SceneState::Edit) {
			m_EditorCamera.OnEvent(e);
		}

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(LNX_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(LNX_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e) {
		if (e.GetRepeatCount() > 0)
			return false;

		bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);

		switch (e.GetKeyCode()) {
		case Key::N: {
			if (control)
				NewScene();
			break;
		}
		case Key::O: {
			if (control)
				OpenScene();
			break;
		}
		case Key::S: {
			if (control) {
				if (shift)
					SaveSceneAs();
				else
					SaveScene();
			}
			break;
		}
		case Key::D: {
			if (control)
				OnDuplicateEntity();
			break;
		}
		case Key::Q: {
			if (!ImGuizmo::IsUsing())
				m_GizmoType = -1;
			break;
		}
		case Key::W: {
			if (!ImGuizmo::IsUsing())
				m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
			break;
		}
		case Key::E: {
			if (!ImGuizmo::IsUsing())
				m_GizmoType = ImGuizmo::OPERATION::ROTATE;
			break;
		}
		case Key::R: {
			if (!ImGuizmo::IsUsing())
				m_GizmoType = ImGuizmo::OPERATION::SCALE;
			break;
		}
		}

		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e) {
		if (e.GetMouseButton() == Mouse::ButtonLeft) {
			if (m_ViewportPanel.IsViewportHovered() && !ImGuizmo::IsOver() && !Input::IsKeyPressed(Key::LeftAlt))
				m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
		}
		return false;
	}

	void EditorLayer::OnOverlayRender() {
		if (m_SceneState == SceneState::Play) {
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (!camera)
				return;
			glm::mat4 viewProjection = camera.GetComponent<CameraComponent>().Camera.GetProjection() *
				glm::inverse(camera.GetComponent<TransformComponent>().GetTransform());
			Renderer2D::BeginScene(viewProjection);
		}
		else {
			Renderer2D::BeginScene(m_EditorCamera.GetViewProjection());
		}

		if (m_SettingsPanel.GetShowPhysicsColliders()) {
			// Box Colliders
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
				for (auto entity : view) {
					auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

					glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawQuad(transform, glm::vec4(0, 1, 0, 1));
				}
			}

			// Circle Colliders
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
				for (auto entity : view) {
					auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);

					glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.01f);
				}
			}
		}

		// Draw selected entity outline
		if (Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity()) {
			const TransformComponent& transform = selectedEntity.GetComponent<TransformComponent>();
			Renderer2D::DrawQuad(transform.GetTransform(), glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
		}

		Renderer2D::EndScene();
	}

	void EditorLayer::NewScene() {
		m_ActiveScene = CreateRef<Scene>();
		glm::vec2 viewportSize = m_ViewportPanel.GetViewportSize();
		m_ActiveScene->OnViewportResize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ViewportPanel.SetActiveScene(m_ActiveScene);
		m_EditorScenePath = std::filesystem::path();
	}

	void EditorLayer::OpenScene() {
		std::string filepath = FileDialogs::OpenFile("Lunex Scene (*.lunex)\0*.lunex\0");
		if (!filepath.empty())
			OpenScene(filepath);
	}

	void EditorLayer::OpenScene(const std::filesystem::path& path) {
		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		if (path.extension().string() != ".lunex") {
			LNX_LOG_WARN("Could not load {0} - not a scene file", path.filename().string());
			return;
		}

		Ref<Scene> newScene = CreateRef<Scene>();
		SceneSerializer serializer(newScene);
		if (serializer.Deserialize(path.string())) {
			m_EditorScene = newScene;
			glm::vec2 viewportSize = m_ViewportPanel.GetViewportSize();
			m_EditorScene->OnViewportResize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_EditorScene);
			m_ActiveScene = m_EditorScene;
			m_ViewportPanel.SetActiveScene(m_ActiveScene);
			m_EditorScenePath = path;
		}
	}

	void EditorLayer::SaveScene() {
		if (!m_EditorScenePath.empty())
			SerializeScene(m_ActiveScene, m_EditorScenePath);
		else
			SaveSceneAs();
	}

	void EditorLayer::SaveSceneAs() {
		std::string filepath = FileDialogs::SaveFile("Lunex Scene (*.lunex)\0*.lunex\0");
		if (!filepath.empty()) {
			SerializeScene(m_ActiveScene, filepath);
			m_EditorScenePath = filepath;
		}
	}

	void EditorLayer::SerializeScene(Ref<Scene> scene, const std::filesystem::path& path) {
		SceneSerializer serializer(scene);
		serializer.Serialize(path.string());
	}

	void EditorLayer::OnScenePlay() {
		if (m_SceneState == SceneState::Simulate)
			OnSceneStop();

		m_SceneState = SceneState::Play;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnRuntimeStart();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ViewportPanel.SetActiveScene(m_ActiveScene);
	}

	void EditorLayer::OnSceneSimulate() {
		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		m_SceneState = SceneState::Simulate;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnSimulationStart();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ViewportPanel.SetActiveScene(m_ActiveScene);
	}

	void EditorLayer::OnSceneStop() {
		LNX_CORE_ASSERT(m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate);

		if (m_SceneState == SceneState::Play)
			m_ActiveScene->OnRuntimeStop();
		else if (m_SceneState == SceneState::Simulate)
			m_ActiveScene->OnSimulationStop();

		m_SceneState = SceneState::Edit;
		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_ViewportPanel.SetActiveScene(m_ActiveScene);
	}

	void EditorLayer::OnDuplicateEntity() {
		if (m_SceneState != SceneState::Edit)
			return;

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity)
			m_EditorScene->DuplicateEntity(selectedEntity);
	}
}