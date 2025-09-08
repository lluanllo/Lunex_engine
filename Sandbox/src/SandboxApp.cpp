#include <Lunex.h>

#include "imgui.h"
#include <glm/ext/matrix_transform.hpp>

class ExampleLayer : public Lunex::Layer{
	public:
		ExampleLayer()
			: Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f), m_SquarePosition(0.0f) {
			
			m_VertexArray.reset(Lunex::VertexArray::Create());
			
			float vertices[3 * 7] = {
				-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
				 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
				 0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f
			};
			
			std::shared_ptr<Lunex::VertexBuffer> vertexBuffer;
			vertexBuffer.reset(Lunex::VertexBuffer::Create(vertices, sizeof(vertices)));
			Lunex::BufferLayout layout = {
				{ Lunex::ShaderDataType::Float3, "a_Position" },
				{ Lunex::ShaderDataType::Float4, "a_Color" }
			};
			vertexBuffer->SetLayout(layout);
			m_VertexArray->AddVertexBuffer(vertexBuffer);
			
			uint32_t indices[3] = { 0, 1, 2 };
			std::shared_ptr<Lunex::IndexBuffer> indexBuffer;
			indexBuffer.reset(Lunex::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
			m_VertexArray->SetIndexBuffer(indexBuffer);
			
			m_SquareVA.reset(Lunex::VertexArray::Create());
			
			float squareVertices[3 * 4] = {
				-0.75f, -0.75f, 0.0f,
				 0.75f, -0.75f, 0.0f,
				 0.75f,  0.75f, 0.0f,
				-0.75f,  0.75f, 0.0f
			};
			
			std::shared_ptr<Lunex::VertexBuffer> squareVB;
			squareVB.reset(Lunex::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));

			squareVB->SetLayout({
				{ Lunex::ShaderDataType::Float3, "a_Position" }
				});

			m_SquareVA->AddVertexBuffer(squareVB);

			uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
			std::shared_ptr<Lunex::IndexBuffer> squareIB;
			squareIB.reset(Lunex::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
			m_SquareVA->SetIndexBuffer(squareIB);

			std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
			
			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;
			
			out vec3 v_Position;
			out vec4 v_Color;
			
			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);	
			}
		)";

			std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;
			
			in vec3 v_Position;
			in vec4 v_Color;
			
			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
				color = v_Color;
			}
		)";

			m_Shader.reset(new Lunex::Shader(vertexSrc, fragmentSrc));

			std::string blueShaderVertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			
			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;
			
			out vec3 v_Position;
			
			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);	
			}
		)";

			std::string blueShaderFragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;
			
			in vec3 v_Position;
			
			void main()
			{
				color = vec4(0.2, 0.3, 0.8, 1.0);
			}
		)";
			
			m_BlueShader.reset(new Lunex::Shader(blueShaderVertexSrc, blueShaderFragmentSrc));
		}

		void OnUpdate(Lunex::Timestep ts) override {
			
			if (Lunex::Input::IsKeyPressed(LN_KEY_LEFT))
				m_CameraPosition.x += m_CameraMoveSpeed * ts;
			else if (Lunex::Input::IsKeyPressed(LN_KEY_RIGHT))
				m_CameraPosition.x -= m_CameraMoveSpeed;
			
			if (Lunex::Input::IsKeyPressed(LN_KEY_UP))
				m_CameraPosition.y -= m_CameraMoveSpeed * ts;
			else if (Lunex::Input::IsKeyPressed(LN_KEY_DOWN))
				m_CameraPosition.y += m_CameraMoveSpeed * ts;
			
			if(Lunex::Input::IsKeyPressed(LN_KEY_A))
				m_CameraRotation -= m_CameraRotationSpeed * ts;
			else if (Lunex::Input::IsKeyPressed(LN_KEY_D))
				m_CameraRotation += m_CameraRotationSpeed * ts;
			
			if (Lunex::Input::IsKeyPressed(LN_KEY_J))
				m_SquarePosition.x += m_SquareMoveSpeed * ts;
			else if (Lunex::Input::IsKeyPressed(LN_KEY_L))
				m_SquarePosition.x -= m_SquareMoveSpeed;
			
			if (Lunex::Input::IsKeyPressed(LN_KEY_I))
				m_SquarePosition.y -= m_SquareMoveSpeed * ts;
			else if (Lunex::Input::IsKeyPressed(LN_KEY_K))
				m_SquarePosition.y += m_SquareMoveSpeed * ts;
			
			Lunex::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
			Lunex::RenderCommand::Clear();
			
			m_Camera.SetPosition(m_CameraPosition);
			m_Camera.SetRotation(m_CameraRotation);
			
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_SquarePosition);
			
			Lunex::Renderer::BeginScene(m_Camera);
			
			Lunex::Renderer::Submit(m_BlueShader, m_SquareVA, transform);
			Lunex::Renderer::Submit(m_Shader, m_VertexArray);
			
			Lunex::Renderer::EndScene();
		}

		
		virtual void OnImGuiRender() override {
			
		}

		void OnEvent(Lunex::Event& event) override {
			Lunex::EventDispatcher dispatcher(event);
			dispatcher.Dispatch<Lunex::KeyPressedEvent>(LNX_BIND_EVENT_FN(ExampleLayer::OnKeyPressEvent));
		}

		bool OnKeyPressEvent(Lunex::KeyPressedEvent& event) {
			return false;
		}
		
		private:
			std::shared_ptr<Lunex::Shader> m_Shader;
			std::shared_ptr<Lunex::VertexArray> m_VertexArray;
			
			std::shared_ptr<Lunex::Shader> m_BlueShader;
			std::shared_ptr<Lunex::VertexArray> m_SquareVA;
			
			Lunex::OrthographicCamera m_Camera;
			glm::vec3 m_CameraPosition;

			float m_CameraRotation = 0.0f;
			float m_CameraRotationSpeed = 180.0f;
			float m_CameraMoveSpeed = 5.0f;

			glm::vec3 m_SquarePosition;
			float m_SquareMoveSpeed = 2.0f;
};

class Sandbox : public Lunex::Application{
	public:
		Sandbox() {
			PushLayer(new ExampleLayer());
		}
		
		~Sandbox() {
		}
};

Lunex::Application* Lunex::CreateApplication() {
	return new Sandbox();
}