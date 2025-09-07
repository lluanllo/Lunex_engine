#pragma once

#include "Core/Core.h"
#include "stpch.h"

#include "Events/Event.h"

namespace Lunex {
	
	struct WindowProps {
		std::string Title;
		unsigned int Width;
		unsigned int Height;
		
		WindowProps(const std::string& title = "Lunex Engine",
			unsigned int width = 1920,
			unsigned int height = 1080)
			: Title(title), Width(width), Height(height) {
		}
	};

	class LUNEX_API Window {
		
		public:
			using EventCallbackFn = std::function<void(Event&)>;
			
			virtual ~Window() = default;
			
			virtual void OnUpdate() = 0;
			
			virtual unsigned int GetWidth() const = 0;
			virtual unsigned int GetHeight() const = 0;
			
			virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
			virtual void SetVSync(bool enabled) = 0;
			virtual bool IsVSync() const = 0;
			
			virtual void* GetNativeWindow() const = 0;
			
			static Window* Create(const WindowProps& props = WindowProps());
	};
}