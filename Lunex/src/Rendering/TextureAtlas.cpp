#include "stpch.h"
#include "TextureAtlas.h"
#include "Log/Log.h"

#include <glad/glad.h>

#ifdef _WIN32
	#include <Windows.h>
	static void* GetGLProcAddress(const char* name) {
		void* p = (void*)wglGetProcAddress(name);
		if (!p || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1)
			return nullptr;
		return p;
	}
#else
	static void* GetGLProcAddress(const char* name) { return nullptr; }
#endif

// ============================================================================
// GL_ARB_bindless_texture function pointers (not always in glad)
// ============================================================================

#ifndef GLAD_GL_ARB_bindless_texture

typedef GLuint64 (APIENTRYP PFNGLGETTEXTUREHANDLEARBPROC)(GLuint texture);
typedef void     (APIENTRYP PFNGLMAKETEXTUREHANDLERESIDENTARBPROC)(GLuint64 handle);
typedef void     (APIENTRYP PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC)(GLuint64 handle);

static PFNGLGETTEXTUREHANDLEARBPROC                glGetTextureHandleARB_             = nullptr;
static PFNGLMAKETEXTUREHANDLERESIDENTARBPROC       glMakeTextureHandleResidentARB_    = nullptr;
static PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC    glMakeTextureHandleNonResidentARB_ = nullptr;

static bool s_BindlessLoaded = false;

static bool LoadBindlessFunctions() {
	if (s_BindlessLoaded) return glGetTextureHandleARB_ != nullptr;
	s_BindlessLoaded = true;

	glGetTextureHandleARB_             = (PFNGLGETTEXTUREHANDLEARBPROC)            GetGLProcAddress("glGetTextureHandleARB");
	glMakeTextureHandleResidentARB_    = (PFNGLMAKETEXTUREHANDLERESIDENTARBPROC)   GetGLProcAddress("glMakeTextureHandleResidentARB");
	glMakeTextureHandleNonResidentARB_ = (PFNGLMAKETEXTUREHANDLENONRESIDENTARBPROC)GetGLProcAddress("glMakeTextureHandleNonResidentARB");

	return glGetTextureHandleARB_ != nullptr &&
	       glMakeTextureHandleResidentARB_ != nullptr &&
	       glMakeTextureHandleNonResidentARB_ != nullptr;
}

#endif // GLAD_GL_ARB_bindless_texture

namespace Lunex {

	// ====================================================================
	// Helper wrappers for the function pointers
	// ====================================================================
#ifndef GLAD_GL_ARB_bindless_texture
	static inline GLuint64 CallGetTextureHandleARB(GLuint tex) {
		return glGetTextureHandleARB_ ? glGetTextureHandleARB_(tex) : 0;
	}
	static inline void CallMakeResidentARB(GLuint64 h) {
		if (glMakeTextureHandleResidentARB_) glMakeTextureHandleResidentARB_(h);
	}
	static inline void CallMakeNonResidentARB(GLuint64 h) {
		if (glMakeTextureHandleNonResidentARB_) glMakeTextureHandleNonResidentARB_(h);
	}
#else
	static inline GLuint64 CallGetTextureHandleARB(GLuint tex) {
		return glGetTextureHandleARB(tex);
	}
	static inline void CallMakeResidentARB(GLuint64 h) {
		glMakeTextureHandleResidentARB(h);
	}
	static inline void CallMakeNonResidentARB(GLuint64 h) {
		glMakeTextureHandleNonResidentARB(h);
	}
#endif

	// ====================================================================
	// BINDLESS SUPPORT CHECK
	// ====================================================================

	bool TextureAtlas::IsBindlessSupported() {
		static int cached = -1;
		if (cached < 0) {
#ifdef GLAD_GL_ARB_bindless_texture
			cached = (GLAD_GL_ARB_bindless_texture != 0) ? 1 : 0;
#else
			// Check extension string via indexed queries (GL 3.0+)
			bool hasExt = false;
			GLint numExt = 0;
			glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
			for (GLint i = 0; i < numExt; ++i) {
				const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
				if (ext && strcmp(ext, "GL_ARB_bindless_texture") == 0) {
					hasExt = true;
					break;
				}
			}
			if (hasExt) {
				cached = LoadBindlessFunctions() ? 1 : 0;
			} else {
				cached = 0;
			}
#endif
			if (cached)
				LNX_LOG_INFO("TextureAtlas: GL_ARB_bindless_texture supported");
			else
				LNX_LOG_WARN("TextureAtlas: GL_ARB_bindless_texture NOT supported — textures disabled in path tracer");
		}
		return cached == 1;
	}

	// ====================================================================
	// LIFECYCLE
	// ====================================================================

	void TextureAtlas::Initialize() {
		m_SSBO = StorageBuffer::Create(256 * sizeof(uint64_t), 0);
		m_Handles.reserve(256);
		m_TextureRefs.reserve(256);
	}

	void TextureAtlas::Shutdown() {
		if (IsBindlessSupported()) {
			for (uint64_t h : m_Handles) {
				if (h != 0)
					CallMakeNonResidentARB(h);
			}
		}
		m_Handles.clear();
		m_LookupMap.clear();
		m_TextureRefs.clear();
		m_SSBO.reset();
	}

	// ====================================================================
	// REGISTER TEXTURE
	// ====================================================================

	int32_t TextureAtlas::GetOrAddTexture(const Ref<Texture2D>& texture) {
		if (!texture || !texture->IsLoaded() || !IsBindlessSupported())
			return RT_TEXTURE_NONE;

		uint32_t glID = texture->GetRendererID();
		if (glID == 0) return RT_TEXTURE_NONE;

		auto it = m_LookupMap.find(glID);
		if (it != m_LookupMap.end())
			return it->second;

		uint64_t handle = CallGetTextureHandleARB(glID);
		if (handle == 0) {
			LNX_LOG_WARN("TextureAtlas: Failed to get bindless handle for GL texture {0}", glID);
			return RT_TEXTURE_NONE;
		}

		CallMakeResidentARB(handle);

		int32_t index = static_cast<int32_t>(m_Handles.size());
		m_Handles.push_back(handle);
		m_TextureRefs.push_back(texture);
		m_LookupMap[glID] = index;
		m_Dirty = true;
		return index;
	}

	// ====================================================================
	// UPLOAD & BIND
	// ====================================================================

	void TextureAtlas::UploadToGPU() {
		if (!m_Dirty || m_Handles.empty()) return;

		uint32_t requiredSize = static_cast<uint32_t>(m_Handles.size() * sizeof(uint64_t));

		if (!m_SSBO || requiredSize > static_cast<uint32_t>(m_Handles.capacity() * sizeof(uint64_t))) {
			uint32_t newCap = std::max(requiredSize * 2, 256u * static_cast<uint32_t>(sizeof(uint64_t)));
			m_SSBO = StorageBuffer::Create(newCap, 0);
		}

		m_SSBO->SetData(m_Handles.data(), requiredSize);
		m_Dirty = false;
	}

	void TextureAtlas::Bind(uint32_t binding) const {
		if (m_SSBO)
			m_SSBO->BindForCompute(binding);
	}

	void TextureAtlas::Clear() {
		if (IsBindlessSupported()) {
			for (uint64_t h : m_Handles) {
				if (h != 0)
					CallMakeNonResidentARB(h);
			}
		}
		m_Handles.clear();
		m_LookupMap.clear();
		m_TextureRefs.clear();
		m_Dirty = true;
	}

} // namespace Lunex
