#include "stpch.h"
#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"

#include "Log/Log.h"

#include <glad/glad.h>

#include <atomic>
#include <cstring>

namespace Lunex::RHI::Vk {

	// ============================================================================
	// OPENGL STUBS (legacy GL code keeps running as no-ops under Vulkan)
	// ============================================================================

	namespace {
		// x64 has a single calling convention: a function taking no parameters can stand in
		// for any GL entry point (the caller owns argument cleanup); it returns 0.
		uintptr_t StubZero() { return 0; }

		std::atomic<GLuint> s_FakeName{ 1 };
		GLuint APIENTRY StubCreateName() { return s_FakeName++; }
		void APIENTRY StubGenNames(GLsizei n, GLuint* names) { for (GLsizei i = 0; names && i < n; i++) names[i] = s_FakeName++; }
		void APIENTRY StubCreateNames(GLenum, GLsizei n, GLuint* names) { StubGenNames(n, names); }

		const GLubyte* APIENTRY StubGetString(GLenum name) {
			switch (name) {
				case GL_VERSION: return reinterpret_cast<const GLubyte*>("4.6.0 (Lunex Vulkan backend)");
				case GL_SHADING_LANGUAGE_VERSION: return reinterpret_cast<const GLubyte*>("4.60");
				case GL_VENDOR: return reinterpret_cast<const GLubyte*>("Lunex");
				case GL_RENDERER: return reinterpret_cast<const GLubyte*>("Vulkan");
				default: return reinterpret_cast<const GLubyte*>("");
			}
		}
		const GLubyte* APIENTRY StubGetStringi(GLenum, GLuint) { return reinterpret_cast<const GLubyte*>(""); }

		// glad allocates the extension list from GL_NUM_EXTENSIONS and gives up on 0: report one (empty) extension
		void APIENTRY StubGetIntegerv(GLenum pname, GLint* data) { if (data) *data = pname == GL_NUM_EXTENSIONS ? 1 : 0; }
		void APIENTRY StubGetFloatv(GLenum, GLfloat* data) { if (data) *data = 0.0f; }
		void APIENTRY StubGetBooleanv(GLenum, GLboolean* data) { if (data) *data = GL_FALSE; }
		void APIENTRY StubGetShaderiv(GLuint, GLenum pname, GLint* v) {
			if (!v) return;
			*v = (pname == GL_COMPILE_STATUS || pname == GL_LINK_STATUS || pname == GL_VALIDATE_STATUS) ? GL_TRUE : 0;
		}
		GLint APIENTRY StubGetLocation(GLuint, const GLchar*) { return -1; }
		GLenum APIENTRY StubFramebufferComplete(GLenum) { return GL_FRAMEBUFFER_COMPLETE; }
		GLenum APIENTRY StubNamedFramebufferComplete(GLuint, GLenum) { return GL_FRAMEBUFFER_COMPLETE; }
		GLenum APIENTRY StubClientWaitSync(GLsync, GLbitfield, GLuint64) { return GL_ALREADY_SIGNALED; }
		GLsync APIENTRY StubFenceSync(GLenum, GLbitfield) { return reinterpret_cast<GLsync>(static_cast<uintptr_t>(1)); }
		GLboolean APIENTRY StubIsEnabled(GLenum) { return GL_FALSE; }

		// glMap*: writable scratch memory so callers can fill "mapped" buffers
		uint8_t* Scratch() { static uint8_t* s = new uint8_t[32u * 1024u * 1024u]; return s; }
		void* APIENTRY StubMap(GLenum, GLenum) { return Scratch(); }
		void* APIENTRY StubMapRange(GLenum, GLintptr, GLsizeiptr, GLbitfield) { return Scratch(); }
		void* APIENTRY StubMapNamed(GLuint, GLenum) { return Scratch(); }
		void* APIENTRY StubMapNamedRange(GLuint, GLintptr, GLsizeiptr, GLbitfield) { return Scratch(); }
		GLboolean APIENTRY StubUnmap(GLenum) { return GL_TRUE; }
		GLboolean APIENTRY StubUnmapNamed(GLuint) { return GL_TRUE; }

		void* StubLoader(const char* name) {
			struct Entry { const char* Name; void* Fn; };
			static const Entry entries[] = {
				{ "glGetString", reinterpret_cast<void*>(&StubGetString) },
				{ "glGetStringi", reinterpret_cast<void*>(&StubGetStringi) },
				{ "glGetIntegerv", reinterpret_cast<void*>(&StubGetIntegerv) },
				{ "glGetFloatv", reinterpret_cast<void*>(&StubGetFloatv) },
				{ "glGetBooleanv", reinterpret_cast<void*>(&StubGetBooleanv) },
				{ "glGetShaderiv", reinterpret_cast<void*>(&StubGetShaderiv) },
				{ "glGetProgramiv", reinterpret_cast<void*>(&StubGetShaderiv) },
				{ "glGetUniformLocation", reinterpret_cast<void*>(&StubGetLocation) },
				{ "glGetAttribLocation", reinterpret_cast<void*>(&StubGetLocation) },
				{ "glCheckFramebufferStatus", reinterpret_cast<void*>(&StubFramebufferComplete) },
				{ "glCheckNamedFramebufferStatus", reinterpret_cast<void*>(&StubNamedFramebufferComplete) },
				{ "glClientWaitSync", reinterpret_cast<void*>(&StubClientWaitSync) },
				{ "glFenceSync", reinterpret_cast<void*>(&StubFenceSync) },
				{ "glIsEnabled", reinterpret_cast<void*>(&StubIsEnabled) },
				{ "glCreateShader", reinterpret_cast<void*>(&StubCreateName) },
				{ "glCreateProgram", reinterpret_cast<void*>(&StubCreateName) },
				{ "glGenTextures", reinterpret_cast<void*>(&StubGenNames) },
				{ "glGenBuffers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glGenFramebuffers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glGenRenderbuffers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glGenVertexArrays", reinterpret_cast<void*>(&StubGenNames) },
				{ "glGenQueries", reinterpret_cast<void*>(&StubGenNames) },
				{ "glCreateBuffers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glCreateFramebuffers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glCreateRenderbuffers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glCreateVertexArrays", reinterpret_cast<void*>(&StubGenNames) },
				{ "glCreateSamplers", reinterpret_cast<void*>(&StubGenNames) },
				{ "glCreateTextures", reinterpret_cast<void*>(&StubCreateNames) },
				{ "glCreateQueries", reinterpret_cast<void*>(&StubCreateNames) },
				{ "glMapBuffer", reinterpret_cast<void*>(&StubMap) },
				{ "glMapBufferRange", reinterpret_cast<void*>(&StubMapRange) },
				{ "glMapNamedBuffer", reinterpret_cast<void*>(&StubMapNamed) },
				{ "glMapNamedBufferRange", reinterpret_cast<void*>(&StubMapNamedRange) },
				{ "glUnmapBuffer", reinterpret_cast<void*>(&StubUnmap) },
				{ "glUnmapNamedBuffer", reinterpret_cast<void*>(&StubUnmapNamed) },
			};
			for (const auto& e : entries) if (std::strcmp(e.Name, name) == 0) return e.Fn;
			return reinterpret_cast<void*>(&StubZero);
		}
	}

	void LoadOpenGLStubs() {
		if (!gladLoadGLLoader(&StubLoader)) LNX_LOG_ERROR("Vulkan: could not install the OpenGL stubs");
	}

	// ============================================================================
	// DEVICE
	// ============================================================================

	VulkanRHIDevice::VulkanRHIDevice() {
		auto& ctx = VulkanContext::Get();
		const auto& p = ctx.GetProperties();
		m_VkName = ctx.GetDeviceName();
		m_VkCaps.DeviceName = m_VkName;
		m_VkCaps.VendorName = p.vendorID == 0x10DE ? "NVIDIA" : p.vendorID == 0x1002 ? "AMD" : p.vendorID == 0x8086 ? "Intel" : "Unknown";
		m_VkCaps.DriverVersion = "Vulkan " + std::to_string(VK_API_VERSION_MAJOR(p.apiVersion)) + "." + std::to_string(VK_API_VERSION_MINOR(p.apiVersion)) +
			"." + std::to_string(VK_API_VERSION_PATCH(p.apiVersion));
		m_VkCaps.MaxTextureSize = p.limits.maxImageDimension2D;
		m_VkCaps.SupportsCompute = true;
	}

	Ref<RHITexture2D> VulkanRHIDevice::CreateTexture2D(const TextureCreateInfo& info) {
		TextureDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;
		desc.MipLevels = info.MipLevels;
		desc.Format = info.Format;
		desc.IsRenderTarget = info.IsRenderTarget;
		desc.IsStorage = info.IsStorage;
		desc.GenerateMipmaps = info.GenerateMipmaps;
		desc.DebugName = info.DebugName;
		return CreateRef<VulkanTexture2D>(desc, info.InitialData, info.InitialDataSize);
	}

	uint64_t VulkanRHIDevice::GetAllocatedMemory() const { return VulkanContext::Get().GetAllocatedBytes(); }
	void VulkanRHIDevice::WaitIdle() { VulkanContext::Get().WaitIdle(); }

	// ============================================================================
	// CONTEXT
	// ============================================================================

	bool VulkanRHIContext::Initialize() {
		if (VulkanContext::IsActive()) return true;
		if (!VulkanContext::Get().Init(m_Window)) return false;
		LoadOpenGLStubs();
		return true;
	}

	void VulkanRHIContext::Shutdown() {
		// The context outlives every RHI object; released at process exit (Application::~Application)
	}

	std::string VulkanRHIContext::GetAPIVersion() const {
		const auto& p = VulkanContext::Get().GetProperties();
		return "Vulkan " + std::to_string(VK_API_VERSION_MAJOR(p.apiVersion)) + "." + std::to_string(VK_API_VERSION_MINOR(p.apiVersion));
	}

	bool VulkanRHIContext::IsValid() const { return VulkanContext::IsActive(); }

}
