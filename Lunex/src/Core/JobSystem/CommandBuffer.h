#pragma once

#include "Core/Core.h"
#include <functional>
#include <memory>
#include <vector>
#include <utility>

// Forward declarations
namespace Lunex {
	class Renderer2D;
	class Renderer3D;
	class Scene;
	class Asset;
	class AssetManager;
}

namespace Lunex {

	/// <summary>
	/// Context provided to main-thread commands for safe access to engine systems.
	/// All pointers are guaranteed valid during command execution.
	/// </summary>
	struct MainThreadContext {
		Renderer2D* Renderer2D = nullptr;
		Renderer3D* Renderer3D = nullptr;
		Scene* ActiveScene = nullptr;
		AssetManager* Assets = nullptr;

		// Add more systems as needed
	};

	/// <summary>
	/// Main-thread command with ownership transfer for safe async->main data passing.
	/// 
	/// Commands are enqueued from worker threads and executed on main thread.
	/// Uses shared_ptr for ownership transfer to prevent dangling pointers.
	/// 
	/// Scene Versioning:
	/// Commands store scene version for automatic cancellation when scenes reload.
	/// This prevents stale commands from executing on wrong scene.
	/// 
	/// Usage Example (CORRECT):
	/// ```cpp
	/// // Worker thread
	/// auto meshData = std::make_shared<MeshData>(GenerateMesh());
	/// auto cmd = Command::CreateWithOwnership(
	///     sceneVersion,
	///     meshData,
	///     [](MainThreadContext& ctx, MeshData* data) {
	///         ctx.Renderer3D->UploadMesh(*data);
	///     }
	/// );
	/// JobSystem::Get().PushMainThreadCommand(std::move(cmd));
	/// 
	/// // Main thread (in game loop)
	/// JobSystem::Get().FlushMainThreadCommands(currentSceneVersion);
	/// ```
	/// 
	/// Usage Example (INCORRECT - DANGLING POINTER):
	/// ```cpp
	/// // ? BAD: Local variable will be destroyed!
	/// MeshData data = GenerateMesh();
	/// PushMainThreadCommand([&data]() { // DANGER!
	///     UploadMesh(data); // data is already destroyed
	/// });
	/// ```
	/// </summary>
	struct Command {
		/// Scene version this command was created for
		uint64_t SceneVersion = 0;

		/// Function to execute on main thread
		std::function<void(MainThreadContext&)> Function;

		/// Creation timestamp for profiling
		float CreationTime = 0.0f;

		/// <summary>
		/// Factory method for commands with ownership transfer.
		/// 
		/// Uses shared_ptr to ensure data lifetime across threads.
		/// Data is kept alive until command executes (or is cancelled).
		/// </summary>
		/// <typeparam name="T">Type of owned data</typeparam>
		/// <param name="version">Scene version for cancellation</param>
		/// <param name="data">Shared data (copied by reference count)</param>
		/// <param name="fn">Function to execute with data pointer</param>
		/// <returns>Command ready to enqueue</returns>
		template<typename T>
		static Command CreateWithOwnership(
			uint64_t version,
			std::shared_ptr<T> data,
			std::function<void(MainThreadContext&, T*)> fn
		) {
			Command cmd;
			cmd.SceneVersion = version;
			
			// Capture data by copy (increments ref count)
			cmd.Function = [data, fn](MainThreadContext& ctx) {
				fn(ctx, data.get());
			};

			return cmd;
		}

		/// <summary>
		/// Factory method for simple commands without owned data.
		/// Use only if all captures are guaranteed to outlive command.
		/// </summary>
		/// <param name="version">Scene version for cancellation</param>
		/// <param name="fn">Function to execute</param>
		/// <returns>Command ready to enqueue</returns>
		static Command Create(
			uint64_t version,
			std::function<void(MainThreadContext&)> fn
		) {
			Command cmd;
			cmd.SceneVersion = version;
			cmd.Function = std::move(fn);
			return cmd;
		}
	};

	/// <summary>
	/// Thread-local command buffer for worker threads.
	/// Each worker accumulates commands locally, then merges to global buffer.
	/// </summary>
	class CommandBuffer {
	public:
		CommandBuffer() = default;
		~CommandBuffer() = default;

		// Movable but not copyable
		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;
		CommandBuffer(CommandBuffer&&) = default;
		CommandBuffer& operator=(CommandBuffer&&) = default;

		/// <summary>
		/// Add command to local buffer (no synchronization needed).
		/// </summary>
		void Push(Command&& cmd) {
			m_Commands.push_back(std::move(cmd));
		}

		/// <summary>
		/// Get all commands and clear buffer.
		/// </summary>
		std::vector<Command> Flush() {
			std::vector<Command> result = std::move(m_Commands);
			m_Commands.clear();
			return result;
		}

		/// <summary>
		/// Get number of pending commands.
		/// </summary>
		size_t Size() const {
			return m_Commands.size();
		}

		/// <summary>
		/// Check if buffer is empty.
		/// </summary>
		bool IsEmpty() const {
			return m_Commands.empty();
		}

	private:
		std::vector<Command> m_Commands;
	};

	// ========================================================================
	// USAGE EXAMPLES
	// ========================================================================

	// Example 1: Async Texture Upload
	// --------------------------------
	// struct TextureUploadData {
	//     std::vector<uint8_t> Pixels;
	//     uint32_t Width, Height;
	// };
	//
	// auto data = std::make_shared<TextureUploadData>();
	// data->Pixels = LoadPixels(...);
	// data->Width = width;
	// data->Height = height;
	//
	// auto cmd = Command::CreateWithOwnership(
	//     sceneVersion,
	//     data,
	//     [](MainThreadContext& ctx, TextureUploadData* d) {
	//         auto texture = Texture2D::Create(d->Width, d->Height);
	//         texture->SetData(d->Pixels.data(), d->Pixels.size());
	//     }
	// );
	// JobSystem::Get().PushMainThreadCommand(std::move(cmd));

	// Example 2: Batch Entity Creation
	// ---------------------------------
	// struct EntityBatchData {
	//     std::vector<EntityDescriptor> Descriptors;
	// };
	//
	// auto data = std::make_shared<EntityBatchData>();
	// data->Descriptors = PrepareEntities(...);
	//
	// auto cmd = Command::CreateWithOwnership(
	//     sceneVersion,
	//     data,
	//     [](MainThreadContext& ctx, EntityBatchData* d) {
	//         for (auto& desc : d->Descriptors) {
	//             ctx.ActiveScene->CreateEntity(desc.Name);
	//             // Add components...
	//         }
	//     }
	// );
	// JobSystem::Get().PushMainThreadCommand(std::move(cmd));

	// Example 3: Simple Command (No Owned Data)
	// ------------------------------------------
	// // Only safe if 'state' outlives command execution!
	// SomeState* state = &persistentState; // Must outlive command
	//
	// auto cmd = Command::Create(
	//     sceneVersion,
	//     [state](MainThreadContext& ctx) {
	//         state->Update();
	//     }
	// );
	// JobSystem::Get().PushMainThreadCommand(std::move(cmd));

} // namespace Lunex
