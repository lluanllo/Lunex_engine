#pragma once

/**
 * @file Blueprint.h
 * @brief Visual scripting: node graphs (.lublueprint) interpreted per entity.
 *
 * A blueprint is a Graph::NodeGraph whose nodes come from the blueprint registry:
 * events (On Start, On Update, On Key...), flow control (Branch, Sequence, For Loop,
 * Delay, Do Once), actions on the entity (transform, physics, materials, particles,
 * animation) and pure data nodes (math, vectors, comparisons, variables, input).
 *
 * It is attached through the ScriptComponent (BlueprintPath) and executed by the
 * BlueprintSystem in Play mode, next to the C++ scripts. Execution follows the white
 * Flow links; data inputs are pulled from pure nodes on demand.
 */

#include "Core/Core.h"
#include "Graph/NodeGraph.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lunex {

	class Scene;
	class Entity;

	namespace Blueprint {

		const Graph::NodeRegistry& GetRegistry();

		// A new blueprint with On Start / On Update ready to be wired
		std::shared_ptr<Graph::NodeGraph> CreateDefault();

		class BlueprintAsset {
		public:
			std::string Name;
			std::filesystem::path Path;
			std::shared_ptr<Graph::NodeGraph> Graph;

			bool Save() const;
			static Ref<BlueprintAsset> Load(const std::filesystem::path& path);
			// Cached by path, reloaded when the file changes on disk
			static Ref<BlueprintAsset> GetOrLoad(const std::filesystem::path& path);
			static Ref<BlueprintAsset> CreateNew(const std::filesystem::path& path);

		private:
			std::filesystem::file_time_type m_LoadedTime{};
		};

		struct Value {
			glm::vec4 V{ 0.0f };
			std::string S;
			uint64_t Entity = 0;   // UUID, 0 = self
		};

		// Runtime state of one blueprint on one entity
		class Instance {
		public:
			explicit Instance(Ref<BlueprintAsset> asset) : m_Asset(std::move(asset)) {}

			void Start(Scene* scene, Entity self);
			void Update(Scene* scene, Entity self, float deltaTime);

			const Ref<BlueprintAsset>& GetAsset() const { return m_Asset; }

			// State used by node implementations
			std::unordered_map<std::string, Value> Variables;
			std::unordered_map<Graph::ID, float> Timers;          // On Timer accumulators
			std::unordered_map<Graph::ID, bool> Flags;            // Do Once, key edge detection
			struct PendingDelay { Graph::ID Node = 0; float Remaining = 0.0f; };
			std::vector<PendingDelay> Delays;

		private:
			void FireEvent(Scene* scene, Entity self, const Graph::Node& node, float deltaTime);
			Ref<BlueprintAsset> m_Asset;
		};

	}

} // namespace Lunex
