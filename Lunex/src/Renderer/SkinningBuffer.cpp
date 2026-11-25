#include "stpch.h"
#include "SkinningBuffer.h"

#include "Renderer/StorageBuffer.h"
#include "Resources/Animation/Animation.h"

#include <algorithm>

namespace Lunex::SkinningBuffer {

	static int s_DebugBone = -1;
	void SetDebugBone(int bone) { s_DebugBone = bone; }
	int GetDebugBone() { return s_DebugBone; }

	int Upload(const std::vector<glm::mat4>* bones) {
		if (!bones || bones->empty()) return 0;

		static Ref<StorageBuffer> s_Buffer;
		if (!s_Buffer) {
			s_Buffer = StorageBuffer::Create(static_cast<uint32_t>(MAX_BONES * sizeof(glm::mat4)), Binding);
		}

		size_t count = std::min<size_t>(bones->size(), MAX_BONES);
		s_Buffer->SetData(bones->data(), static_cast<uint32_t>(count * sizeof(glm::mat4)));
		s_Buffer->BindForRead(Binding);
		return 1;
	}

} // namespace Lunex::SkinningBuffer
