#include "stpch.h"
#include "ProjectThumbnail.h"

#include "Log/Log.h"

#include <glad/glad.h>
#include <stb_image_write.h>

#include <algorithm>
#include <vector>

namespace Lunex::ProjectThumbnail {

	std::filesystem::path GetPath(const std::filesystem::path& projectDir) {
		return projectDir / ".lunex" / "thumbnail.png";
	}

	bool SaveFromTexture(uint32_t glTextureID, uint32_t width, uint32_t height,
		const std::filesystem::path& outPath, uint32_t maxWidth)
	{
		if (glTextureID == 0 || width == 0 || height == 0) return false;

		// Float readback so HDR targets work too; values are clamped when converted
		std::vector<float> src(static_cast<size_t>(width) * height * 4);
		glBindTexture(GL_TEXTURE_2D, glTextureID);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, src.data());
		glBindTexture(GL_TEXTURE_2D, 0);

		// Box-filter downscale + vertical flip (GL origin is bottom-left)
		const uint32_t outW = std::min(width, maxWidth);
		const uint32_t outH = std::max(1u, static_cast<uint32_t>(static_cast<uint64_t>(height) * outW / width));
		std::vector<uint8_t> dst(static_cast<size_t>(outW) * outH * 4);
		const float sx = static_cast<float>(width) / outW;
		const float sy = static_cast<float>(height) / outH;
		for (uint32_t y = 0; y < outH; y++) {
			uint32_t y0 = static_cast<uint32_t>(y * sy), y1 = std::max(y0 + 1, static_cast<uint32_t>((y + 1) * sy));
			for (uint32_t x = 0; x < outW; x++) {
				uint32_t x0 = static_cast<uint32_t>(x * sx), x1 = std::max(x0 + 1, static_cast<uint32_t>((x + 1) * sx));
				float acc[3] = { 0, 0, 0 };
				uint32_t n = 0;
				for (uint32_t yy = y0; yy < y1 && yy < height; yy++)
					for (uint32_t xx = x0; xx < x1 && xx < width; xx++, n++)
						for (int c = 0; c < 3; c++) acc[c] += src[(static_cast<size_t>(yy) * width + xx) * 4 + c];
				uint8_t* o = &dst[(static_cast<size_t>(outH - 1 - y) * outW + x) * 4];
				for (int c = 0; c < 3; c++)
					o[c] = static_cast<uint8_t>(std::clamp(acc[c] / std::max(n, 1u), 0.0f, 1.0f) * 255.0f + 0.5f);
				o[3] = 255;
			}
		}

		std::error_code ec;
		std::filesystem::create_directories(outPath.parent_path(), ec);
		if (!stbi_write_png(outPath.string().c_str(), outW, outH, 4, dst.data(), outW * 4)) {
			LNX_LOG_WARN("Could not write project thumbnail {0}", outPath.string());
			return false;
		}
		return true;
	}

}
