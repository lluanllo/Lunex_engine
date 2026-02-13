#include "stpch.h"
#include "RayTracingScene.h"
#include "Log/Log.h"

namespace Lunex {

	void RayTracingScene::Initialize() {
		m_GeomPool.Initialize();
		m_MatTable.Initialize();
		m_TexAtlas.Initialize();
	}

	void RayTracingScene::Shutdown() {
		m_GeomPool.Shutdown();
		m_MatTable.Shutdown();
		m_TexAtlas.Shutdown();
	}

	void RayTracingScene::Rebuild(const SceneRenderData& data) {
		m_MatTable.Clear();
		m_TexAtlas.Clear();

		auto result = m_GeomPool.Build(
			data.DrawItems,
			[this](const Ref<MaterialInstance>& mat) -> uint32_t {
				return m_MatTable.GetOrAddMaterial(mat, &m_TexAtlas);
			}
		);

		m_TexAtlas.UploadToGPU();
		m_MatTable.UploadToGPU();
		m_Dirty = false;

		LNX_LOG_TRACE("RayTracingScene rebuilt: {0} tris, {1} BVH nodes, {2} materials, {3} textures ({4:.2f}ms)",
		              result.TriangleCount, result.BVHNodeCount,
		              m_MatTable.GetMaterialCount(), m_TexAtlas.GetTextureCount(),
		              result.BuildTimeMs);
	}

	void RayTracingScene::Bind(uint32_t triBinding, uint32_t bvhBinding,
	                            uint32_t matBinding, uint32_t texBinding) const {
		m_GeomPool.BindForRayTracing(triBinding, bvhBinding);
		m_MatTable.Bind(matBinding);
		m_TexAtlas.Bind(texBinding);
	}

} // namespace Lunex
