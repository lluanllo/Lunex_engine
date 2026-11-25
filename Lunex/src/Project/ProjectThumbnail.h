#pragma once

/**
 * @file ProjectThumbnail.h
 * @brief Project preview image shown by the launcher (<project>/.lunex/thumbnail.png).
 *
 * The editor captures it automatically from the scene's primary camera (or the editor
 * viewport when there is none) a moment after a project opens and whenever a scene is
 * saved.
 */

#include <cstdint>
#include <filesystem>

namespace Lunex::ProjectThumbnail {

	// <projectDir>/.lunex/thumbnail.png
	std::filesystem::path GetPath(const std::filesystem::path& projectDir);

	// Read an RGBA color texture from the GPU, downscale to maxWidth and save as PNG
	bool SaveFromTexture(uint32_t glTextureID, uint32_t width, uint32_t height,
		const std::filesystem::path& outPath, uint32_t maxWidth = 480);

}
