//---Lunex Core----------
#include "Application.h"
#include "Layer.h"
//--------------------------

//---Lunex Timestep--------
#include "Timestep.h"
//--------------------------

//---Lunex Codes---------
#include "Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseButtonCodes.h"
//--------------------------

//---Lunex ImGui---------
#include "ImGui/ImGuiLayer.h"
//--------------------------

//---Lunex Scene---------
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/ScriptableEntity.h"
#include "Scene/Components.h"
#include "Scene/SceneManager.h"
//-----------------------

//---Lunex Physics-------
#include "Physics/Physics.h"
//--------------------------

//---Lunex Renderer------
#include "Renderer/Renderer.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Renderer3D.h"
#include "RHI/RHI.h"
#include "Renderer/Buffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Texture.h"
#include "Renderer/VertexArray.h"
#include "Scene/Camera/OrthographicCamera.h"
#include "Scene/Camera/OrthographicCameraController.h"
//--------------------------

//===== AAA ARCHITECTURE =====
// These are the new, properly layered systems.
// Legacy includes above still work via compatibility wrappers.

//---AAA Scene System---
#include "Scene/Core/SceneMode.h"
#include "Scene/Core/SceneContext.h"
#include "Scene/Core/SceneEvents.h"
#include "Scene/Core/ISceneSystem.h"
//-----------------------

//---AAA Camera System---
#include "Scene/Camera/CameraData.h"
#include "Scene/Camera/CameraSystem.h"
//-----------------------

//---AAA Lighting System---
#include "Scene/Lighting/LightTypes.h"
#include "Scene/Lighting/LightSystem.h"
//--------------------------

//---AAA Render System---
#include "Rendering/RenderSystem.h"
//------------------------

//===== UNIFIED ASSET SYSTEM =====
// New unified asset architecture with JobSystem integration

//---Unified Assets Core---
#include "Assets/Core/AssetCore.h"
//-------------------------

//---Unified Assets Types---
#include "Assets/Materials/MaterialAsset.h"
#include "Assets/Mesh/MeshAsset.h"
// Note: TextureAsset, Prefab - coming soon
//---------------------------