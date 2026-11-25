#include "stpch.h"
#include "ColliderWireframe.h"

#include "Physics/PhysicsUtils.h"
#include "Resources/Mesh/Model.h"

#include <LinearMath/btConvexHullComputer.h>

#include <unordered_map>
#include <unordered_set>

namespace Lunex::ColliderWireframe {

	// Dense concave meshes would produce hundreds of thousands of lines; keep the
	// overlay readable and cheap.
	static constexpr size_t MaxEdges = 40000;

	static void BuildConvexEdges(const std::vector<glm::vec3>& points, std::vector<glm::vec3>& out) {
		if (points.size() < 4) return;
		btConvexHullComputer hull;
		hull.compute(&points[0].x, sizeof(glm::vec3), static_cast<int>(points.size()), 0.0f, 0.0f);

		for (int i = 0; i < hull.edges.size(); i++) {
			const auto& edge = hull.edges[i];
			int a = edge.getSourceVertex();
			int b = edge.getTargetVertex();
			if (a >= b) continue; // each undirected edge once
			const btVector3& pa = hull.vertices[a];
			const btVector3& pb = hull.vertices[b];
			out.emplace_back(pa.x(), pa.y(), pa.z());
			out.emplace_back(pb.x(), pb.y(), pb.z());
		}
	}

	static void BuildTriangleEdges(const std::vector<glm::vec3>& points, const std::vector<uint32_t>& indices,
		std::vector<glm::vec3>& out)
	{
		std::unordered_set<uint64_t> seen;
		seen.reserve(indices.size());
		auto addEdge = [&](uint32_t a, uint32_t b) {
			if (a > b) std::swap(a, b);
			if (!seen.insert((static_cast<uint64_t>(a) << 32) | b).second) return;
			out.push_back(points[a]);
			out.push_back(points[b]);
		};

		// Stride over triangles if the mesh is too dense, keeping an even sampling
		size_t triCount = indices.size() / 3;
		size_t stride = std::max<size_t>(1, (triCount * 3) / (MaxEdges * 2));
		for (size_t t = 0; t < triCount; t += stride) {
			uint32_t i0 = indices[t * 3], i1 = indices[t * 3 + 1], i2 = indices[t * 3 + 2];
			if (i0 >= points.size() || i1 >= points.size() || i2 >= points.size()) continue;
			addEdge(i0, i1); addEdge(i1, i2); addEdge(i2, i0);
			if (out.size() / 2 >= MaxEdges) break;
		}
	}

	static void Build(const std::vector<glm::vec3>& points, const std::vector<uint32_t>& indices, bool convex,
		std::vector<glm::vec3>& out)
	{
		out.clear();
		if (convex) BuildConvexEdges(points, out);
		else BuildTriangleEdges(points, indices, out);
	}

	const std::vector<glm::vec3>& GetModelEdges(const Ref<Model>& model, bool convex) {
		struct Entry { std::weak_ptr<Model> Owner; std::vector<glm::vec3> Edges; };
		static std::unordered_map<const Model*, Entry> s_Cache[2];
		static const std::vector<glm::vec3> s_Empty;
		if (!model) return s_Empty;

		auto& cache = s_Cache[convex ? 1 : 0];
		auto it = cache.find(model.get());
		// Address reuse after a model was freed must not return stale edges
		if (it != cache.end() && it->second.Owner.lock() == model) return it->second.Edges;

		// Merge submeshes exactly like the physics system does
		std::vector<glm::vec3> points;
		std::vector<uint32_t> indices;
		for (const auto& mesh : model->GetMeshes()) {
			uint32_t offset = static_cast<uint32_t>(points.size());
			for (const auto& v : mesh->GetVertices()) points.push_back(v.Position);
			for (uint32_t idx : mesh->GetIndices()) indices.push_back(idx + offset);
		}

		auto& entry = cache[model.get()];
		entry.Owner = model;
		Build(points, indices, convex, entry.Edges);
		return entry.Edges;
	}

	const std::vector<glm::vec3>& GetCustomEdges(const std::vector<glm::vec3>& vertices,
		const std::vector<uint32_t>& indices, bool convex)
	{
		static std::unordered_map<const void*, std::pair<size_t, std::vector<glm::vec3>>> s_Cache[2];
		auto& entry = s_Cache[convex ? 1 : 0][vertices.data()];
		if (entry.first != vertices.size() + indices.size() * 31 || entry.second.empty()) {
			entry.first = vertices.size() + indices.size() * 31;
			Build(vertices, indices, convex, entry.second);
		}
		return entry.second;
	}

} // namespace Lunex::ColliderWireframe
