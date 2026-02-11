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

// ✅ Input System
#include "Input/InputManager.h"

// ✅ MeshAsset System (Unified Assets)
#include "Assets/Mesh/MeshAsset.h"
#include "Assets/Mesh/MeshImporter.h"
#include "Asset/Prefab.h"

// ✅ AssetDatabase (Unified Assets)
#include "Assets/Core/AssetDatabase.h"

// ✅ Skybox Renderer for camera preview
#include "Renderer/SkyboxRenderer.h"

// ✅ Outline Renderer for selection outlines and collider visualization
#include "Renderer/Outline/OutlineRenderer.h"

#include "RHI/RHI.h"

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
		
		// ========================================
		// INITIALIZE JOB SYSTEM
		// ========================================
		JobSystemConfig jobConfig;
		jobConfig.NumWorkers = std::thread::hardware_concurrency() - 1; // Reserve 1 for main thread
		jobConfig.NumIOWorkers = 2;
		jobConfig.EnableWorkStealing = true;
		jobConfig.EnableProfiling = true;
		JobSystem::Init(jobConfig);
		LNX_LOG_INFO("JobSystem initialized with {0} workers", jobConfig.NumWorkers);
		
		// ✅ NEW: Initialize Input System
		InputManager::Get().Initialize();
		RegisterEditorActions();
		
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
		
		// ========================================
		// LOAD GIZMO SETTINGS ICONS
		// ========================================
		LNX_LOG_INFO("Loading gizmo settings icons...");
		
		// Pivot Point Icons
		Ref<Texture2D> iconMedianPoint = Texture2D::Create("Resources/Icons/GizmoSettings/MedianPointIcon.png");
		if (iconMedianPoint) {
			m_GizmoSettingsPanel.SetMedianPointIcon(iconMedianPoint);
			LNX_LOG_INFO("✓ MedianPointIcon loaded successfully");
		} else {
			LNX_LOG_WARN("Failed to load MedianPointIcon.png - using fallback emoji");
		}
		
		Ref<Texture2D> iconActiveElement = Texture2D::Create("Resources/Icons/GizmoSettings/ActiveElementIcon.png");
		if (iconActiveElement) {
			m_GizmoSettingsPanel.SetActiveElementIcon(iconActiveElement);
			LNX_LOG_INFO("✓ ActiveElementIcon loaded successfully");
		} else {
			LNX_LOG_WARN("Failed to load ActiveElementIcon.png - using fallback emoji");
		}
		
		Ref<Texture2D> iconIndividualOrigins = Texture2D::Create("Resources/Icons/GizmoSettings/IndividualOriginsIcon.png");
		if (iconIndividualOrigins) {
			m_GizmoSettingsPanel.SetIndividualOriginsIcon(iconIndividualOrigins);
			LNX_LOG_INFO("✓ IndividualOriginsIcon loaded successfully");
		} else {
			LNX_LOG_WARN("Failed to load IndividualOriginsIcon.png - using fallback emoji");
		}
		
		Ref<Texture2D> iconBoundingBox = Texture2D::Create("Resources/Icons/GizmoSettings/BoundingBoxIcon.png");
		if (iconBoundingBox) {
			m_GizmoSettingsPanel.SetBoundingBoxIcon(iconBoundingBox);
			LNX_LOG_INFO("✓ BoundingBoxIcon loaded successfully");
		} else {
			LNX_LOG_WARN("Failed to load BoundingBoxIcon.png - using fallback emoji");
		}
		
		// Orientation Icons
		Ref<Texture2D> iconGlobal = Texture2D::Create("Resources/Icons/GizmoSettings/GlobalOrientationIcon.png");
		if (iconGlobal) {
			m_GizmoSettingsPanel.SetGlobalIcon(iconGlobal);
			LNX_LOG_INFO("✓ GlobalOrientationIcon loaded successfully");
		} else {
			LNX_LOG_WARN("Failed to load GlobalOrientationIcon.png - using fallback emoji");
		}
		
		Ref<Texture2D> iconLocal = Texture2D::Create("Resources/Icons/GizmoSettings/LocalOrientationIcon.png");
		if (iconLocal) {
			m_GizmoSettingsPanel.SetLocalIcon(iconLocal);
			LNX_LOG_INFO("✓ LocalOrientationIcon loaded successfully");
		} else {
			LNX_LOG_WARN("Failed to load LocalOrientationIcon.png - using fallback emoji");
		}
		
		// Configure viewport panel
		m_ViewportPanel.SetOnSceneDropCallback([this](const std::filesystem::path& path) {
			OpenScene(path);
			});
		
		// ========================================
		// MESH IMPORT CALLBACKS
		// ========================================
		m_ViewportPanel.SetOnModelDropCallback([this](const std::filesystem::path& path) {
			OnModelDropped(path);
		});
		
		m_ViewportPanel.SetOnMeshAssetDropCallback([this](const std::filesystem::path& path) {
			OnMeshAssetDropped(path);
		});
		
		m_ViewportPanel.SetOnPrefabDropCallback([this](const std::filesystem::path& path) {
			OnPrefabDropped(path);
		});
		
		m_MeshImportModal.SetOnImportCallback([this](Ref<MeshAsset> asset) {
			OnMeshImported(asset);
		});
		
		LNX_LOG_INFO("✅ MeshAsset and Prefab import system configured");

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
		
		m_MenuBarPanel.SetOnOpenInputSettingsCallback([this]() { m_InputSettingsPanel.Open(); });
		m_MenuBarPanel.SetOnOpenJobSystemPanelCallback([this]() { m_JobSystemPanel.Toggle(); });

		// ========================================
		// MATERIAL EDITOR PANEL CALLBACKS
		// ========================================
		
		// 1. Content Browser -> Material Editor (double-click .umat files)
		m_ContentBrowserPanel.SetOnMaterialOpenCallback([this](const std::filesystem::path& path) {
			m_MaterialEditorPanel.OpenMaterial(path);
		});
		
		// 2. Properties Panel -> Material Editor (Edit Material button)
		m_PropertiesPanel.SetOnMaterialEditCallback([this](Ref<MaterialAsset> asset) {
			m_MaterialEditorPanel.OpenMaterial(asset);
		});
		
		// 3. Material Editor -> Content Browser & Properties Panel (hot reload when saved)
		m_MaterialEditorPanel.SetOnMaterialSavedCallback([this](const std::filesystem::path& path) {
			// Invalidate Content Browser thumbnail (memory cache)
			m_ContentBrowserPanel.InvalidateMaterialThumbnail(path);
			
			// ✅ Invalidate disk cache so thumbnail is regenerated
			m_ContentBrowserPanel.InvalidateThumbnailDiskCache(path);
			
			// Also invalidate PropertiesPanel thumbnail cache for the material
			// (we need to get the material's UUID to invalidate it)
			auto material = MaterialRegistry::Get().LoadMaterial(path);
			if (material) {
				m_PropertiesPanel.InvalidateMaterialThumbnail(material->GetID());
			}
			
			LNX_LOG_INFO("Hot reload: Material thumbnails invalidated for {0}", path.filename().string());
		});
		
		LNX_LOG_INFO("✅ Material Editor Panel callbacks configured");

		// Configure project creation dialog
		m_ProjectCreationDialog.SetOnCreateCallback([this](const std::string& name, const std::filesystem::path& location) {
			CreateProjectWithDialog(name, location);
		});

		Lunex::FramebufferSpecification fbSpec;
		fbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::Depth };
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		// Camera Preview Framebuffer (smaller size for performance)
		Lunex::FramebufferSpecification previewFbSpec;
		previewFbSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
		previewFbSpec.Width = 400;
		previewFbSpec.Height = 225; // 16:9 aspect ratio
		m_CameraPreviewFramebuffer = Framebuffer::Create(previewFbSpec);

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

		// ========================================
		// INITIALIZE OUTLINE RENDERER
		// ========================================
		OutlineRenderer::Get().Initialize(1280, 720);
		LNX_LOG_INFO("✅ OutlineRenderer initialized");

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
		
		// ✅ NEW: AssetDatabase commands
		m_ConsolePanel.RegisterCommand("refresh_assets", "Refresh the asset database", "refresh_assets",
			[this](const std::vector<std::string>& args) {
				auto project = ProjectManager::GetActiveProject();
				if (!project) {
					m_ConsolePanel.AddLog("No active project", LogLevel::Warning, "Assets");
					return;
				}
				
				m_ConsolePanel.AddLog("Refreshing AssetDatabase...", LogLevel::Info, "Assets");
				ProjectManager::RefreshAssetDatabase();
				
				auto& db = ProjectManager::GetAssetDatabase();
				m_ConsolePanel.AddLog("AssetDatabase refreshed: " + std::to_string(db.GetAssetCount()) + " assets", LogLevel::Info, "Assets");
			});
		
		m_ConsolePanel.RegisterCommand("list_assets", "List all assets in the database", "list_assets [type]",
			[this](const std::vector<std::string>& args) {
				auto project = ProjectManager::GetActiveProject();
				if (!project) {
					m_ConsolePanel.AddLog("No active project", LogLevel::Warning, "Assets");
					return;
				}
				
				auto& db = ProjectManager::GetAssetDatabase();
				const auto& assets = db.GetAllAssets();
				
				if (assets.empty()) {
					m_ConsolePanel.AddLog("No assets found in database", LogLevel::Info, "Assets");
					return;
				}
				
				m_ConsolePanel.AddLog("Assets in database:", LogLevel::Info, "Assets");
				
				// Filter by type if specified
				std::string typeFilter = "";
				if (!args.empty()) {
					typeFilter = args[0];
					std::transform(typeFilter.begin(), typeFilter.end(), typeFilter.begin(), ::tolower);
				}
				
				int count = 0;
				for (const auto& [id, entry] : assets) {
					// Apply filter if specified
					if (!typeFilter.empty()) {
						std::string typeName = AssetTypeToString(entry.Type);
						std::string lowerTypeName = typeName;
						std::transform(lowerTypeName.begin(), lowerTypeName.end(), lowerTypeName.begin(), ::tolower);
						
						if (lowerTypeName.find(typeFilter) == std::string::npos) {
							continue;
						}
					}
					
					std::string msg = "  [" + std::string(AssetTypeToString(entry.Type)) + "] " + entry.Name;
					m_ConsolePanel.AddLog(msg, LogLevel::Info, "Assets");
					count++;
				}
				
				m_ConsolePanel.AddLog("Total: " + std::to_string(count) + " assets", LogLevel::Info, "Assets");
			});

		m_ConsolePanel.AddLog("Lunex Editor initialized", LogLevel::Info, "System");
		m_ConsolePanel.AddLog("Welcome! Type 'help' to see available commands", LogLevel::Info, "System");

		UI_UpdateWindowTitle();
	}

	void EditorLayer::OnDetach() {
		LNX_PROFILE_FUNCTION();
		
		// ========================================
		// SHUTDOWN OUTLINE RENDERER
		// ========================================
		OutlineRenderer::Get().Shutdown();
		
		// ========================================
		// SHUTDOWN JOB SYSTEM
		// ========================================
		JobSystem::Get().WaitForAllJobs();
		JobSystem::Shutdown();
		LNX_LOG_INFO("JobSystem shut down");
		
		// ✅ NEW: Shutdown Input System
		InputManager::Get().Shutdown();
	}
	
	// ========================================
	// REGISTER EDITOR ACTIONS
	// ========================================
	
	void EditorLayer::RegisterEditorActions() {
		LNX_LOG_INFO("Registering editor actions...");
		
		auto& registry = ActionRegistry::Get();
		
		// ========================================
		// PROJECT OPERATIONS (Ctrl+Shift+N/O/S)
		// ========================================
		registry.Register("Editor.NewProject", CreateRef<FunctionAction>(
			"Editor.NewProject", ActionContext::Pressed,
			[this](const ActionState&) { NewProject(); },
			"Create new project", true
		));
		
		registry.Register("Editor.OpenProject", CreateRef<FunctionAction>(
			"Editor.OpenProject", ActionContext::Pressed,
			[this](const ActionState&) { OpenProject(); },
			"Open project", true
		));
		
		registry.Register("Editor.SaveProject", CreateRef<FunctionAction>(
			"Editor.SaveProject", ActionContext::Pressed,
			[this](const ActionState&) { SaveProject(); },
			"Save project", true
		));
		
		// ========================================
		// SCENE OPERATIONS (Ctrl+S/O/N/P)
		// ========================================
		registry.Register("Editor.SaveScene", CreateRef<FunctionAction>(
			"Editor.SaveScene", ActionContext::Pressed,
			[this](const ActionState&) { SaveScene(); },
			"Save current scene", true
		));
		
		registry.Register("Editor.OpenScene", CreateRef<FunctionAction>(
			"Editor.OpenScene", ActionContext::Pressed,
			[this](const ActionState&) { OpenScene(); },
			"Open scene", true
		));
		
		registry.Register("Editor.NewScene", CreateRef<FunctionAction>(
			"Editor.NewScene", ActionContext::Pressed,
			[this](const ActionState&) { NewScene(); },
			"Create new scene", true
		));
		
		registry.Register("Editor.PlayScene", CreateRef<FunctionAction>(
			"Editor.PlayScene", ActionContext::Pressed,
			[this](const ActionState&) { 
				if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) {
					OnScenePlay();
				} else if (m_SceneState == SceneState::Play) {
					OnSceneStop();
				}
			},
			"Play/Stop scene", true
		));
		
		// ========================================
		// ENTITY OPERATIONS (Global - SceneHierarchy)
		// ========================================
		registry.Register("Editor.DuplicateEntity", CreateRef<FunctionAction>(
			"Editor.DuplicateEntity", ActionContext::Pressed,
			[this](const ActionState&) { 
				if (m_SceneState != SceneState::Edit) return;
				m_SceneHierarchyPanel.DuplicateSelectedEntities();
			},
			"Duplicate selected entity/item", true
		));
		
		registry.Register("Editor.SelectAll", CreateRef<FunctionAction>(
			"Editor.SelectAll", ActionContext::Pressed,
			[this](const ActionState&) {
				// Detectar qué panel está enfocado (Hierarchy tiene prioridad)
				if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
					// TODO: Mejorar detección de panel enfocado
					m_SceneHierarchyPanel.SelectAll();
				}
			},
			"Select all items in focused panel", true
		));
		
		registry.Register("Editor.DeleteSelected", CreateRef<FunctionAction>(
			"Editor.DeleteSelected", ActionContext::Pressed,
			[this](const ActionState&) {
				if (m_SceneState != SceneState::Edit) return;
				
				// ========================================
				// CONTEXT-AWARE DELETE (Smart detection)
				// ========================================
				// Check which panel is focused and delete accordingly
				
				// Get focused window name
				ImGuiWindow* window = ImGui::GetCurrentContext()->NavWindow;
				const char* focusedWindow = window ? window->Name : nullptr;
				
				// Priority 1: Content Browser (if has selection and is focused)
				if (focusedWindow && 
					(strcmp(focusedWindow, "Content Browser") == 0 || 
					 strstr(focusedWindow, "Content Browser") != nullptr)) {
					// Content Browser is focused - delete files/folders
					m_ContentBrowserPanel.DeleteSelectedItems();
					LNX_LOG_INFO("Context-aware delete: Content Browser items");
					return;
				}
				
				// Priority 2: Scene Hierarchy (if has selection and is focused)
				if (focusedWindow && 
					(strcmp(focusedWindow, "Scene Hierarchy") == 0 || 
					 strstr(focusedWindow, "Scene Hierarchy") != nullptr)) {
					// Scene Hierarchy is focused - delete entities
					m_SceneHierarchyPanel.DeleteSelectedEntities();
					LNX_LOG_INFO("Context-aware delete: Scene entities");
					return;
				}
				
				// Priority 3: If Content Browser has selections (fallback)
				if (m_ContentBrowserPanel.HasSelection()) {
					m_ContentBrowserPanel.DeleteSelectedItems();
					LNX_LOG_INFO("Context-aware delete: Content Browser items (fallback)");
					return;
				}
				
				// Priority 4: If Scene Hierarchy has selections (fallback)
				if (!m_SceneHierarchyPanel.GetSelectedEntities().empty()) {
					m_SceneHierarchyPanel.DeleteSelectedEntities();
					LNX_LOG_INFO("Context-aware delete: Scene entities (fallback)");
					return;
				}
				
				// No selections in either panel
				LNX_LOG_WARN("Delete pressed but no items selected in any panel");
			},
			"Delete selected items (context-aware)", true
		));
		
		registry.Register("Editor.RenameSelected", CreateRef<FunctionAction>(
			"Editor.RenameSelected", ActionContext::Pressed,
			[this](const ActionState&) {
				m_SceneHierarchyPanel.RenameSelectedEntity();
			},
			"Rename selected item", true
		));
		
		registry.Register("Editor.ClearSelection", CreateRef<FunctionAction>(
			"Editor.ClearSelection", ActionContext::Pressed,
			[this](const ActionState&) {
				m_SceneHierarchyPanel.ClearSelection();
				m_ContentBrowserPanel.ClearSelection();
			},
			"Clear selection in all panels", true
		));
		
		// ========================================
		// CLIPBOARD OPERATIONS (Global - ContentBrowser)
		// ========================================
		registry.Register("Editor.Copy", CreateRef<FunctionAction>(
			"Editor.Copy", ActionContext::Pressed,
			[this](const ActionState&) {
				m_ContentBrowserPanel.CopySelectedItems();
			},
			"Copy selected items", true
		));
		
		registry.Register("Editor.Cut", CreateRef<FunctionAction>(
			"Editor.Cut", ActionContext::Pressed,
			[this](const ActionState&) {
				m_ContentBrowserPanel.CutSelectedItems();
			},
			"Cut selected items", true
		));
		
		registry.Register("Editor.Paste", CreateRef<FunctionAction>(
			"Editor.Paste", ActionContext::Pressed,
			[this](const ActionState&) {
				m_ContentBrowserPanel.PasteItems();
			},
			"Paste items", true
		));
		
		// ========================================
		// NAVIGATION (Content Browser)
		// ========================================
		registry.Register("Editor.NavigateBack", CreateRef<FunctionAction>(
			"Editor.NavigateBack", ActionContext::Pressed,
			[this](const ActionState&) {
				m_ContentBrowserPanel.NavigateBack();
			},
			"Navigate back in Content Browser", true
		));
		
		registry.Register("Editor.NavigateForward", CreateRef<FunctionAction>(
			"Editor.NavigateForward", ActionContext::Pressed,
			[this](const ActionState&) {
				m_ContentBrowserPanel.NavigateForward();
			},
			"Navigate forward in Content Browser", true
		));
		
		registry.Register("Editor.NavigateUp", CreateRef<FunctionAction>(
			"Editor.NavigateUp", ActionContext::Pressed,
			[this](const ActionState&) {
				m_ContentBrowserPanel.NavigateUp();
			},
			"Navigate to parent directory", true
		));
		
		// ========================================
		// GIZMO OPERATIONS
		// ========================================
		registry.Register("Gizmo.None", CreateRef<FunctionAction>(
			"Gizmo.None", ActionContext::Pressed,
			[this](const ActionState&) { if (!ImGuizmo::IsUsing()) m_GizmoType = -1; },
			"Deselect gizmo", false
		));
		
		registry.Register("Gizmo.Translate", CreateRef<FunctionAction>(
			"Gizmo.Translate", ActionContext::Pressed,
			[this](const ActionState&) { if (!ImGuizmo::IsUsing()) m_GizmoType = ImGuizmo::OPERATION::TRANSLATE; },
			"Translate gizmo", false
		));
		
		registry.Register("Gizmo.Rotate", CreateRef<FunctionAction>(
			"Gizmo.Rotate", ActionContext::Pressed,
			[this](const ActionState&) { if (!ImGuizmo::IsUsing()) m_GizmoType = ImGuizmo::OPERATION::ROTATE; },
			"Rotate gizmo", false
		));
		
		registry.Register("Gizmo.Scale", CreateRef<FunctionAction>(
			"Gizmo.Scale", ActionContext::Pressed,
			[this](const ActionState&) { if (!ImGuizmo::IsUsing()) m_GizmoType = ImGuizmo::OPERATION::SCALE; },
			"Scale gizmo", false
		));
		
		// ========================================
		// DEBUG COMMANDS
		// ========================================
		registry.Register("Debug.ToggleStats", CreateRef<FunctionAction>(
			"Debug.ToggleStats", ActionContext::Pressed,
			[this](const ActionState&) { m_StatsPanel.Toggle(); },
			"Toggle stats panel"
		));
		
		registry.Register("Debug.ToggleColliders", CreateRef<FunctionAction>(
			"Debug.ToggleColliders", ActionContext::Pressed,
			[this](const ActionState&) { 
				// Toggle both 2D and 3D colliders
				bool current = m_SettingsPanel.GetShowPhysicsColliders();
				m_SettingsPanel.SetShowPhysicsColliders(!current);
				m_SettingsPanel.SetShowPhysics3DColliders(!current);
			},
			"Toggle collider visualization"
		));
		
		registry.Register("Debug.ToggleConsole", CreateRef<FunctionAction>(
			"Debug.ToggleConsole", ActionContext::Pressed,
			[this](const ActionState&) { m_ConsolePanel.Toggle(); },
			"Toggle console panel"
		));
		
		// ========================================
		// PREFERENCES
		// ========================================
		registry.Register("Preferences.InputSettings", CreateRef<FunctionAction>(
			"Preferences.InputSettings", ActionContext::Pressed,
			[this](const ActionState&) { m_InputSettingsPanel.Open(); },
			"Open input settings", true
		));
		
		LNX_LOG_INFO("✅ Registered {0} editor actions (100% complete!)", registry.GetActionCount());
	}

	void EditorLayer::OnUpdate(Lunex::Timestep ts) {
		LNX_PROFILE_FUNCTION();
		
		// ========================================
		// FLUSH MAIN-THREAD COMMANDS (Job System)
		// ========================================
		JobSystem::Get().FlushMainThreadCommands(0);
		
		// ✅ NEW: Update Input System
		InputManager::Get().Update(ts);

		// Get viewport size from ViewportPanel
		m_ViewportSize = m_ViewportPanel.GetViewportSize();
		const glm::vec2* viewportBounds = m_ViewportPanel.GetViewportBounds();
		m_ViewportBounds[0] = viewportBounds[0];
		m_ViewportBounds[1] = viewportBounds[1];

		// Resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_CameraController.OnResize(m_ViewportSize.x, m_ViewportSize.y);

			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			OutlineRenderer::Get().OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}

		// ========================================
		// MAIN VIEWPORT RENDERING
		// ========================================
		Renderer2D::ResetStats();
		Renderer3D::ResetStats();

		// ========================================
		// SHADOW PASS (before main framebuffer bind)
		// Renders shadow maps into the shadow atlas
		// ========================================
		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) {
			Renderer3D::UpdateShadows(m_ActiveScene.get(), m_EditorCamera);
		} else if (m_SceneState == SceneState::Play) {
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (camera) {
				auto& cameraComp = camera.GetComponent<CameraComponent>();
				auto& transformComp = camera.GetComponent<TransformComponent>();
				Renderer3D::UpdateShadows(m_ActiveScene.get(), cameraComp.Camera, transformComp.GetTransform());
			}
		}

		// ✅ CRITICAL FIX: Bind framebuffer BEFORE rendering
		m_Framebuffer->Bind();

		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetViewport(0.0f, 0.0f, m_ViewportSize.x, m_ViewportSize.y);
			
			// ✅ FIX: Use SkyboxRenderer's background color when no HDRI is loaded
			if (!SkyboxRenderer::HasEnvironmentLoaded()) {
				glm::vec3 bgColor = SkyboxRenderer::GetBackgroundColor();
				cmdList->SetClearColor(glm::vec4(bgColor, 1.0f));
			} else {
				cmdList->SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			}
			cmdList->Clear();
		}

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

		// ✅ ENTITY PICKING
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

		m_StatsPanel.SetHoveredEntity(m_HoveredEntity);
		//m_MaterialEditorPanel.OnUpdate(ts.GetSeconds());

		OnOverlayRender();

		m_Framebuffer->Unbind();

		// ========================================
		// CAMERA PREVIEW RENDERING (after main scene, separate pass)
		// ========================================

		// Material Editor Preview (has own UBOs but shares bindings)
		m_MaterialEditorPanel.OnUpdate(ts.GetSeconds());
		
		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity && selectedEntity.HasComponent<CameraComponent>() && m_SceneState == SceneState::Edit) {
			RenderCameraPreview(selectedEntity);
		}

		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) {
			Renderer3D::BeginScene(m_EditorCamera);
			Renderer3D::UpdateLights(m_ActiveScene.get());
			Renderer3D::EndScene();
		}
		else if (m_SceneState == SceneState::Play) {
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (camera) {
				auto& cameraComp = camera.GetComponent<CameraComponent>();
				auto& transformComp = camera.GetComponent<TransformComponent>();
				glm::mat4 cameraTransform = transformComp.GetTransform();
				Renderer3D::BeginScene(cameraComp.Camera, cameraTransform);
				Renderer3D::UpdateLights(m_ActiveScene.get());
				Renderer3D::EndScene();
			}
		}
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

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f,0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 22.0f)); // Increase menu bar height for the owning window
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar(); // FramePadding for menu bar height
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
		m_MaterialEditorPanel.OnImGuiRender();
		m_StatsPanel.OnImGuiRender();
		m_SettingsPanel.OnImGuiRender();
		m_ConsolePanel.OnImGuiRender();
		m_InputSettingsPanel.OnImGuiRender();
		m_JobSystemPanel.OnImGuiRender();
		m_MeshImportModal.OnImGuiRender();  // NEW: Mesh import modal

		// Render dialogs (on top)
		m_ProjectCreationDialog.OnImGuiRender();

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		
		// Sincronizar la entidad seleccionada con PropertiesPanel
		m_PropertiesPanel.SetSelectedEntity(selectedEntity);
		
		// Pass camera preview framebuffer only if a camera is selected
		Ref<Framebuffer> cameraPreview = nullptr;
		if (selectedEntity && selectedEntity.HasComponent<CameraComponent>() && m_SceneState == SceneState::Edit) {
			cameraPreview = m_CameraPreviewFramebuffer;
		}
		
		m_ViewportPanel.OnImGuiRender(m_Framebuffer, cameraPreview, m_SceneHierarchyPanel, m_EditorCamera,
			selectedEntity, m_GizmoType, m_ToolbarPanel,
			m_SceneState, (bool)m_ActiveScene);
		
		// ========================================
		// RENDER GIZMO SETTINGS PANEL (overlay on viewport)
		// ========================================
		bool toolbarEnabled = m_SceneState == SceneState::Edit && m_ActiveScene;
		m_GizmoSettingsPanel.OnImGuiRender(
			glm::vec2(m_ViewportBounds[0].x, m_ViewportBounds[0].y),
			glm::vec2(m_ViewportSize.x, m_ViewportSize.y),
			toolbarEnabled
		);

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
		dispatcher.Dispatch<KeyReleasedEvent>(LNX_BIND_EVENT_FN(EditorLayer::OnKeyReleased));
		dispatcher.Dispatch<MouseButtonPressedEvent>(LNX_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e) {
		if (e.GetRepeatCount() > 0)
			return false;

		// Convert to KeyModifiers format
		uint8_t modifiers = KeyModifiers::None;
		if (Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl))
			modifiers |= KeyModifiers::Ctrl;
		if (Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift))
			modifiers |= KeyModifiers::Shift;
		if (Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt))
			modifiers |= KeyModifiers::Alt;

		// Process through InputManager
		InputManager::Get().OnKeyPressed(static_cast<KeyCode>(e.GetKeyCode()), modifiers);

		return false;
	}

	bool EditorLayer::OnKeyReleased(KeyReleasedEvent& e) {
		// Convert to KeyModifiers format
		uint8_t modifiers = KeyModifiers::None;
		if (Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl))
			modifiers |= KeyModifiers::Ctrl;
		if (Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift))
			modifiers |= KeyModifiers::Shift;
		if (Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt))
			modifiers |= KeyModifiers::Alt;

		// Process through InputManager
		InputManager::Get().OnKeyReleased(static_cast<KeyCode>(e.GetKeyCode()), modifiers);

		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e) {
		// ✅ PREVENT SELECTION when fly camera is active (right click held)
		if (m_EditorCamera.IsFlyCameraActive()) {
			return false; // Don't process clicks while flying
		}

		// ✅ MULTI-SELECTION RAY PICKING (Blender-style)
		if (e.GetMouseButton() == Mouse::ButtonLeft) {
			if (m_ViewportPanel.IsViewportHovered() && !ImGuizmo::IsOver()) {
				// ✅ Use Input system for modifier detection
				bool shiftPressed = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
				bool ctrlPressed = Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl);

				if (m_HoveredEntity) {
					if (shiftPressed) {
						// Shift+Click: Add to selection (Blender-style)
						const auto& selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();

						if (selectedEntities.empty()) {
							// No selection yet, just select this entity
							m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
							LNX_LOG_INFO("Selected first entity");
						}
						else if (m_SceneHierarchyPanel.IsEntitySelected(m_HoveredEntity)) {
							// Already selected, do nothing (keep selection)
							LNX_LOG_INFO("Entity already in selection");
						}
						else {
							// Add clicked entity to selection
							m_SceneHierarchyPanel.AddEntityToSelection(m_HoveredEntity);
							LNX_LOG_INFO("Added entity to multi-selection ({0} selected)", selectedEntities.size() + 1);
						}
					}
					else if (ctrlPressed) {
						// Ctrl+Click: Toggle selection (Blender-style)
						m_SceneHierarchyPanel.ToggleEntitySelection(m_HoveredEntity);
						LNX_LOG_INFO("Toggled entity selection");
					}
					else {
						// Normal click: Select only this entity (clear previous)
						m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
					}
				}
				else {
					// Clicked on empty space
					if (!shiftPressed && !ctrlPressed) {
						// Clear selection if no modifiers
						m_SceneHierarchyPanel.ClearSelection();
						LNX_LOG_INFO("Cleared selection");
					}
				}
			}
		}
		return false;
	}

	void EditorLayer::OnOverlayRender() {
		// ========================================
		// DETERMINE CAMERA VIEW-PROJECTION
		// ========================================
		glm::mat4 viewProjection;
		if (m_SceneState == SceneState::Play) {
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (!camera)
				return;
			auto& cameraComp = camera.GetComponent<CameraComponent>();
			auto& transformComp = camera.GetComponent<TransformComponent>();
			viewProjection = cameraComp.Camera.GetProjection() * glm::inverse(transformComp.GetTransform());
			Renderer2D::BeginScene(cameraComp.Camera, transformComp.GetTransform());
		}
		else {
			viewProjection = m_EditorCamera.GetViewProjection();
			Renderer2D::BeginScene(m_EditorCamera);
		}

		auto& outlineRenderer = OutlineRenderer::Get();

		// ========================================
		// SELECTION OUTLINE (OutlineRenderer - blurred post-process)
		// ========================================
		if (outlineRenderer.IsInitialized()) {
			uint64_t sceneFBOHandle = static_cast<uint64_t>(m_Framebuffer->GetRendererID());

			const auto& selectedEntities = m_SceneHierarchyPanel.GetSelectedEntities();
			if (!selectedEntities.empty()) {
				outlineRenderer.RenderSelectionOutline(
					m_ActiveScene.get(),
					selectedEntities,
					viewProjection,
					sceneFBOHandle,
					glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)  // Orange outline
				);
			}

			// Collider outlines
			bool show2D = m_SettingsPanel.GetShowPhysicsColliders();
			bool show3D = m_SettingsPanel.GetShowPhysics3DColliders();
			if (show2D || show3D) {
				outlineRenderer.RenderColliderOutlines(
					m_ActiveScene.get(),
					viewProjection,
					sceneFBOHandle,
					show3D, show2D,
					glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),  // Green for 3D
					glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)    // Red for 2D
				);
			}

			// Re-bind the main scene FBO after outline passes
			m_Framebuffer->Bind();
		}

		// ========================================
		// LEGACY 2D OVERLAY (Camera frustum, light gizmos, etc.)
		// These still use Renderer2D line drawing for non-outlined gizmos.
		// ========================================

		// Draw 2D Physics Colliders (Red wireframe lines — fallback if OutlineRenderer not available)
		if (!outlineRenderer.IsInitialized() && m_SettingsPanel.GetShowPhysicsColliders()) {
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider2DComponent>();
				for (auto entityID : view) {
					auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entityID);

					glm::vec3 translation = tc.Translation + glm::vec3(bc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawRect(transform, glm::vec4(1, 0, 0, 1));
				}
			}
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CircleCollider2DComponent>();
				for (auto entityID : view) {
					auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entityID);

					glm::vec3 translation = tc.Translation + glm::vec3(cc2d.Offset, 0.001f);
					glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawCircle(transform, glm::vec4(1, 0, 0, 1), 0.01f);
				}
			}
		}

		if (!outlineRenderer.IsInitialized() && m_SettingsPanel.GetShowPhysics3DColliders()) {
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, BoxCollider3DComponent>();
				for (auto entityID : view) {
					auto [tc, bc3d] = view.get<TransformComponent, BoxCollider3DComponent>(entityID);

					glm::vec3 translation = tc.Translation + bc3d.Offset;
					glm::vec3 scale = tc.Scale * (bc3d.HalfExtents * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::toMat4(glm::quat(tc.Rotation))
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
				}
			}
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, SphereCollider3DComponent>();
				for (auto entityID : view) {
					auto [tc, sc3d] = view.get<TransformComponent, SphereCollider3DComponent>(entityID);

					glm::vec3 translation = tc.Translation + sc3d.Offset;
					glm::vec3 scale = tc.Scale * glm::vec3(sc3d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawCircle(transform, glm::vec4(0, 1, 0, 1), 0.01f);
				}
			}
			{
				auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, CapsuleCollider3DComponent>();
				for (auto entityID : view) {
					auto [tc, cc3d] = view.get<TransformComponent, CapsuleCollider3DComponent>(entityID);

					glm::vec3 translation = tc.Translation + cc3d.Offset;
					glm::vec3 scale = tc.Scale * glm::vec3(cc3d.Radius * 2.0f, cc3d.Height, cc3d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
						* glm::toMat4(glm::quat(tc.Rotation))
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawRect(transform, glm::vec4(0, 1, 0, 1));
				}
			}
		}

		// Draw selected entity outline (thin wireframe fallback if OutlineRenderer not available)
		if (!outlineRenderer.IsInitialized()) {
			if (Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity()) {
				const TransformComponent& transform = selectedEntity.GetComponent<TransformComponent>();
				Renderer2D::DrawRect(transform.GetTransform(), glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
			}
		}

		Renderer2D::EndScene();
	}

	void EditorLayer::NewScene() {
		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;
		m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_SceneHierarchyPanel.SetContext(m_ActiveScene);
		m_PropertiesPanel.SetContext(m_ActiveScene);
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
			m_PropertiesPanel.SetContext(m_EditorScene);
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
	// ===========================================================================

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
		
		// Update Console Panel with project directory for terminal
		m_ConsolePanel.SetProjectDirectory(project->GetProjectDirectory());

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
		
		// Update Console Panel with project directory for terminal
		m_ConsolePanel.SetProjectDirectory(project->GetProjectDirectory());

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

	void EditorLayer::RenderCameraPreview(Entity cameraEntity) {
		if (!cameraEntity || !cameraEntity.HasComponent<CameraComponent>())
			return;

		auto& cameraComp = cameraEntity.GetComponent<CameraComponent>();
		glm::mat4 cameraWorldTransform = m_ActiveScene->GetWorldTransform(cameraEntity);

		// ========================================
		// CALCULATE PREVIEW SIZE WITH CORRECT ASPECT RATIO
		// ========================================
		// Use the camera's aspect ratio to avoid distortion
		float cameraAspect = cameraComp.Camera.GetAspectRatio();
		if (cameraAspect <= 0.0f) {
			cameraAspect = 16.0f / 9.0f; // Default fallback
		}
		
		// Base preview width, calculate height from camera aspect ratio
		uint32_t previewWidth = 320;
		uint32_t previewHeight = static_cast<uint32_t>(previewWidth / cameraAspect);
		
		// Clamp height to reasonable bounds
		previewHeight = std::clamp(previewHeight, 100u, 400u);

		auto previewSpec = m_CameraPreviewFramebuffer->GetSpecification();
		if (previewSpec.Width != previewWidth || previewSpec.Height != previewHeight) {
			m_CameraPreviewFramebuffer->Resize(previewWidth, previewHeight);
		}

		// ========================================
		// SAVE CURRENT VIEWPORT STATE
		// ========================================
		int currentViewport[4];
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->GetViewport(currentViewport);
		}

		// ========================================
		// RENDER CAMERA PREVIEW (Isolated)
		// ========================================
		m_CameraPreviewFramebuffer->Bind();
		if (cmdList) {
			cmdList->SetViewport(0.0f, 0.0f, static_cast<float>(previewWidth), static_cast<float>(previewHeight));
			cmdList->SetClearColor({ 0.15f, 0.15f, 0.18f, 1.0f });
			cmdList->Clear();
		}

		// ========================================
		// RENDER SKYBOX FIRST (before geometry)
		// ========================================
		SkyboxRenderer::RenderGlobalSkybox(cameraComp.Camera, cameraWorldTransform);

		// ========================================
		// RENDER 3D MESHES (NO GRID, NO BILLBOARDS)
		// ========================================
		Renderer3D::BeginScene(cameraComp.Camera, cameraWorldTransform);
		Renderer3D::UpdateLights(m_ActiveScene.get());

		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
			for (auto entityID : view) {
				Entity e = { entityID, m_ActiveScene.get() };
				auto& mesh = e.GetComponent<MeshComponent>();
				glm::mat4 worldTransform = m_ActiveScene->GetWorldTransform(e);

				if (e.HasComponent<MaterialComponent>()) {
					auto& material = e.GetComponent<MaterialComponent>();
					Renderer3D::DrawMesh(worldTransform, mesh, material, -1);
				}
				else {
					Renderer3D::DrawModel(worldTransform, mesh.MeshModel, mesh.Color, -1);
				}
			}
		}

		Renderer3D::EndScene();

		// ========================================
		// RENDER 2D SPRITES (NO GRID, NO BILLBOARDS)
		// ========================================
		Renderer2D::BeginScene(cameraComp.Camera, cameraWorldTransform);

		{
			auto view = m_ActiveScene->GetAllEntitiesWith<TransformComponent, SpriteRendererComponent>();
			for (auto entityID : view) {
				Entity e = { entityID, m_ActiveScene.get() };
				auto& sprite = e.GetComponent<SpriteRendererComponent>();
				glm::mat4 worldTransform = m_ActiveScene->GetWorldTransform(e);
				Renderer2D::DrawSprite(worldTransform, sprite, -1);
			}
		}

		Renderer2D::EndScene();

		// ========================================
		// RESTORE MAIN VIEWPORT STATE
		// ========================================
		m_CameraPreviewFramebuffer->Unbind();
		if (cmdList) {
			cmdList->SetViewport(
				static_cast<float>(currentViewport[0]), 
				static_cast<float>(currentViewport[1]),
				static_cast<float>(currentViewport[2]), 
				static_cast<float>(currentViewport[3])
			);
		}

		// ========================================
		// CRITICAL: RESTORE MAIN CAMERA (Editor or Runtime)
		// ========================================
		if (m_SceneState == SceneState::Edit) {
			Renderer2D::BeginScene(m_EditorCamera);
			Renderer2D::EndScene();

			Renderer3D::BeginScene(m_EditorCamera);
			Renderer3D::EndScene();
		}
		else if (m_SceneState == SceneState::Play) {
			Entity camera = m_ActiveScene->GetPrimaryCameraEntity();
			if (camera) {
				auto& cameraComponent = camera.GetComponent<CameraComponent>();
				glm::mat4 cameraTransform = camera.GetComponent<TransformComponent>().GetTransform();

				Renderer2D::BeginScene(cameraComponent.Camera, cameraTransform);
				Renderer2D::EndScene();

				Renderer3D::BeginScene(cameraComponent.Camera, cameraTransform);
				Renderer3D::EndScene();
			}
		}
	}

	// ============================================================================
	// MESH IMPORT HANDLERS
	// ============================================================================

	void EditorLayer::OnModelDropped(const std::filesystem::path& modelPath) {
		if (!MeshImporter::IsSupported(modelPath)) {
			LNX_LOG_WARN("Unsupported model format: {0}", modelPath.extension().string());
			m_ConsolePanel.AddLog("Unsupported model format: " + modelPath.extension().string(), LogLevel::Warning, "Import");
			return;
		}
		
		// Get the assets directory for output
		auto project = ProjectManager::GetActiveProject();
		std::filesystem::path outputDir = project ? project->GetAssetDirectory() : g_AssetPath;
		
		// Open the import modal
		m_MeshImportModal.Open(modelPath, outputDir);
		
		LNX_LOG_INFO("Opening mesh import modal for: {0}", modelPath.filename().string());
	}

	void EditorLayer::OnMeshAssetDropped(const std::filesystem::path& meshAssetPath) {
		if (m_SceneState != SceneState::Edit) {
			LNX_LOG_WARN("Cannot create entities while playing");
			return;
		}
		
		// Load the MeshAsset
		Ref<MeshAsset> meshAsset = MeshAsset::LoadFromFile(meshAssetPath);
		if (!meshAsset) {
			LNX_LOG_ERROR("Failed to load MeshAsset: {0}", meshAssetPath.string());
			m_ConsolePanel.AddLog("Failed to load mesh: " + meshAssetPath.filename().string(), LogLevel::Error, "Asset");
			return;
		}
		
		// Create entity with the mesh
		OnMeshImported(meshAsset);
	}

	void EditorLayer::OnMeshImported(Ref<MeshAsset> meshAsset) {
		if (!meshAsset || m_SceneState != SceneState::Edit) {
			return;
		}
		
		// Create a new entity with the mesh
		std::string entityName = meshAsset->GetName();
		if (entityName.empty()) {
			entityName = "Mesh";
		}
		
		Entity newEntity = m_ActiveScene->CreateEntity(entityName);
		
		// Add MeshComponent and set the MeshAsset
		auto& meshComp = newEntity.AddComponent<MeshComponent>();
		meshComp.SetMeshAsset(meshAsset);
		
		// MaterialComponent is added automatically by MeshComponent
		// but we ensure it exists
		if (!newEntity.HasComponent<MaterialComponent>()) {
			newEntity.AddComponent<MaterialComponent>();
		}
		
		// Select the new entity
		m_SceneHierarchyPanel.SetSelectedEntity(newEntity);
		
		LNX_LOG_INFO("Created entity '{0}' with MeshAsset", entityName);
		m_ConsolePanel.AddLog("Created mesh entity: " + entityName, LogLevel::Info, "Scene");
	}
	
	void EditorLayer::OnPrefabDropped(const std::filesystem::path& prefabPath) {
		if (m_SceneState != SceneState::Edit) {
			LNX_LOG_WARN("Cannot instantiate prefabs while playing");
			return;
		}
		
		// Load the prefab
		Ref<Prefab> prefab = Prefab::LoadFromFile(prefabPath);
		if (!prefab) {
			LNX_LOG_ERROR("Failed to load prefab: {0}", prefabPath.string());
			m_ConsolePanel.AddLog("Failed to load prefab: " + prefabPath.filename().string(), LogLevel::Error, "Prefab");
			return;
		}
		
		// Instantiate the prefab at origin
		Entity rootEntity = prefab->Instantiate(m_ActiveScene, glm::vec3(0.0f));
		if (!rootEntity) {
			LNX_LOG_ERROR("Failed to instantiate prefab: {0}", prefabPath.string());
			m_ConsolePanel.AddLog("Failed to instantiate prefab: " + prefabPath.filename().string(), LogLevel::Error, "Prefab");
			return;
		}
		
		// Select the new entity
		m_SceneHierarchyPanel.SetSelectedEntity(rootEntity);
		
		std::string prefabName = prefab->GetName();
		LNX_LOG_INFO("Instantiated prefab '{0}' with {1} entities", prefabName, prefab->GetEntityCount());
		m_ConsolePanel.AddLog("Instantiated prefab: " + prefabName, LogLevel::Info, "Scene");
	}
}