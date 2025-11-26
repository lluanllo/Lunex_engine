#include "EditorLayer.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Scene/SceneSerializer.h"
#include "Project/Project.h"
#include "Project/ProjectManager.h"
#include "Project/ProjectSerializer.h"

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
		
		// Cargar iconos de toolbar con logs de depuración
		LNX_LOG_INFO("Loading toolbar icons...");

		m_IconPlay = Texture2D::Create("Resources/Icons/PlayStopButtons/PlayButtonIcon.png");
		if (!m_IconPlay) {
			LNX_LOG_WARN("Failed to load PlayButtonIcon.png - using fallback");
		}
		else {
			LNX_LOG_INFO("✓ PlayButtonIcon loaded successfully");
		}

		m_IconSimulate = Texture2D::Create("Resources/Icons/PlayStopButtons/SimulateButtonIcon.png");
		if (!m_IconSimulate) {
			LNX_LOG_WARN("Failed to load SimulateButtonIcon.png - using fallback");
		}
		else {
			LNX_LOG_INFO("✓ SimulateButtonIcon loaded successfully");
		}

		m_IconStop = Texture2D::Create("Resources/Icons/PlayStopButtons/StopButtonIcon.png");
		if (!m_IconStop) {
			LNX_LOG_WARN("Failed to load StopButtonIcon.png - using fallback");
		}
		else {
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

		// Configure menu bar panel
		m_MenuBarPanel.SetOnNewProjectCallback([this]() { NewProject(); });
		m_MenuBarPanel.SetOnOpenProjectCallback([this]() { OpenProject(); });
		m_MenuBarPanel.SetOnSaveProjectCallback([this]() { SaveProject(); });
		m_MenuBarPanel.SetOnSaveProjectAsCallback([this]() { SaveProjectAs(); });
		
		m_MenuBarPanel.SetOnNewSceneCallback([this]() { NewScene(); });
		m_MenuBarPanel.SetOnOpenSceneCallback([this]() { OpenScene(); });
		m_MenuBarPanel.SetOnSaveSceneCallback([this]() { SaveScene(); });
		m_MenuBarPanel.SetOnSaveSceneAsCallback([this]() { SaveSceneAs(); });
		
		m_MenuBarPanel.SetOnExitCallback([this]() { Application::Get().Close(); });

		// Configure project creation dialog
		m_ProjectCreationDialog.SetOnCreateCallback([this](const std::string& name, const std::filesystem::path& location) {
			CreateProjectWithDialog(name, location);
		});

		Lunex::FramebufferSpecification fbSpec;
		fbSpec.Attachments = { 
			FramebufferTextureFormat::RGBA8,       // Color output (location 0)
			FramebufferTextureFormat::RED_INTEGER, // Entity ID (location 1)
			FramebufferTextureFormat::RGBA16F,     // G-Buffer Position (location 2)
			FramebufferTextureFormat::RGBA16F,     // G-Buffer Normal (location 3)
			FramebufferTextureFormat::Depth        // Depth
		};
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		// ===== INITIALIZE RAY TRACING MANAGER =====
		m_RayTracingManager.Init(1280, 720);
		
		// Configure ray tracing settings
		RayTracingManager::Settings rtSettings;
		rtSettings.enabled = false; // Disabled by default
		rtSettings.shadowBias = 0.001f;
		rtSettings.shadowSoftness = 1.0f;
		rtSettings.samplesPerLight = 4;
		rtSettings.enableDenoiser = true;
		rtSettings.denoiseRadius = 2.0f;
		m_RayTracingManager.SetSettings(rtSettings);
		
		LNX_LOG_INFO("✓ Ray Tracing Manager initialized");

		// Create initial project
		ProjectManager::New();

		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;

		// Cargar escena inicial si se proporcionó en los argumentos
		if (!m_InitialScenePath.empty()) {
			SceneSerializer serializer(m_ActiveScene);
			serializer.Deserialize(m_InitialScenePath);
		}

		Renderer2D::SetLineWidth(4.0f);

		// Configure stats panel with ray tracing manager
		m_StatsPanel.SetRayTracingManager(&m_RayTracingManager);

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
				}
				else {
					SerializeScene(m_ActiveScene, args[0]);
				}
				m_ConsolePanel.AddLog("Scene saved successfully", LogLevel::Info, "Scene");
			});

		m_ConsolePanel.RegisterCommand("new_scene", "Create a new empty scene", "new_scene",
			[this](const std::vector<std::string>& args) {
				NewScene();
				m_ConsolePanel.AddLog("New scene created", LogLevel::Info, "Scene");
			});

		// Project commands
		m_ConsolePanel.RegisterCommand("new_project", "Create a new project", "new_project",
			[this](const std::vector<std::string>& args) {
				NewProject();
				m_ConsolePanel.AddLog("New project created", LogLevel::Info, "Project");
			});

		m_ConsolePanel.RegisterCommand("save_project", "Save the current project", "save_project",
			[this](const std::vector<std::string>& args) {
				SaveProject();
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

		// Ray Tracing commands
		m_ConsolePanel.RegisterCommand("rt_rebuild", "Rebuild BVH for ray tracing", "rt_rebuild",
			[this](const std::vector<std::string>& args) {
				m_GeometryDirty = true;
				m_ConsolePanel.AddLog("Ray tracing BVH marked for rebuild", LogLevel::Info, "RayTracing");
			});

		m_ConsolePanel.RegisterCommand("rt_stats", "Show ray tracing statistics", "rt_stats",
			[this](const std::vector<std::string>& args) {
				auto stats = m_RayTracingManager.GetStats();
				m_ConsolePanel.AddLog("=== Ray Tracing Statistics ===", LogLevel::Info, "RayTracing");
				m_ConsolePanel.AddLog("Triangles: " + std::to_string(stats.triangleCount), LogLevel::Info, "RayTracing");
				m_ConsolePanel.AddLog("BVH Nodes: " + std::to_string(stats.bvhNodeCount), LogLevel::Info, "RayTracing");
				m_ConsolePanel.AddLog("BVH Build Time: " + std::to_string(stats.bvhBuildTime) + "ms", LogLevel::Info, "RayTracing");
				m_ConsolePanel.AddLog("Shadow Compute Time: " + std::to_string(stats.shadowComputeTime) + "ms", LogLevel::Info, "RayTracing");
				m_ConsolePanel.AddLog("Geometry Dirty: " + std::string(stats.geometryDirty ? "Yes" : "No"), LogLevel::Info, "RayTracing");
			});

		m_ConsolePanel.AddLog("Lunex Editor initialized", LogLevel::Info, "System");
		m_ConsolePanel.AddLog("Welcome! Type 'help' to see available commands", LogLevel::Info, "System");

		UI_UpdateWindowTitle();
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
			
			// Resize ray tracing manager
			m_RayTracingManager.Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		// Render
		Renderer2D::ResetStats();
		Renderer3D::ResetStats();  // ✅ AÑADIDO: Reset stats del 3D también

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

		// ✅ ENTITY PICKING - Funciona para 2D Y 3D
		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		glm::vec2 viewportSize = m_ViewportBounds[1] - m_ViewportBounds[0];
		my = viewportSize.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y) {
			int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY);
			m_HoveredEntity = pixelData == -1 ? Entity() : Entity((entt::entity)pixelData, m_ActiveScene.get());
		}

		// Update stats panel with hovered entity
		m_StatsPanel.SetHoveredEntity(m_HoveredEntity);

		OnOverlayRender();

		// ===== COMPUTE RAY TRACING =====
		if (m_SettingsPanel.GetEnableComputeRayTracing()) {
			LNX_PROFILE_SCOPE("Compute Ray Tracing");
			
			// Update scene geometry (only when dirty)
			if (m_GeometryDirty) {
				LNX_LOG_TRACE("Updating scene geometry for ray tracing...");
				m_RayTracingManager.UpdateSceneGeometry(m_ActiveScene.get());
				m_GeometryDirty = false;
				
				// Log statistics
				auto stats = m_RayTracingManager.GetStats();
				LNX_LOG_INFO("Ray Tracing: {0} triangles, {1} BVH nodes, built in {2:.2f}ms",
							 stats.triangleCount, stats.bvhNodeCount, stats.bvhBuildTime);
			}
			
			// Pass G-Buffer textures from main framebuffer to ray tracing manager
			uint32_t positionTex = m_Framebuffer->GetColorAttachmentRendererID(2);
			uint32_t normalTex = m_Framebuffer->GetColorAttachmentRendererID(3);
			
			// Compute shadows using external G-Buffer
			m_RayTracingManager.ComputeShadowsWithGBuffer(m_ActiveScene.get(), positionTex, normalTex);
		}
		
		// Bind shadow texture for rendering (slot 10 matches u_ComputeShadowMap in shader)
		m_RayTracingManager.BindShadowTexture(10);

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

		// Render MenuBar using MenuBarPanel
		m_MenuBarPanel.OnImGuiRender();

		// Render all panels
		m_SceneHierarchyPanel.OnImGuiRender();
		m_PropertiesPanel.OnImGuiRender();
		m_ContentBrowserPanel.OnImGuiRender();
		m_StatsPanel.OnImGuiRender();
		m_SettingsPanel.OnImGuiRender();
		m_ConsolePanel.OnImGuiRender();

		// Render dialogs (on top)
		m_ProjectCreationDialog.OnImGuiRender();

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		
		// Sincronizar la entidad seleccionada con PropertiesPanel
		m_PropertiesPanel.SetSelectedEntity(selectedEntity);
		
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
		
		// Forward events to Content Browser for file drop
		m_ContentBrowserPanel.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(LNX_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(LNX_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e) {
		if (e.GetRepeatCount() > 0)
			return false;

		bool control = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);
		bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
		bool alt = Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt);

		switch (e.GetKeyCode()) {
		// Project shortcuts
		case Key::N: {
			if (control && shift)
				NewProject();
			else if (control)
				NewScene();
			break;
		}
		case Key::O: {
			if (control && shift)
				OpenProject();
			else if (control)
				OpenScene();
			break;
		}
		case Key::S: {
			if (control && shift)
				SaveProject();
			else if (control && alt)
				SaveSceneAs();
			else if (control)
				SaveScene();
			break;
		}
		// Entity operations
		case Key::D: {
			if (control)
				OnDuplicateEntity();
			break;
		}
		// Gizmo modes
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
		// ✅ LÓGICA DE SELECCIÓN - Funciona para 2D Y 3D automáticamente
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

		// ✅ Draw selected entity outline - FUNCIONA PARA 2D Y 3D
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
		m_PropertiesPanel.SetContext(m_ActiveScene);
		m_EditorScenePath = std::filesystem::path();
		m_GeometryDirty = true; // Mark for BVH rebuild
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
			m_PropertiesPanel.SetContext(m_EditorScene);
			m_ActiveScene = m_EditorScene;
			m_EditorScenePath = path;
			m_GeometryDirty = true; // Mark for BVH rebuild
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
		m_PropertiesPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnSceneSimulate() {
		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		m_SceneState = SceneState::Simulate;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnSimulationStart();
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_PropertiesPanel.SetContext(m_ActiveScene);
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
		m_PropertiesPanel.SetContext(m_ActiveScene);
	}

	void EditorLayer::OnDuplicateEntity() {
		if (m_SceneState != SceneState::Edit)
			return;

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity)
			m_EditorScene->DuplicateEntity(selectedEntity);
	}

	// ============================================================================
	// PROJECT MANAGEMENT
	// ============================================================================

	void EditorLayer::NewProject() {
		// Open project creation dialog
		m_ProjectCreationDialog.Open();
	}

	void EditorLayer::CreateProjectWithDialog(const std::string& name, const std::filesystem::path& location) {
		// Create project directory
		std::filesystem::path projectPath = location / name;
		std::filesystem::path projectFile = projectPath / "project.lunex";
		
		// Create new project
		Ref<Project> project = ProjectManager::New();
		project->SetName(name);
		project->GetConfig().AssetDirectory = "Assets";
		project->GetConfig().Width = 1920;
		project->GetConfig().Height = 1080;
		project->GetConfig().VSync = true;
		project->GetConfig().StartScene = "Scenes/SampleScene.lunex";
		
		// Save project (this will create directories and default scene)
		if (!ProjectManager::SaveActive(projectFile)) {
			LNX_LOG_ERROR("Failed to create project");
			m_ConsolePanel.AddLog("Failed to create project: " + name, LogLevel::Error, "Project");
			return;
		}
		
		// Update Content Browser to show project assets
		m_ContentBrowserPanel.SetRootDirectory(project->GetAssetDirectory());
		
		// Load the default scene
		auto startScenePath = project->GetAssetFileSystemPath(project->GetConfig().StartScene);
		if (std::filesystem::exists(startScenePath)) {
			OpenScene(startScenePath);
		}
		else {
			// If default scene doesn't exist, create new empty scene
			NewScene();
		}
		
		UI_UpdateWindowTitle();
		m_ConsolePanel.AddLog("Project created: " + name, LogLevel::Info, "Project");
		LNX_LOG_INFO("Project created successfully at: {0}", projectPath.string());
	}

	void EditorLayer::OpenProject() {
		std::string filepath = FileDialogs::OpenFile("Lunex Project (*.lunex)\0*.lunex\0");
		if (!filepath.empty())
			OpenProject(filepath);
	}

	void EditorLayer::OpenProject(const std::filesystem::path& path) {
		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		if (path.extension().string() != ".lunex") {
			LNX_LOG_WARN("Could not load {0} - not a project file", path.filename().string());
			m_ConsolePanel.AddLog("Failed to open project: Not a .lunex file", LogLevel::Error, "Project");
			return;
		}

		Ref<Project> project = ProjectManager::Load(path);
		if (!project) {
			m_ConsolePanel.AddLog("Failed to load project: " + path.string(), LogLevel::Error, "Project");
			return;
		}

		// Update Content Browser to show project assets
		m_ContentBrowserPanel.SetRootDirectory(project->GetAssetDirectory());

		// Load start scene if specified
		auto& config = project->GetConfig();
		if (!config.StartScene.empty()) {
			auto startScenePath = project->GetAssetFileSystemPath(config.StartScene);
			if (std::filesystem::exists(startScenePath)) {
				OpenScene(startScenePath);
			}
			else {
				LNX_LOG_WARN("Start scene not found: {0}", startScenePath.string());
				NewScene();
			}
		}
		else {
			NewScene();
		}

		UI_UpdateWindowTitle();
		m_ConsolePanel.AddLog("Project opened: " + project->GetName(), LogLevel::Info, "Project");
	}

	void EditorLayer::SaveProject() {
		auto project = ProjectManager::GetActiveProject();
		if (!project) {
			LNX_LOG_ERROR("No active project!");
			m_ConsolePanel.AddLog("No active project to save", LogLevel::Error, "Project");
			return;
		}

		auto projectPath = project->GetProjectPath();
		if (projectPath.empty()) {
			SaveProjectAs();
			return;
		}

		// Save current scene first
		if (!m_EditorScenePath.empty()) {
			SerializeScene(m_ActiveScene, m_EditorScenePath);
		}

		if (ProjectManager::SaveActive(projectPath)) {
			m_ConsolePanel.AddLog("Project saved: " + project->GetName(), LogLevel::Info, "Project");
		}
		else {
			m_ConsolePanel.AddLog("Failed to save project", LogLevel::Error, "Project");
		}
	}

	void EditorLayer::SaveProjectAs() {
		auto project = ProjectManager::GetActiveProject();
		if (!project) {
			LNX_LOG_ERROR("No active project!");
			return;
		}

		std::string filepath = FileDialogs::SaveFile("Lunex Project (*.lunex)\0*.lunex\0");
		if (!filepath.empty()) {
			// Ensure .lunex extension
			std::filesystem::path path(filepath);
			if (path.extension() != ".lunex") {
				path += ".lunex";
			}

			// Update project name from filename
			project->SetName(path.stem().string());

			// Set asset directory relative to project file
			project->GetConfig().AssetDirectory = "Assets";

			if (ProjectManager::SaveActive(path)) {
				m_ConsolePanel.AddLog("Project saved as: " + path.string(), LogLevel::Info, "Project");
				UI_UpdateWindowTitle();
			}
			else {
				m_ConsolePanel.AddLog("Failed to save project", LogLevel::Error, "Project");
			}
		}
	}

	void EditorLayer::UI_UpdateWindowTitle() {
		auto project = ProjectManager::GetActiveProject();
		std::string title = "Lunex Editor";

		if (project) {
			title += " - " + project->GetName();
			m_MenuBarPanel.SetProjectName(project->GetName());
		}
		else {
			m_MenuBarPanel.SetProjectName("No Project");
		}
	}
}