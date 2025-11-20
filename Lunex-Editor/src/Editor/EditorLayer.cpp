#include "EditorLayer.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
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
		// Inicializar m_ViewportBounds para evitar C26495
		m_ViewportBounds[0] = { 0.0f, 0.0f };
		m_ViewportBounds[1] = { 0.0f, 0.0f };
	}
	
	void EditorLayer::OnAttach() {
		LNX_PROFILE_FUNCTION();
		
		m_CheckerboardTexture = Texture2D::Create("assets/textures/Checkerboard.png");
		
		// Cargar iconos de toolbar con logs de depuración
		LNX_LOG_INFO("Loading toolbar icons...");
		
		m_IconPlay = Texture2D::Create("Resources/Icons/PlayStopButtons/PlayButtonIcon.png");
		if (!m_IconPlay) {
			LNX_LOG_WARN("Failed to load PlayButtonIcon.png - using fallback");
		} else {
			LNX_LOG_INFO("✓ PlayButtonIcon loaded successfully");
		}
		
		m_IconSimulate = Texture2D::Create("Resources/Icons/PlayStopButtons/SimulateButtonIcon.png");
		if (!m_IconSimulate) {
			LNX_LOG_WARN("Failed to load SimulateButtonIcon.png - using fallback");
		} else {
			LNX_LOG_INFO("✓ SimulateButtonIcon loaded successfully");
		}
		
		m_IconStop = Texture2D::Create("Resources/Icons/PlayStopButtons/StopButtonIcon.png");
		if (!m_IconStop) {
			LNX_LOG_WARN("Failed to load StopButtonIcon.png - using fallback");
		} else {
			LNX_LOG_INFO("✓ StopButtonIcon loaded successfully");
		}
		
		// Configure toolbar panel
		m_ToolbarPanel.SetPlayIcon(m_IconPlay);
		m_ToolbarPanel.SetSimulateIcon(m_IconSimulate);
		m_ToolbarPanel.SetStopIcon(m_IconStop);
		m_ToolbarPanel.SetOnPlayCallback([this]() { OnScenePlay(); });
		m_ToolbarPanel.SetOnSimulateCallback([this]() { OnSceneSimulate(); });
		m_ToolbarPanel.SetOnStopCallback([this]() { OnSceneStop(); });
		
		LNX_LOG_INFO("Toolbar configured with icons: Play={0}, Simulate={1}, Stop={2}",
			m_IconPlay ? "OK" : "NULL", 
			m_IconSimulate ? "OK" : "NULL", 
			m_IconStop ? "OK" : "NULL");
		
		// Configure viewport panel
		m_ViewportPanel.SetOnSceneDropCallback([this](const std::filesystem::path& path) {
			OpenScene(path);
		});
		
		Lunex::FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);
		
		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
		
		// Cargar escena inicial si se proporcionó en los argumentos
		if (!m_InitialScenePath.empty()) {
			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(m_InitialScenePath);
		}
		
		Renderer2D::SetLineWidth(4.0f);
		
		// Register custom console commands
		m_ConsolePanel.RegisterCommand("load_scene", "Load a scene file", "load_scene <path>",
			[this](const std::vector<std::string>& args) {
				if (args.empty()) {
					m_ConsolePanel.AddLog("Usage: load_scene <path>", LogLevel::Warning, "Scene");
					return;
				}
				OpenScene(args[0]);
				m_ConsolePanel.AddLog("Scene loaded: " + args[0], LogLevel::Info, "Scene");
			});

		m_ConsolePanel.RegisterCommand("save_scene", "Save the current scene", "save_scene [path]",
			[this](const std::vector<std::string>& args) {
				if (args.empty()) {
					SaveScene();
				} else {
					SerializeScene(m_ActiveScene, args[0]);
				}
				m_ConsolePanel.AddLog("Scene saved successfully", LogLevel::Info, "Scene");
			});

		m_ConsolePanel.RegisterCommand("new_scene", "Create a new empty scene", "new_scene",
			[this](const std::vector<std::string>& args) {
				NewScene();
				m_ConsolePanel.AddLog("New scene created", LogLevel::Info, "Scene");
			});

		m_ConsolePanel.RegisterCommand("play", "Start playing the scene", "play",
			[this](const std::vector<std::string>& args) {
				OnScenePlay();
				m_ConsolePanel.AddLog("Playing scene...", LogLevel::Info, "Runtime");
			});

		m_ConsolePanel.RegisterCommand("stop", "Stop playing the scene", "stop",
			[this](const std::vector<std::string>& args) {
				OnSceneStop();
				m_ConsolePanel.AddLog("Scene stopped", LogLevel::Info, "Runtime");
			});

		m_ConsolePanel.RegisterCommand("list_entities", "List all entities in the scene", "list_entities",
			[this](const std::vector<std::string>& args) {
				m_ConsolePanel.AddLog("Entities in scene:", LogLevel::Info, "Scene");
				auto view = m_ActiveScene->GetAllEntitiesWith<TagComponent>();
				int count = 0;
				for (auto entity : view) {
					Entity e = { entity, m_ActiveScene.get() };
					auto& tag = e.GetComponent<TagComponent>().Tag;
					m_ConsolePanel.AddLog("  - " + tag, LogLevel::Info, "Scene");
					count++;
				}
				m_ConsolePanel.AddLog("Total: " + std::to_string(count) + " entities", LogLevel::Info, "Scene");
			});

		m_ConsolePanel.RegisterCommand("fps", "Show current FPS", "fps",
			[this](const std::vector<std::string>& args) {
				auto stats = Renderer2D::GetStats();
				m_ConsolePanel.AddLog("FPS: " + std::to_string(1.0f / ImGui::GetIO().DeltaTime), LogLevel::Info, "Performance");
				m_ConsolePanel.AddLog("Draw Calls: " + std::to_string(stats.DrawCalls), LogLevel::Info, "Performance");
			});

		m_ConsolePanel.AddLog("Lunex Editor initialized", LogLevel::Info, "System");
		m_ConsolePanel.AddLog("Welcome! Type 'help' to see available commands", LogLevel::Info, "System");
	}
	
	void EditorLayer::OnDetach() {
		LNX_PROFILE_FUNCTION();
	}
	
	void EditorLayer::OnUpdate(Lunex::Timestep ts) {
		LNX_PROFILE_FUNCTION();
		
		// Get viewport size from ViewportPanel
		m_ViewportSize = m_ViewportPanel.GetViewportSize();
		const glm::vec2* viewportBounds = m_ViewportPanel.GetViewportBounds();
		m_ViewportBounds[0] = viewportBounds[0];
		m_ViewportBounds[1] = viewportBounds[1];
		
		// Resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f && // zero sized framebuffer is invalid
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y)) 
		{
				m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
				m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);
				
				m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
				m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}
		
		// Render
		Renderer2D::ResetStats();
		m_Framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		RenderCommand::Clear();
		
		// Clear our entity ID attachment to -1
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
		
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;
		
		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y) {
			// Cambiado a -1 para mayor coherencia, antes era 0
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity((entt::entity)pixelData, m_ActiveScene.get());
		}
		
		// Update stats panel with hovered entity
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
				
				if (ImGui::MenuItem("Exit")) Application::Get().Close();
				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}
		
		// Render all panels
		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();
		m_StatsPanel.OnImGuiRender();
		m_SettingsPanel.OnImGuiRender();
		m_ConsolePanel.OnImGuiRender();
		
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		m_ViewportPanel.OnImGuiRender(m_Framebuffer, m_SceneHierarchyPanel, m_EditorCamera, 
									  selectedEntity, m_GizmoType, m_ToolbarPanel, 
									  m_SceneState, (bool)m_ActiveScene);
		
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
			Renderer2D::BeginScene(camera.GetComponent<CameraComponent>().Camera, camera.GetComponent<TransformComponent>().GetTransform());
		}
		else {
			Renderer2D::BeginScene(m_EditorCamera);
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
					
					Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
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
			Renderer2D::DrawRect(transform.GetTransform(), glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
		}
		Renderer2D::EndScene();
	}
	
	void EditorLayer::NewScene() {
		m_ActiveScene = CreateRef<Scene>();
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
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
			m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_SceneHierarchyPanel.SetContext(m_EditorScene);
			m_ActiveScene = m_EditorScene;
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
		std::string filepath = FileDialogs::SaveFile("Luenx Scene (*.lunex)\0*.lunex\0");
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
	}
	
	void EditorLayer::OnSceneSimulate() {
		if (m_SceneState == SceneState::Play)
			OnSceneStop();
		
		m_SceneState = SceneState::Simulate;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnSimulationStart();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
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
	}
	
	void EditorLayer::OnDuplicateEntity() {
		if (m_SceneState != SceneState::Edit)
			return;
		
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity)
			m_EditorScene->DuplicateEntity(selectedEntity);
	}
}