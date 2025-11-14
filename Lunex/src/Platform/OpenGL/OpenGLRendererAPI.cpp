#include "stpch.h"
#include "OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Lunex {
	void OpenGLMessageCallback(
		unsigned source,
		unsigned type,
		unsigned id,
		unsigned severity,
		int length,
		const char* message,
		const void* userParam) {
		switch (severity) {
			case GL_DEBUG_SEVERITY_HIGH:         LNX_LOG_CRITICAL(message); return;
			case GL_DEBUG_SEVERITY_MEDIUM:       LNX_LOG_ERROR(message); return;
			case GL_DEBUG_SEVERITY_LOW:          LNX_LOG_WARN(message); return;
			case GL_DEBUG_SEVERITY_NOTIFICATION: LNX_LOG_TRACE(message); return;
		}
		
		LNX_CORE_ASSERT(false, "Unknown severity level!");
	}
	
	void OpenGLRendererAPI::Init() {
		#ifdef LNX_DEBUG
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(OpenGLMessageCallback, nullptr);
			
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		#endif
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_LINE_SMOOTH);
	}
	
	void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
		glViewport(x, y, width, height);
	}
	
	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color) {
		glClearColor(color.r, color.g, color.b, color.a);
	}
	
	void OpenGLRendererAPI::Clear() {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	
	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) {
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
	}
	
	void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) {
		vertexArray->Bind();
		glDrawArrays(GL_LINES, 0, vertexCount);
	}
	
	void OpenGLRendererAPI::SetLineWidth(float width) {
		glLineWidth(width);
	}
	
	void OpenGLRendererAPI::SetDepthTest(bool enabled) {
		if (enabled)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	}
	
	void OpenGLRendererAPI::SetDepthFunc(DepthFunc func) {
		switch (func) {
		case DepthFunc::Less:         glDepthFunc(GL_LESS); break;
		case DepthFunc::LessOrEqual:  glDepthFunc(GL_LEQUAL); break;
		case DepthFunc::Greater:      glDepthFunc(GL_GREATER); break;
		case DepthFunc::Always:       glDepthFunc(GL_ALWAYS); break;
		}
	}
	
	void OpenGLRendererAPI::SetCullMode(CullMode mode) {
		switch (mode) {
		case CullMode::None:
			glDisable(GL_CULL_FACE);
			break;
		case CullMode::Front:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			break;
		case CullMode::Back:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			break;
		case CullMode::FrontAndBack:
			glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT_AND_BACK);
			break;
		}
	}
	
	void OpenGLRendererAPI::SetDepthMask(bool enabled) {
		glDepthMask(enabled ? GL_TRUE : GL_FALSE);
	}
	
	void OpenGLRendererAPI::SetBlend(bool enabled) {
		if (enabled)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	}
	
	void OpenGLRendererAPI::SetBlendFunc(BlendFactor src, BlendFactor dst) {
		GLenum s = GL_SRC_ALPHA, d = GL_ONE_MINUS_SRC_ALPHA;
		auto toGL = [](BlendFactor f) -> GLenum {
			switch (f) {
			case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
			case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
			case BlendFactor::One: return GL_ONE;
			case BlendFactor::Zero: return GL_ZERO;
			}
			return GL_ZERO;
			};
		s = toGL(src); d = toGL(dst);
		glBlendFunc(s, d);
	}
}