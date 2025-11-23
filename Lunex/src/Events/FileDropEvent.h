#pragma once

#include "Event.h"
#include <vector>
#include <string>

namespace Lunex {
	class FileDropEvent : public Event {
	public:
		FileDropEvent(const std::vector<std::string>& paths)
			: m_Paths(paths) {}
		
		const std::vector<std::string>& GetPaths() const { return m_Paths; }
		
		std::string ToString() const override {
			std::stringstream ss;
			ss << "FileDropEvent: " << m_Paths.size() << " files";
			return ss.str();
		}
		
		EVENT_CLASS_TYPE(FileDrop)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
		
	private:
		std::vector<std::string> m_Paths;
	};
}
