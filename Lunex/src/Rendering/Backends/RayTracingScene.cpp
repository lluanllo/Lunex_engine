#include "stpch.h"
#include "RayTracingScene.h"

#include "Rendering/SceneRenderData.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "RHI/RHIDevice.h"
#include "Log/Log.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <numeric>
#include <stack>

namespace Lunex {

	// ============================================================================
	// SCENE DATA COLLECTION
	// ============================================================================

	void RayTracingSceneData::CollectTriangles(Scene* scene) {
		m_BuildTriangles.clear();

		if (!scene) return;

		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		int materialBase = 0;

		for (auto entityID : view) {
			Entity entity = { entityID, scene };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& meshComp  = entity.GetComponent<MeshComponent>();

			if (!meshComp.MeshModel) continue;

			glm::mat4 modelMatrix = transform.GetTransform();
			glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));

			for (auto& mesh : meshComp.MeshModel->GetMeshes()) {
				auto& vertices = mesh->GetVertices();
				auto& indices  = mesh->GetIndices();

				for (size_t i = 0; i + 2 < indices.size(); i += 3) {
					auto& v0 = vertices[indices[i]];
					auto& v1 = vertices[indices[i + 1]];
					auto& v2 = vertices[indices[i + 2]];

					BVHBuildTriangle tri;
					tri.V0 = glm::vec3(modelMatrix * glm::vec4(v0.Position, 1.0f));
					tri.V1 = glm::vec3(modelMatrix * glm::vec4(v1.Position, 1.0f));
					tri.V2 = glm::vec3(modelMatrix * glm::vec4(v2.Position, 1.0f));
					tri.N0 = glm::normalize(normalMatrix * v0.Normal);
					tri.N1 = glm::normalize(normalMatrix * v1.Normal);
					tri.N2 = glm::normalize(normalMatrix * v2.Normal);
					tri.UV0 = v0.TexCoords;
					tri.UV1 = v1.TexCoords;
					tri.MaterialIndex = materialBase;

					m_BuildTriangles.push_back(tri);
				}
			}

			materialBase++;
		}
	}

	void RayTracingSceneData::CollectMaterials(Scene* scene) {
		m_GPUMaterials.clear();

		if (!scene) return;

		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();

		for (auto entityID : view) {
			Entity entity = { entityID, scene };

			GPUMaterial mat{};
			mat.Albedo = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
			mat.EmissionAndMetallic = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			mat.RoughnessSpecularPad = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
			mat.Padding = glm::vec4(0.0f);

			if (entity.HasComponent<MaterialComponent>()) {
				auto& matComp = entity.GetComponent<MaterialComponent>();
				glm::vec4 albedo = matComp.GetAlbedo();
				mat.Albedo = albedo;
				mat.EmissionAndMetallic.x = matComp.GetEmissionColor().x;
				mat.EmissionAndMetallic.y = matComp.GetEmissionColor().y;
				mat.EmissionAndMetallic.z = matComp.GetEmissionColor().z;
				mat.EmissionAndMetallic.w = matComp.GetMetallic();
				mat.RoughnessSpecularPad.x = matComp.GetRoughness();
				mat.RoughnessSpecularPad.y = matComp.GetSpecular();
			}

			m_GPUMaterials.push_back(mat);
		}

		// Ensure at least one default material
		if (m_GPUMaterials.empty()) {
			GPUMaterial def{};
			def.Albedo = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
			def.RoughnessSpecularPad = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
			m_GPUMaterials.push_back(def);
		}
	}

	void RayTracingSceneData::CollectLights(const SceneRenderData& sceneData) {
		m_GPULights.clear();

		for (const auto& light : sceneData.Lighting.Lights) {
			GPURTLight l{};
			int lightType = static_cast<int>(light.Position.w);
			l.PositionAndType = glm::vec4(light.Position.x, light.Position.y, light.Position.z, (float)lightType);
			l.DirectionAndRange = glm::vec4(light.Direction.x, light.Direction.y, light.Direction.z, light.Direction.w);
			l.ColorAndIntensity = glm::vec4(light.Color.x, light.Color.y, light.Color.z, light.Color.a);
			l.Params = glm::vec4(light.Params.x, light.Params.y, light.Params.z, 0.0f);
			m_GPULights.push_back(l);
		}
	}

	// ============================================================================
	// BVH CONSTRUCTION (SAH-based)
	// ============================================================================

	void RayTracingSceneData::BuildBVH() {
		const size_t N = m_BuildTriangles.size();
		if (N == 0) return;

		// Convert build tris -> GPU tris (pre-transform already done)
		m_GPUTriangles.resize(N);
		for (size_t i = 0; i < N; i++) {
			auto& src = m_BuildTriangles[i];
			auto& dst = m_GPUTriangles[i];
			dst.V0 = glm::vec4(src.V0, src.UV0.x);
			dst.V1 = glm::vec4(src.V1, src.UV0.y);
			dst.V2 = glm::vec4(src.V2, src.UV1.x);
			dst.N0 = glm::vec4(src.N0, src.UV1.y);
			// Pack N1, N2 and materialIndex
			dst.N1N2MatID = glm::vec4(src.N1.x, src.N1.y,
				glm::uintBitsToFloat(
					(glm::floatBitsToUint(src.N1.z) & 0xFFFF0000u) |
					(glm::floatBitsToUint(src.N2.x) >> 16u)),
				glm::intBitsToFloat(src.MaterialIndex));
		}

		// Simple top-down SAH BVH
		struct BVHBuildNode {
			AABB Bounds;
			int32_t Left = -1, Right = -1;
			int32_t FirstTri = 0, TriCount = 0;
		};

		std::vector<BVHBuildNode> buildNodes;
		buildNodes.reserve(2 * N);

		// Triangle indices (will be reordered)
		std::vector<uint32_t> triIndices(N);
		std::iota(triIndices.begin(), triIndices.end(), 0u);

		// Centroids
		std::vector<glm::vec3> centroids(N);
		for (size_t i = 0; i < N; i++)
			centroids[i] = m_BuildTriangles[i].Centroid();

		struct BuildTask {
			int32_t NodeIndex;
			uint32_t Start, End;
		};

		// Create root
		BVHBuildNode root;
		root.FirstTri = 0;
		root.TriCount = (int32_t)N;
		for (size_t i = 0; i < N; i++) {
			auto& t = m_BuildTriangles[i];
			root.Bounds.Grow(t.V0);
			root.Bounds.Grow(t.V1);
			root.Bounds.Grow(t.V2);
		}
		buildNodes.push_back(root);

		std::stack<BuildTask> taskStack;
		taskStack.push({ 0, 0, (uint32_t)N });

		constexpr int MAX_LEAF_SIZE = 4;
		constexpr int SAH_BINS = 12;

		while (!taskStack.empty()) {
			auto task = taskStack.top();
			taskStack.pop();

			auto& node = buildNodes[task.NodeIndex];
			uint32_t count = task.End - task.Start;

			if (count <= MAX_LEAF_SIZE) {
				node.FirstTri = (int32_t)task.Start;
				node.TriCount = (int32_t)count;
				continue;
			}

			// Find best split axis using SAH
			float bestCost = std::numeric_limits<float>::max();
			int bestAxis = 0;
			int bestBin = 0;

			AABB centroidBounds;
			for (uint32_t i = task.Start; i < task.End; i++)
				centroidBounds.Grow(centroids[triIndices[i]]);

			glm::vec3 extent = centroidBounds.Max - centroidBounds.Min;

			for (int axis = 0; axis < 3; axis++) {
				if (extent[axis] < 1e-6f) continue;

				struct Bin {
					AABB bounds;
					int count = 0;
				};
				Bin bins[SAH_BINS];

				float scale = (float)SAH_BINS / extent[axis];

				for (uint32_t i = task.Start; i < task.End; i++) {
					int binIdx = std::min((int)((centroids[triIndices[i]][axis] - centroidBounds.Min[axis]) * scale), SAH_BINS - 1);
					bins[binIdx].count++;
					auto& t = m_BuildTriangles[triIndices[i]];
					bins[binIdx].bounds.Grow(t.V0);
					bins[binIdx].bounds.Grow(t.V1);
					bins[binIdx].bounds.Grow(t.V2);
				}

				// Evaluate SAH for each split
				float leftArea[SAH_BINS - 1], rightArea[SAH_BINS - 1];
				int leftCount[SAH_BINS - 1], rightCount[SAH_BINS - 1];

				AABB leftBox, rightBox;
				int leftN = 0, rightN = 0;

				for (int i = 0; i < SAH_BINS - 1; i++) {
					leftN += bins[i].count;
					leftBox.Grow(bins[i].bounds);
					leftCount[i] = leftN;
					leftArea[i] = leftBox.SurfaceArea();
				}

				for (int i = SAH_BINS - 1; i > 0; i--) {
					rightN += bins[i].count;
					rightBox.Grow(bins[i].bounds);
					rightCount[i - 1] = rightN;
					rightArea[i - 1] = rightBox.SurfaceArea();
				}

				float parentArea = node.Bounds.SurfaceArea();
				for (int i = 0; i < SAH_BINS - 1; i++) {
					float cost = 1.0f + (leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i]) / parentArea;
					if (cost < bestCost) {
						bestCost = cost;
						bestAxis = axis;
						bestBin = i;
					}
				}
			}

			// If SAH says leaf is cheaper, make a leaf
			float leafCost = (float)count;
			if (bestCost >= leafCost && count <= 16) {
				node.FirstTri = (int32_t)task.Start;
				node.TriCount = (int32_t)count;
				continue;
			}

			// Partition
			float scale = (float)SAH_BINS / extent[bestAxis];
			auto mid = std::partition(
				triIndices.begin() + task.Start,
				triIndices.begin() + task.End,
				[&](uint32_t idx) {
					int binIdx = std::min(
						(int)((centroids[idx][bestAxis] - centroidBounds.Min[bestAxis]) * scale),
						SAH_BINS - 1);
					return binIdx <= bestBin;
				});

			uint32_t splitIdx = (uint32_t)(mid - triIndices.begin());
			if (splitIdx == task.Start || splitIdx == task.End) {
				splitIdx = (task.Start + task.End) / 2;
			}

			// Create children
			BVHBuildNode leftChild, rightChild;
			for (uint32_t i = task.Start; i < splitIdx; i++) {
				auto& t = m_BuildTriangles[triIndices[i]];
				leftChild.Bounds.Grow(t.V0);
				leftChild.Bounds.Grow(t.V1);
				leftChild.Bounds.Grow(t.V2);
			}
			for (uint32_t i = splitIdx; i < task.End; i++) {
				auto& t = m_BuildTriangles[triIndices[i]];
				rightChild.Bounds.Grow(t.V0);
				rightChild.Bounds.Grow(t.V1);
				rightChild.Bounds.Grow(t.V2);
			}

			int leftIdx = (int)buildNodes.size();
			buildNodes.push_back(leftChild);
			int rightIdx = (int)buildNodes.size();
			buildNodes.push_back(rightChild);

			node.Left = leftIdx;
			node.Right = rightIdx;
			node.TriCount = 0; // inner node
			// Note: after push_back, node reference may be invalidated, but we
			// already captured task.NodeIndex and will only touch buildNodes by index

			buildNodes[task.NodeIndex].Left = leftIdx;
			buildNodes[task.NodeIndex].Right = rightIdx;
			buildNodes[task.NodeIndex].TriCount = 0;

			taskStack.push({ leftIdx, task.Start, splitIdx });
			taskStack.push({ rightIdx, splitIdx, task.End });
		}

		// Reorder GPU triangles to match triIndices
		std::vector<GPUTriangle> reordered(N);
		for (size_t i = 0; i < N; i++)
			reordered[i] = m_GPUTriangles[triIndices[i]];
		m_GPUTriangles = std::move(reordered);

		// Convert build nodes -> GPU nodes
		m_GPUNodes.resize(buildNodes.size());
		for (size_t i = 0; i < buildNodes.size(); i++) {
			auto& src = buildNodes[i];
			auto& dst = m_GPUNodes[i];
			dst.AABBMin = src.Bounds.Min;
			dst.AABBMax = src.Bounds.Max;
			if (src.TriCount > 0) {
				// Leaf
				dst.LeftOrFirst = src.FirstTri;
				dst.TriCount = src.TriCount;
			} else {
				// Inner
				dst.LeftOrFirst = src.Left;
				dst.TriCount = 0;
			}
		}
	}

	// ============================================================================
	// GPU UPLOAD
	// ============================================================================

	void RayTracingSceneData::UploadToGPU() {
		auto* device = RHI::RHIDevice::Get();
		if (!device) return;

		auto createSSBO = [&](uint64_t size, const void* data) -> Ref<RHI::RHIStorageBuffer> {
			RHI::BufferCreateInfo info;
			info.Type = RHI::BufferType::Storage;
			info.Usage = RHI::BufferUsage::Dynamic;
			info.Size = size;
			info.InitialData = data;
			auto buf = device->CreateBuffer(info);
			return std::dynamic_pointer_cast<RHI::RHIStorageBuffer>(buf);
		};

		auto createUBO = [&](uint64_t size, const void* data) -> Ref<RHI::RHIUniformBuffer> {
			RHI::BufferCreateInfo info;
			info.Type = RHI::BufferType::Uniform;
			info.Usage = RHI::BufferUsage::Dynamic;
			info.Size = size;
			info.InitialData = data;
			auto buf = device->CreateBuffer(info);
			return std::dynamic_pointer_cast<RHI::RHIUniformBuffer>(buf);
		};

		// BVH nodes
		if (!m_GPUNodes.empty()) {
			uint64_t sz = m_GPUNodes.size() * sizeof(GPUBVHNode);
			m_BVHBuffer = createSSBO(sz, m_GPUNodes.data());
		}

		// Triangles
		if (!m_GPUTriangles.empty()) {
			uint64_t sz = m_GPUTriangles.size() * sizeof(GPUTriangle);
			m_TriangleBuffer = createSSBO(sz, m_GPUTriangles.data());
		}

		// Materials
		if (!m_GPUMaterials.empty()) {
			uint64_t sz = m_GPUMaterials.size() * sizeof(GPUMaterial);
			m_MaterialBuffer = createSSBO(sz, m_GPUMaterials.data());
		}

		// Lights
		if (!m_GPULights.empty()) {
			uint64_t sz = m_GPULights.size() * sizeof(GPURTLight);
			m_LightBuffer = createSSBO(sz, m_GPULights.data());
		} else {
			// Ensure a valid (empty) buffer
			GPURTLight dummy{};
			m_LightBuffer = createSSBO(sizeof(GPURTLight), &dummy);
		}

		// Scene info UBO
		m_SceneInfoUBO = createUBO(sizeof(GPURTSceneInfo), &m_SceneInfo);
	}

	// ============================================================================
	// PUBLIC BUILD
	// ============================================================================

	void RayTracingSceneData::Build(const SceneRenderData& sceneData, Scene* scene) {
		LNX_LOG_INFO("[RayTracingScene] Building acceleration structure...");

		CollectTriangles(scene);
		CollectMaterials(scene);
		CollectLights(sceneData);
		BuildBVH();

		m_SceneInfo.NumTriangles = (int32_t)m_GPUTriangles.size();
		m_SceneInfo.NumBVHNodes  = (int32_t)m_GPUNodes.size();
		m_SceneInfo.NumLights    = (int32_t)m_GPULights.size();
		m_SceneInfo.NumMaterials = (int32_t)m_GPUMaterials.size();

		UploadToGPU();

		m_Built = true;
		LNX_LOG_INFO("[RayTracingScene] Built: {0} triangles, {1} BVH nodes, {2} materials, {3} lights",
			m_GPUTriangles.size(), m_GPUNodes.size(), m_GPUMaterials.size(), m_GPULights.size());
	}

	void RayTracingSceneData::UpdatePerFrame(
		const CameraRenderData& camera,
		uint32_t viewportW, uint32_t viewportH,
		uint32_t frameIndex, int maxBounces,
		bool accumulate)
	{
		m_SceneInfo.InverseView       = camera.InverseViewMatrix;
		m_SceneInfo.InverseProjection = camera.InverseProjectionMatrix;
		m_SceneInfo.CameraPositionAndFOV = glm::vec4(camera.Position, camera.FieldOfView);
		m_SceneInfo.ViewportSizeAndFrame = glm::vec4((float)viewportW, (float)viewportH, (float)frameIndex, 1.0f);
		m_SceneInfo.SkyColorTop    = glm::vec4(0.5f, 0.7f, 1.0f, 1.0f);
		m_SceneInfo.SkyColorBottom = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		m_SceneInfo.MaxBounces = maxBounces;
		m_SceneInfo.AccumulationEnabled = accumulate ? 1 : 0;

		if (m_SceneInfoUBO) {
			m_SceneInfoUBO->SetData(&m_SceneInfo, sizeof(GPURTSceneInfo), 0);
		}
	}

} // namespace Lunex
