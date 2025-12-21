#pragma once

/**
 * @file SceneSerializer.h
 * @brief Scene serialization (YAML format)
 * 
 * AAA Architecture: SceneSerializer handles scene persistence.
 * Supports versioning for future migrations.
 */

#include "Core/Core.h"
#include "Scene.h"

namespace Lunex {
	
	/**
	 * @brief Scene file format version
	 */
	constexpr uint32_t SCENE_FORMAT_VERSION = 2;
	
	class SceneSerializer {
	public:
		SceneSerializer(const Ref<Scene>& scene);
		
		// ========== TEXT FORMAT (YAML) ==========
		void Serialize(const std::string& filepath);
		bool Deserialize(const std::string& filepath);
		
		// ========== BINARY FORMAT (Future) ==========
		void SerializeRuntime(const std::string& filepath);
		bool DeserializeRuntime(const std::string& filepath);
		
		// ========== VERSION INFO ==========
		static uint32_t GetFormatVersion() { return SCENE_FORMAT_VERSION; }
		
	private:
		Ref<Scene> m_Scene;
	};
}