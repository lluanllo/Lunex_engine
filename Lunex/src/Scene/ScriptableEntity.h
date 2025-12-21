#pragma once

/**
 * @file ScriptableEntity.h
 * @brief Base class for native C++ scripts
 * 
 * AAA Architecture: ScriptableEntity provides the interface
 * for native scripts attached via NativeScriptComponent.
 */

#include "Entity.h"

namespace Lunex {
	
	// Forward declarations
	class ScriptSystem;
	
	class ScriptableEntity {
		public:
			virtual ~ScriptableEntity() {}
			
			template<typename T>
			T& GetComponent() {
				return m_Entity.GetComponent<T>();
			}
			
			Entity GetEntity() const { return m_Entity; }
			
		protected:
			virtual void OnCreate() {}
			virtual void OnDestroy() {}
			virtual void OnUpdate(Timestep ts) {}
			
		private:
			Entity m_Entity;
			
			friend class Scene;
			friend class ScriptSystem;  // AAA Architecture: Allow ScriptSystem access
	};
}