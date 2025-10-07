#include "stpch.h"
#include "Entity.h"

namespace Lunex {
	Entity::Entity(entt::entity handle, Scene* scene)
		: m_EntityHandle(handle), m_Scene(scene) {
	}
}