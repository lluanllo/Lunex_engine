#pragma once
#include "Core/Core.h"

#include "Core/UUID.h"
#include "Scene.h"
#include "Components.h"

#include "entt.hpp"

namespace Lunex {
	class Entity {
		public:
			Entity() = default;
			Entity(entt::entity handle, Scene* scene);
			Entity(const Entity& other) = default;
			
			template<typename T, typename... Args>
			T& AddComponent(Args&&... args) {
				LNX_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
				T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
				m_Scene->OnComponentAdded<T>(*this, component);
				return component;
			}
			
			template<typename T, typename... Args>
			T& AddOrReplaceComponent(Args&&... args) {
				T& component = m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
				m_Scene->OnComponentAdded<T>(*this, component);
				return component;
			}
			
			template<typename T>
			T& GetComponent() {
				LNX_CORE_ASSERT(m_Scene && m_EntityHandle != entt::null && m_Scene->m_Registry.valid(m_EntityHandle), "Entity is not valid!");
				LNX_CORE_ASSERT(m_Scene->m_Registry.all_of<T>(m_EntityHandle), "Entity does not have component!");
				return m_Scene->m_Registry.get<T>(m_EntityHandle);
			}
			
			template<typename T>
			bool HasComponent() const {
				if (!m_Scene || m_EntityHandle == entt::null)
					return false;
				return m_Scene->m_Registry.valid(m_EntityHandle) && m_Scene->m_Registry.all_of<T>(m_EntityHandle);
			}
			
			template<typename T>
			void RemoveComponent() {
				LNX_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
				m_Scene->m_Registry.remove<T>(m_EntityHandle);
			}
			
			operator bool() const { return m_EntityHandle != entt::null && m_Scene != nullptr; }
			operator entt::entity() const { return m_EntityHandle; }
			operator uint32_t() const { return (uint32_t)m_EntityHandle; }
			
			UUID GetUUID() { return GetComponent<IDComponent>().ID; }
			const std::string& GetName() { return GetComponent<TagComponent>().Tag; }
			
			// Get the scene this entity belongs to
			Scene* GetScene() const { return m_Scene; }
			
			bool operator==(const Entity& other) const {
				return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene;
			}
			
			bool operator!=(const Entity& other) const {
				return !(*this == other);
			}
			
			bool operator<(const Entity& other) const {
				// For std::set compatibility
				if (m_Scene != other.m_Scene)
					return m_Scene < other.m_Scene;
				return m_EntityHandle < other.m_EntityHandle;
			}
			
		private:
			entt::entity m_EntityHandle{ entt::null };
			Scene* m_Scene = nullptr;
	};
}