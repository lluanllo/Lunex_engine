#pragma once

#include "Core/Core.h"
#include "Mesh.h"

namespace Lunex {

	/**
	 * BasicShapes - Factory para crear primitivas 3D comunes
	 * Proporciona meshes predefinidas listas para usar
	 */
	class BasicShapes {
	public:
		enum class ShapeType {
			Cube,
			Sphere,
			Plane,
			Cylinder,
			Cone,
			Torus,
			Quad
		};

		// Factory methods para crear shapes básicas
		static Ref<Mesh> CreateCube();
		static Ref<Mesh> CreateSphere(uint32_t segments = 32, uint32_t rings = 16);
		static Ref<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f);
		static Ref<Mesh> CreateCylinder(uint32_t segments = 32, float height = 2.0f, float radius = 0.5f);
		static Ref<Mesh> CreateCone(uint32_t segments = 32, float height = 2.0f, float radius = 0.5f);
		static Ref<Mesh> CreateTorus(uint32_t segments = 32, uint32_t sides = 16, float majorRadius = 0.75f, float minorRadius = 0.25f);
		static Ref<Mesh> CreateQuad();

		// Get shape by enum
		static Ref<Mesh> CreateShape(ShapeType type);

		// Get shape name
		static const char* GetShapeName(ShapeType type);
	};

}
