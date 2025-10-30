#include "stpch.h"
#include "BasicShapes.h"

#include <glm/gtc/constants.hpp>

namespace Lunex {

	Ref<Mesh> BasicShapes::CreateCube() {
		return Mesh::CreateCube();
	}

	Ref<Mesh> BasicShapes::CreateSphere(uint32_t segments, uint32_t rings) {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> indices;

		const float radius = 0.5f;

		// Generate vertices
		for (uint32_t ring = 0; ring <= rings; ring++) {
			float theta = ring * glm::pi<float>() / rings;
			float sinTheta = glm::sin(theta);
			float cosTheta = glm::cos(theta);

			for (uint32_t segment = 0; segment <= segments; segment++) {
				float phi = segment * 2.0f * glm::pi<float>() / segments;
				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				Vertex3D vertex;
				vertex.Position = glm::vec3(
					radius * sinTheta * cosPhi,
					radius * cosTheta,
					radius * sinTheta * sinPhi
				);
				vertex.Normal = glm::normalize(vertex.Position);
				vertex.TexCoord = glm::vec2((float)segment / segments, (float)ring / rings);
				vertex.Tangent = glm::vec3(-sinPhi, 0.0f, cosPhi);
				vertex.Bitangent = glm::cross(vertex.Normal, vertex.Tangent);

				vertices.push_back(vertex);
			}
		}

		// Generate indices
		for (uint32_t ring = 0; ring < rings; ring++) {
			for (uint32_t segment = 0; segment < segments; segment++) {
				uint32_t first = (ring * (segments + 1)) + segment;
				uint32_t second = first + segments + 1;

				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);

				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> BasicShapes::CreatePlane(float width, float height) {
		std::vector<Vertex3D> vertices = {
			{ {-width * 0.5f, 0.0f, -height * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
			{ { width * 0.5f, 0.0f, -height * 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
			{ { width * 0.5f, 0.0f,  height * 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
			{ {-width * 0.5f, 0.0f,  height * 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} }
		};

		std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> BasicShapes::CreateCylinder(uint32_t segments, float height, float radius) {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> indices;

		float halfHeight = height * 0.5f;

		// Generate side vertices
		for (uint32_t i = 0; i <= segments; i++) {
			float angle = (float)i / segments * 2.0f * glm::pi<float>();
			float x = glm::cos(angle) * radius;
			float z = glm::sin(angle) * radius;

			glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
			glm::vec2 texCoord = glm::vec2((float)i / segments, 0.0f);

			// Bottom vertex
			vertices.push_back({
				glm::vec3(x, -halfHeight, z),
				normal,
				texCoord,
				glm::vec3(-glm::sin(angle), 0.0f, glm::cos(angle)),
				glm::vec3(0.0f, 1.0f, 0.0f)
			});

			// Top vertex
			vertices.push_back({
				glm::vec3(x, halfHeight, z),
				normal,
				glm::vec2(texCoord.x, 1.0f),
				glm::vec3(-glm::sin(angle), 0.0f, glm::cos(angle)),
				glm::vec3(0.0f, 1.0f, 0.0f)
			});
		}

		// Generate side indices
		for (uint32_t i = 0; i < segments; i++) {
			uint32_t base = i * 2;
			indices.push_back(base);
			indices.push_back(base + 2);
			indices.push_back(base + 1);

			indices.push_back(base + 1);
			indices.push_back(base + 2);
			indices.push_back(base + 3);
		}

		// Add caps
		uint32_t baseIndex = vertices.size();

		// Bottom cap
		vertices.push_back({ glm::vec3(0.0f, -halfHeight, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
		for (uint32_t i = 0; i <= segments; i++) {
			float angle = (float)i / segments * 2.0f * glm::pi<float>();
			vertices.push_back({
				glm::vec3(glm::cos(angle) * radius, -halfHeight, glm::sin(angle) * radius),
				glm::vec3(0.0f, -1.0f, 0.0f),
				glm::vec2(0.5f + glm::cos(angle) * 0.5f, 0.5f + glm::sin(angle) * 0.5f),
				glm::vec3(1.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			});
		}

		// Bottom cap indices
		for (uint32_t i = 0; i < segments; i++) {
			indices.push_back(baseIndex);
			indices.push_back(baseIndex + i + 2);
			indices.push_back(baseIndex + i + 1);
		}

		// Top cap
		baseIndex = vertices.size();
		vertices.push_back({ glm::vec3(0.0f, halfHeight, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });
		for (uint32_t i = 0; i <= segments; i++) {
			float angle = (float)i / segments * 2.0f * glm::pi<float>();
			vertices.push_back({
				glm::vec3(glm::cos(angle) * radius, halfHeight, glm::sin(angle) * radius),
				glm::vec3(0.0f, 1.0f, 0.0f),
				glm::vec2(0.5f + glm::cos(angle) * 0.5f, 0.5f + glm::sin(angle) * 0.5f),
				glm::vec3(1.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			});
		}

		// Top cap indices
		for (uint32_t i = 0; i < segments; i++) {
			indices.push_back(baseIndex);
			indices.push_back(baseIndex + i + 1);
			indices.push_back(baseIndex + i + 2);
		}

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> BasicShapes::CreateCone(uint32_t segments, float height, float radius) {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> indices;

		float halfHeight = height * 0.5f;

		// Apex
		vertices.push_back({ glm::vec3(0.0f, halfHeight, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.5f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });

		// Base vertices
		for (uint32_t i = 0; i <= segments; i++) {
			float angle = (float)i / segments * 2.0f * glm::pi<float>();
			float x = glm::cos(angle) * radius;
			float z = glm::sin(angle) * radius;

			glm::vec3 position(x, -halfHeight, z);
			glm::vec3 toApex = glm::normalize(glm::vec3(0.0f, halfHeight, 0.0f) - position);
			glm::vec3 tangent(-z, 0.0f, x);
			glm::vec3 normal = glm::normalize(glm::cross(tangent, toApex));

			vertices.push_back({
				position,
				normal,
				glm::vec2((float)i / segments, 0.0f),
				glm::normalize(tangent),
				glm::cross(normal, glm::normalize(tangent))
			});
		}

		// Side indices
		for (uint32_t i = 1; i <= segments; i++) {
			indices.push_back(0);
			indices.push_back(i);
			indices.push_back(i + 1);
		}

		// Base cap
		uint32_t baseCenter = vertices.size();
		vertices.push_back({ glm::vec3(0.0f, -halfHeight, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) });

		for (uint32_t i = 0; i <= segments; i++) {
			float angle = (float)i / segments * 2.0f * glm::pi<float>();
			vertices.push_back({
				glm::vec3(glm::cos(angle) * radius, -halfHeight, glm::sin(angle) * radius),
				glm::vec3(0.0f, -1.0f, 0.0f),
				glm::vec2(0.5f + glm::cos(angle) * 0.5f, 0.5f + glm::sin(angle) * 0.5f),
				glm::vec3(1.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, 1.0f)
			});
		}

		// Base cap indices
		for (uint32_t i = 0; i < segments; i++) {
			indices.push_back(baseCenter);
			indices.push_back(baseCenter + i + 2);
			indices.push_back(baseCenter + i + 1);
		}

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> BasicShapes::CreateTorus(uint32_t segments, uint32_t sides, float majorRadius, float minorRadius) {
		std::vector<Vertex3D> vertices;
		std::vector<uint32_t> indices;

		for (uint32_t i = 0; i <= segments; i++) {
			float u = (float)i / segments * 2.0f * glm::pi<float>();
			
			for (uint32_t j = 0; j <= sides; j++) {
				float v = (float)j / sides * 2.0f * glm::pi<float>();

				float x = (majorRadius + minorRadius * glm::cos(v)) * glm::cos(u);
				float y = minorRadius * glm::sin(v);
				float z = (majorRadius + minorRadius * glm::cos(v)) * glm::sin(u);

				glm::vec3 position(x, y, z);
				glm::vec3 center(majorRadius * glm::cos(u), 0.0f, majorRadius * glm::sin(u));
				glm::vec3 normal = glm::normalize(position - center);

				vertices.push_back({
					position,
					normal,
					glm::vec2((float)i / segments, (float)j / sides),
					glm::vec3(-glm::sin(u), 0.0f, glm::cos(u)),
					glm::cross(normal, glm::vec3(-glm::sin(u), 0.0f, glm::cos(u)))
				});
			}
		}

		// Generate indices
		for (uint32_t i = 0; i < segments; i++) {
			for (uint32_t j = 0; j < sides; j++) {
				uint32_t first = (i * (sides + 1)) + j;
				uint32_t second = first + sides + 1;

				indices.push_back(first);
				indices.push_back(second);
				indices.push_back(first + 1);

				indices.push_back(second);
				indices.push_back(second + 1);
				indices.push_back(first + 1);
			}
		}

		return CreateRef<Mesh>(vertices, indices);
	}

	Ref<Mesh> BasicShapes::CreateQuad() {
		return Mesh::CreateQuad();
	}

	Ref<Mesh> BasicShapes::CreateShape(ShapeType type) {
		switch (type) {
			case ShapeType::Cube:     return CreateCube();
			case ShapeType::Sphere:   return CreateSphere();
			case ShapeType::Plane:    return CreatePlane();
			case ShapeType::Cylinder: return CreateCylinder();
			case ShapeType::Cone:     return CreateCone();
			case ShapeType::Torus:    return CreateTorus();
			case ShapeType::Quad:  return CreateQuad();
		}
		return CreateCube();
	}

	const char* BasicShapes::GetShapeName(ShapeType type) {
		switch (type) {
			case ShapeType::Cube:     return "Cube";
			case ShapeType::Sphere:   return "Sphere";
			case ShapeType::Plane:    return "Plane";
			case ShapeType::Cylinder: return "Cylinder";
			case ShapeType::Cone:     return "Cone";
			case ShapeType::Torus:    return "Torus";
			case ShapeType::Quad:   return "Quad";
		}
		return "Unknown";
	}

}
