#pragma once

/**
 * @file RayTracingScene.h
 * @brief GPU-side scene representation for the ray-tracing backend
 *
 * Owns the MeshGeometryPool (triangles + BVH) and MaterialGPUTable.
 * Rebuilds when the scene changes (dirty flag).
 */

#include "Rendering/MeshGeometryPool.h"
#include "Rendering/MaterialGPUTable.h"
#include "Rendering/SceneRenderData.h"

namespace Lunex {

	class RayTracingScene {
	public:
		RayTracingScene() = default;
		~RayTracingScene() = default;

		void Initialize();
		void Shutdown();

		/** Rebuild from a fresh SceneRenderData snapshot. */
		void Rebuild(const SceneRenderData& data);

		/** Bind all SSBOs for the path-tracer compute shader. */
		void Bind(uint32_t triBinding, uint32_t bvhBinding, uint32_t matBinding) const;

		void MarkDirty()       { m_Dirty = true; }
		bool IsDirty() const   { return m_Dirty; }

		uint32_t GetTriangleCount() const { return m_GeomPool.GetTriangleCount(); }
		uint32_t GetBVHNodeCount()  const { return m_GeomPool.GetBVHNodeCount(); }
		uint32_t GetMaterialCount() const { return m_MatTable.GetMaterialCount(); }

	private:
		MeshGeometryPool m_GeomPool;
		MaterialGPUTable m_MatTable;
		bool             m_Dirty = true;
	};

} // namespace Lunex
