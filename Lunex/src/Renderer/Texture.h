#pragma once

#include <string>

#include "Core/Core.h"

namespace Lunex {
	enum class TextureFormat {
		None = 0,
		RGBA8,
		RGB8,
		DEPTH24STENCIL8,
		// Añade más formatos según necesites
	};
	
	struct TextureSpecification {
		uint32_t Width = 1;
		uint32_t Height = 1;
		TextureFormat Format = TextureFormat::RGBA8;
		
		bool GenerateMipmaps = true;
		bool IsRenderTarget = false;
		
		std::string DebugName = "";
	};
	
	class Texture {
		public:
			virtual ~Texture() = default;
			
			virtual uint32_t GetWidth() const = 0;
			virtual uint32_t GetHeight() const = 0;
			virtual uint32_t GetRendererID() const = 0;
			
			virtual const std::string& GetPath() const = 0;
			
			virtual void SetData(void* data, uint32_t size) = 0;
			
			virtual void Bind(uint32_t slot = 0) const = 0;
			
			virtual bool IsLoaded() const = 0;
			
			virtual bool operator==(const Texture& other) const = 0;
	};
	
	class Texture2D : public Texture {
		public:
			virtual const TextureSpecification& GetSpecification() const = 0;
			
			static Ref<Texture2D> Create(uint32_t width, uint32_t height);
			static Ref<Texture2D> Create(const std::string& path);
			static Ref<Texture2D> Create(const TextureSpecification& spec);
	};
}