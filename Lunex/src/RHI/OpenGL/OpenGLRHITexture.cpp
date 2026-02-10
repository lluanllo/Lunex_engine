#include "stpch.h"
#include "OpenGLRHITexture.h"
#include "OpenGLRHIDevice.h"
#include "Log/Log.h"

// Fallback defines for missing GLAD extensions
#ifndef GLAD_GL_KHR_debug
#define GLAD_GL_KHR_debug 0
#endif

#ifndef GLAD_GL_EXT_texture_filter_anisotropic
#define GLAD_GL_EXT_texture_filter_anisotropic 0
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GLAD_GL_EXT_texture_compression_s3tc
#define GLAD_GL_EXT_texture_compression_s3tc 0
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

#ifndef GLAD_GL_ARB_ES3_compatibility
#define GLAD_GL_ARB_ES3_compatibility 0
#endif

#ifndef GLAD_GL_KHR_texture_compression_astc_ldr
#define GLAD_GL_KHR_texture_compression_astc_ldr 0
#endif

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL RHI TEXTURE 2D IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHITexture2D::OpenGLRHITexture2D(const TextureDesc& desc, const void* initialData) {
		m_Desc = desc;
		
		// Calculate mip levels if auto
		if (desc.MipLevels == 0 || desc.GenerateMipmaps) {
			m_Desc.MipLevels = OpenGLTextureUtils::CalculateMipCount(desc.Width, desc.Height);
		}
		
		CreateTexture(initialData);
	}
	
	OpenGLRHITexture2D::~OpenGLRHITexture2D() {
		if (m_TextureID) {
			// Track deallocation
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(GetGPUMemorySize());
			}
			glDeleteTextures(1, &m_TextureID);
		}
	}
	
	void OpenGLRHITexture2D::CreateTexture(const void* data) {
		m_InternalFormat = OpenGLTextureUtils::GetInternalFormat(m_Desc.Format);
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		glCreateTextures(GL_TEXTURE_2D, 1, &m_TextureID);
		
		// Allocate storage
		glTextureStorage2D(m_TextureID, m_Desc.MipLevels, m_InternalFormat, m_Desc.Width, m_Desc.Height);
		
		// Upload initial data if provided
		if (data && !IsCompressed()) {
			glTextureSubImage2D(m_TextureID, 0, 0, 0, m_Desc.Width, m_Desc.Height, format, type, data);
		}
		
		// Set default sampling parameters
		glTextureParameteri(m_TextureID, GL_TEXTURE_MIN_FILTER, 
			m_Desc.MipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(m_TextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_T, GL_REPEAT);
		
		// Generate mipmaps if requested
		if (data && m_Desc.GenerateMipmaps && m_Desc.MipLevels > 1) {
			glGenerateTextureMipmap(m_TextureID);
		}
		
		// Track allocation
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(GetGPUMemorySize());
		}
	}
	
	void OpenGLRHITexture2D::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		if (!m_TextureID) return;
		
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		uint32_t x = region.X;
		uint32_t y = region.Y;
		uint32_t w = region.Width > 0 ? region.Width : m_Desc.Width;
		uint32_t h = region.Height > 0 ? region.Height : m_Desc.Height;
		uint32_t mip = region.MipLevel;
		
		glTextureSubImage2D(m_TextureID, mip, x, y, w, h, format, type, data);
	}
	
	void OpenGLRHITexture2D::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		if (!m_TextureID) return;
		
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		glGetTextureImage(m_TextureID, region.MipLevel, format, type, static_cast<GLsizei>(size), data);
	}
	
	void OpenGLRHITexture2D::GenerateMipmaps() {
		if (m_TextureID && m_Desc.MipLevels > 1) {
			glGenerateTextureMipmap(m_TextureID);
		}
	}
	
	void OpenGLRHITexture2D::Bind(uint32_t slot) const {
		if (m_TextureID) {
			glBindTextureUnit(slot, m_TextureID);
		}
	}
	
	void OpenGLRHITexture2D::Unbind(uint32_t slot) const {
		glBindTextureUnit(slot, 0);
	}
	
	void OpenGLRHITexture2D::BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel) const {
		if (!m_TextureID) return;
		
		GLenum glAccess = GL_READ_WRITE;
		if (access == BufferAccess::Read) glAccess = GL_READ_ONLY;
		else if (access == BufferAccess::Write) glAccess = GL_WRITE_ONLY;
		
		glBindImageTexture(slot, m_TextureID, mipLevel, GL_FALSE, 0, glAccess, m_InternalFormat);
	}
	
	void OpenGLRHITexture2D::Resize(uint32_t width, uint32_t height) {
		if (width == m_Desc.Width && height == m_Desc.Height) return;
		
		// Delete old texture
		if (m_TextureID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(GetGPUMemorySize());
			}
			glDeleteTextures(1, &m_TextureID);
			m_TextureID = 0;
		}
		
		// Update dimensions
		m_Desc.Width = width;
		m_Desc.Height = height;
		
		// Recreate texture
		CreateTexture(nullptr);
	}
	
	int OpenGLRHITexture2D::ReadPixel(int x, int y) const {
		if (!m_TextureID) return 0;
		
		// Create temp FBO for reading
		GLuint fbo;
		glCreateFramebuffers(1, &fbo);
		glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, m_TextureID, 0);
		
		int value = 0;
		glNamedFramebufferReadBuffer(fbo, GL_COLOR_ATTACHMENT0);
		glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &value);
		
		glDeleteFramebuffers(1, &fbo);
		return value;
	}
	
	void OpenGLRHITexture2D::Clear(const ClearValue& value) {
		if (!m_TextureID) return;
		
		if (IsDepthFormat()) {
			glClearTexImage(m_TextureID, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &value.DepthStencil.Depth);
		} else {
			glClearTexImage(m_TextureID, 0, GL_RGBA, GL_FLOAT, value.Color);
		}
	}
	
	void OpenGLRHITexture2D::OnDebugNameChanged() {
		if (m_TextureID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_TEXTURE, m_TextureID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL RHI TEXTURE CUBE IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHITextureCube::OpenGLRHITextureCube(const TextureDesc& desc) {
		m_Desc = desc;
		m_Desc.ArrayLayers = 6;
		
		if (desc.MipLevels == 0) {
			m_Desc.MipLevels = OpenGLTextureUtils::CalculateMipCount(desc.Width, desc.Width);
		}
		
		m_InternalFormat = OpenGLTextureUtils::GetInternalFormat(m_Desc.Format);
		
		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_TextureID);
		glTextureStorage2D(m_TextureID, m_Desc.MipLevels, m_InternalFormat, m_Desc.Width, m_Desc.Width);
		
		// Default sampling
		glTextureParameteri(m_TextureID, GL_TEXTURE_MIN_FILTER, 
			m_Desc.MipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(m_TextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(GetGPUMemorySize());
		}
	}
	
	OpenGLRHITextureCube::~OpenGLRHITextureCube() {
		if (m_TextureID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(GetGPUMemorySize());
			}
			glDeleteTextures(1, &m_TextureID);
		}
	}
	
	void OpenGLRHITextureCube::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		// Use SetFaceData for cubemaps
	}
	
	void OpenGLRHITextureCube::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		// TODO: Implement cubemap data read
	}
	
	void OpenGLRHITextureCube::SetFaceData(uint32_t face, const void* data, uint64_t size, uint32_t mipLevel) {
		if (!m_TextureID || face >= 6) return;
		
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		uint32_t mipWidth = m_Desc.Width >> mipLevel;
		if (mipWidth == 0) mipWidth = 1;
		
		// For cubemaps, we need to use the old-style binding to upload face data
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_TextureID);
		glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mipLevel, 0, 0, 
						mipWidth, mipWidth, format, type, data);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	}
	
	void OpenGLRHITextureCube::GenerateMipmaps() {
		if (m_TextureID && m_Desc.MipLevels > 1) {
			glGenerateTextureMipmap(m_TextureID);
		}
	}
	
	void OpenGLRHITextureCube::Bind(uint32_t slot) const {
		if (m_TextureID) {
			glBindTextureUnit(slot, m_TextureID);
		}
	}
	
	void OpenGLRHITextureCube::Unbind(uint32_t slot) const {
		glBindTextureUnit(slot, 0);
	}
	
	void OpenGLRHITextureCube::BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel) const {
		if (!m_TextureID) return;
		
		GLenum glAccess = GL_READ_WRITE;
		if (access == BufferAccess::Read) glAccess = GL_READ_ONLY;
		else if (access == BufferAccess::Write) glAccess = GL_WRITE_ONLY;
		
		glBindImageTexture(slot, m_TextureID, mipLevel, GL_TRUE, 0, glAccess, m_InternalFormat);
	}
	
	void OpenGLRHITextureCube::OnDebugNameChanged() {
		if (m_TextureID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_TEXTURE, m_TextureID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL RHI SAMPLER IMPLEMENTATION
	// ============================================================================
	
	namespace {
		GLenum GetGLFilter(FilterMode mode) {
			switch (mode) {
				case FilterMode::Nearest: return GL_NEAREST;
				case FilterMode::Linear: return GL_LINEAR;
				case FilterMode::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
				case FilterMode::LinearMipmapNearest: return GL_LINEAR_MIPMAP_NEAREST;
				case FilterMode::NearestMipmapLinear: return GL_NEAREST_MIPMAP_LINEAR;
				case FilterMode::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
				default: return GL_LINEAR;
			}
		}
		
		GLenum GetGLWrap(WrapMode mode) {
			switch (mode) {
				case WrapMode::Repeat: return GL_REPEAT;
				case WrapMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
				case WrapMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
				case WrapMode::ClampToBorder: return GL_CLAMP_TO_BORDER;
				default: return GL_REPEAT;
			}
		}
		
		GLenum GetGLCompareFunc(CompareFunc func) {
			switch (func) {
				case CompareFunc::Never: return GL_NEVER;
				case CompareFunc::Less: return GL_LESS;
				case CompareFunc::Equal: return GL_EQUAL;
				case CompareFunc::LessEqual: return GL_LEQUAL;
				case CompareFunc::Greater: return GL_GREATER;
				case CompareFunc::NotEqual: return GL_NOTEQUAL;
				case CompareFunc::GreaterEqual: return GL_GEQUAL;
				case CompareFunc::Always: return GL_ALWAYS;
				default: return GL_NEVER;
			}
		}
	}
	
	OpenGLRHISampler::OpenGLRHISampler(const SamplerState& state) {
		m_State = state;
		
		glCreateSamplers(1, &m_SamplerID);
		
		// Filtering
		glSamplerParameteri(m_SamplerID, GL_TEXTURE_MIN_FILTER, GetGLFilter(state.MinFilter));
		glSamplerParameteri(m_SamplerID, GL_TEXTURE_MAG_FILTER, GetGLFilter(state.MagFilter));
		
		// Wrapping
		glSamplerParameteri(m_SamplerID, GL_TEXTURE_WRAP_S, GetGLWrap(state.WrapU));
		glSamplerParameteri(m_SamplerID, GL_TEXTURE_WRAP_T, GetGLWrap(state.WrapV));
		glSamplerParameteri(m_SamplerID, GL_TEXTURE_WRAP_R, GetGLWrap(state.WrapW));
		
		// Border color
		glSamplerParameterfv(m_SamplerID, GL_TEXTURE_BORDER_COLOR, state.BorderColor);
		
		// Anisotropy
		if (state.MaxAnisotropy > 1.0f && GLAD_GL_EXT_texture_filter_anisotropic) {
			glSamplerParameterf(m_SamplerID, GL_TEXTURE_MAX_ANISOTROPY_EXT, state.MaxAnisotropy);
		}
		
		// LOD
		glSamplerParameterf(m_SamplerID, GL_TEXTURE_LOD_BIAS, state.MipLODBias);
		glSamplerParameterf(m_SamplerID, GL_TEXTURE_MIN_LOD, state.MinLOD);
		glSamplerParameterf(m_SamplerID, GL_TEXTURE_MAX_LOD, state.MaxLOD);
		
		// Comparison mode (for shadow maps)
		if (state.ComparisonFunc != CompareFunc::Never) {
			glSamplerParameteri(m_SamplerID, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glSamplerParameteri(m_SamplerID, GL_TEXTURE_COMPARE_FUNC, GetGLCompareFunc(state.ComparisonFunc));
		}
	}
	
	OpenGLRHISampler::~OpenGLRHISampler() {
		if (m_SamplerID) {
			glDeleteSamplers(1, &m_SamplerID);
		}
	}
	
	void OpenGLRHISampler::Bind(uint32_t slot) const {
		if (m_SamplerID) {
			glBindSampler(slot, m_SamplerID);
		}
	}
	
	void OpenGLRHISampler::Unbind(uint32_t slot) const {
		glBindSampler(slot, 0);
	}
	
	void OpenGLRHISampler::OnDebugNameChanged() {
		if (m_SamplerID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_SAMPLER, m_SamplerID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// FACTORY IMPLEMENTATIONS
	// ============================================================================
	
	Ref<RHITexture2D> RHITexture2D::Create(const TextureDesc& desc) {
		return CreateRef<OpenGLRHITexture2D>(desc);
	}
	
	Ref<RHITexture2D> RHITexture2D::Create(const TextureDesc& desc, const void* data, uint64_t dataSize) {
		return CreateRef<OpenGLRHITexture2D>(desc, data);
	}
	
	Ref<RHITextureCube> RHITextureCube::Create(uint32_t size, TextureFormat format, uint32_t mipLevels) {
		TextureDesc desc;
		desc.Width = size;
		desc.Height = size;
		desc.Format = format;
		desc.MipLevels = mipLevels;
		return CreateRef<OpenGLRHITextureCube>(desc);
	}
	
	Ref<RHISampler> RHISampler::Create(const SamplerState& state) {
		return CreateRef<OpenGLRHISampler>(state);
	}
	
	Ref<RHISampler> RHISampler::CreateLinear() {
		return Create(SamplerState::Linear());
	}
	
	Ref<RHISampler> RHISampler::CreatePoint() {
		return Create(SamplerState::Point());
	}
	
	Ref<RHISampler> RHISampler::CreateAnisotropic(float anisotropy) {
		return Create(SamplerState::Anisotropic(anisotropy));
	}
	
	Ref<RHISampler> RHISampler::CreateShadow() {
		return Create(SamplerState::Shadow());
	}
	
	Ref<RHISampler> RHISampler::CreateClamp() {
		SamplerState state;
		state.WrapU = WrapMode::ClampToEdge;
		state.WrapV = WrapMode::ClampToEdge;
		state.WrapW = WrapMode::ClampToEdge;
		return Create(state);
	}

	// ============================================================================
	// OPENGL RHI TEXTURE 2D ARRAY IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHITexture2DArray::OpenGLRHITexture2DArray(const TextureDesc& desc) {
		m_Desc = desc;
		m_InternalFormat = OpenGLTextureUtils::GetInternalFormat(m_Desc.Format);
		
		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_TextureID);
		glTextureStorage3D(m_TextureID, m_Desc.MipLevels, m_InternalFormat,
						   m_Desc.Width, m_Desc.Height, m_Desc.ArrayLayers);
		
		// Default sampling
		glTextureParameteri(m_TextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_TextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(m_TextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTextureParameteri(m_TextureID, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTextureParameteri(m_TextureID, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTextureParameterfv(m_TextureID, GL_TEXTURE_BORDER_COLOR, borderColor);
		
		if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
			device->TrackAllocation(GetGPUMemorySize());
		}
	}
	
	OpenGLRHITexture2DArray::~OpenGLRHITexture2DArray() {
		if (m_TextureID) {
			if (auto* device = dynamic_cast<OpenGLRHIDevice*>(RHIDevice::Get())) {
				device->TrackDeallocation(GetGPUMemorySize());
			}
			glDeleteTextures(1, &m_TextureID);
		}
	}
	
	void OpenGLRHITexture2DArray::SetData(const void* data, uint64_t size, const TextureRegion& region) {
		if (!m_TextureID) return;
		
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		uint32_t w = region.Width > 0 ? region.Width : m_Desc.Width;
		uint32_t h = region.Height > 0 ? region.Height : m_Desc.Height;
		
		glTextureSubImage3D(m_TextureID, region.MipLevel,
							region.X, region.Y, region.ArrayLayer,
							w, h, 1,
							format, type, data);
	}
	
	void OpenGLRHITexture2DArray::GetData(void* data, uint64_t size, const TextureRegion& region) const {
		if (!m_TextureID) return;
		
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		glGetTextureImage(m_TextureID, region.MipLevel, format, type, static_cast<GLsizei>(size), data);
	}
	
	void OpenGLRHITexture2DArray::GenerateMipmaps() {
		if (m_TextureID && m_Desc.MipLevels > 1) {
			glGenerateTextureMipmap(m_TextureID);
		}
	}
	
	void OpenGLRHITexture2DArray::SetLayerData(uint32_t layer, const void* data, uint64_t size, uint32_t mipLevel) {
		if (!m_TextureID || layer >= m_Desc.ArrayLayers) return;
		
		GLenum format = OpenGLTextureUtils::GetFormat(m_Desc.Format);
		GLenum type = OpenGLTextureUtils::GetType(m_Desc.Format);
		
		uint32_t mipWidth = m_Desc.Width >> mipLevel;
		uint32_t mipHeight = m_Desc.Height >> mipLevel;
		if (mipWidth == 0) mipWidth = 1;
		if (mipHeight == 0) mipHeight = 1;
		
		glTextureSubImage3D(m_TextureID, mipLevel, 0, 0, layer,
							mipWidth, mipHeight, 1, format, type, data);
	}
	
	void OpenGLRHITexture2DArray::Bind(uint32_t slot) const {
		if (m_TextureID) {
			glBindTextureUnit(slot, m_TextureID);
		}
	}
	
	void OpenGLRHITexture2DArray::Unbind(uint32_t slot) const {
		glBindTextureUnit(slot, 0);
	}
	
	void OpenGLRHITexture2DArray::BindAsImage(uint32_t slot, BufferAccess access, uint32_t mipLevel) const {
		if (!m_TextureID) return;
		
		GLenum glAccess = GL_READ_WRITE;
		if (access == BufferAccess::Read) glAccess = GL_READ_ONLY;
		else if (access == BufferAccess::Write) glAccess = GL_WRITE_ONLY;
		
		glBindImageTexture(slot, m_TextureID, mipLevel, GL_TRUE, 0, glAccess, m_InternalFormat);
	}
	
	void OpenGLRHITexture2DArray::OnDebugNameChanged() {
		if (m_TextureID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_TEXTURE, m_TextureID, -1, m_DebugName.c_str());
		}
	}
	
	// Factory
	Ref<RHITexture2DArray> RHITexture2DArray::Create(uint32_t width, uint32_t height, uint32_t layers,
													 TextureFormat format, uint32_t mipLevels) {
		TextureDesc desc;
		desc.Width = width;
		desc.Height = height;
		desc.ArrayLayers = layers;
		desc.Format = format;
		desc.MipLevels = mipLevels;
		desc.IsRenderTarget = true;
		return CreateRef<OpenGLRHITexture2DArray>(desc);
	}

	// ============================================================================
	// TEXTURE MEMORY CALCULATION
	// ============================================================================
	
	uint64_t RHITexture::CalculateTextureSize() const {
		uint64_t totalSize = 0;
		uint32_t bpp = 4; // Bytes per pixel (estimate)
		
		// Adjust BPP based on format
		switch (m_Desc.Format) {
			case TextureFormat::R8: bpp = 1; break;
			case TextureFormat::RG8: bpp = 2; break;
			case TextureFormat::RGB8: case TextureFormat::SRGB8: bpp = 3; break;
			case TextureFormat::RGBA8: case TextureFormat::SRGBA8: bpp = 4; break;
			case TextureFormat::R16F: bpp = 2; break;
			case TextureFormat::RG16F: bpp = 4; break;
			case TextureFormat::RGBA16F: bpp = 8; break;
			case TextureFormat::R32F: bpp = 4; break;
			case TextureFormat::RGBA32F: bpp = 16; break;
			case TextureFormat::Depth24Stencil8: bpp = 4; break;
			default: bpp = 4; break;
		}
		
		// Calculate size for all mip levels
		uint32_t w = m_Desc.Width;
		uint32_t h = m_Desc.Height;
		uint32_t d = m_Desc.Depth;
		
		for (uint32_t mip = 0; mip < m_Desc.MipLevels; mip++) {
			totalSize += (uint64_t)w * h * d * bpp;
			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);
			d = std::max(1u, d / 2);
		}
		
		// Multiply by array layers
		totalSize *= m_Desc.ArrayLayers;
		
		return totalSize;
	}

} // namespace RHI
} // namespace Lunex
