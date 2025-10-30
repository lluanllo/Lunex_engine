#include "stpch.h"
#include "Renderer3D.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	Ref<RendererPipeline3D> Renderer3D::s_Pipeline = nullptr;

	void Renderer3D::Init() {
		LNX_PROFILE_FUNCTION();
		
		s_Pipeline = CreateRef<RendererPipeline3D>();
		s_Pipeline->Init();
	}

	void Renderer3D::Shutdown() {
		LNX_PROFILE_FUNCTION();
		
		if (s_Pipeline) {
			s_Pipeline.reset();
		}
	}

	void Renderer3D::BeginScene(const EditorCamera& camera) {
		LNX_PROFILE_FUNCTION();
		
		if (s_Pipeline) {
			s_Pipeline->BeginScene(camera, glm::mat4(1.0f));
		}
	}

	void Renderer3D::BeginScene(const EditorCamera& camera, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();
		
		if (s_Pipeline) {
			s_Pipeline->BeginScene(camera, transform);
		}
	}

	// ? Nueva sobrecarga para SceneCamera en Runtime
	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {
		LNX_PROFILE_FUNCTION();
		
		if (s_Pipeline) {
			// Extract camera position and view matrix from transform
			glm::mat4 viewMatrix = glm::inverse(transform);
			glm::vec3 cameraPosition = glm::vec3(transform[3]);
			
			s_Pipeline->BeginScene(camera, viewMatrix, cameraPosition);
		}
	}

	void Renderer3D::EndScene() {
		LNX_PROFILE_FUNCTION();
		
		if (s_Pipeline) {
			s_Pipeline->EndScene();
		}
	}

	void Renderer3D::DrawMesh(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform, uint32_t entityID) {
		LNX_PROFILE_FUNCTION();
		
		if (s_Pipeline && mesh && material) {
			s_Pipeline->SubmitMesh(mesh, material, transform, entityID);
		}
	}

	void Renderer3D::DrawCube(const glm::vec3& position, const glm::vec3& scale, const Ref<Material>& material) {
		LNX_PROFILE_FUNCTION();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), scale);

		static Ref<Mesh> cubeMesh = Mesh::CreateCube();
		DrawMesh(cubeMesh, material, transform);
	}

	void Renderer3D::DrawSphere(const glm::vec3& position, float radius, const Ref<Material>& material) {
		LNX_PROFILE_FUNCTION();
		
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), glm::vec3(radius));

		static Ref<Mesh> sphereMesh = Mesh::CreateSphere();
		DrawMesh(sphereMesh, material, transform);
	}

	RendererPipeline3D::Statistics Renderer3D::GetStats() {
		if (s_Pipeline) {
			return s_Pipeline->GetStats();
		}
		return {};
	}

	void Renderer3D::ResetStats() {
		// Stats are reset in BeginFrame automatically
	}

}
