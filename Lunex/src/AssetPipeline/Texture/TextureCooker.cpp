#include "stpch.h"
#include "TextureCooker.h"
#include "Log/Log.h"

#include <chrono>

namespace Lunex {

	TextureCookResult TextureCooker::Cook(
		const std::filesystem::path& inputPath,
		const std::filesystem::path& outputPath,
		const TextureCookSettings& settings)
	{
		TextureCookResult result;
		auto startTime = std::chrono::high_resolution_clock::now();
		
		// Check input exists
		if (!std::filesystem::exists(inputPath)) {
			result.Success = false;
			result.ErrorMessage = "Input file not found: " + inputPath.string();
			LNX_LOG_ERROR("TextureCooker: {0}", result.ErrorMessage);
			return result;
		}
		
		// Check cache
		if (settings.UseCache && !NeedsRecook(inputPath, outputPath)) {
			result.Success = true;
			result.OutputPath = outputPath;
			LNX_LOG_INFO("TextureCooker: Using cached: {0}", outputPath.string());
			return result;
		}
		
		// Create output directory
		std::filesystem::create_directories(outputPath.parent_path());
		
		// Import raw data
		std::vector<uint8_t> rawData;
		uint32_t width, height, channels;
		
		if (!TextureImporter::Import(inputPath, rawData, width, height, channels)) {
			result.Success = false;
			result.ErrorMessage = "Failed to import texture: " + inputPath.string();
			LNX_LOG_ERROR("TextureCooker: {0}", result.ErrorMessage);
			return result;
		}
		
		result.OriginalSize = rawData.size();
		
		// TODO: Actual compression using KTX-Software or similar
		// For now, just copy the file
		
		try {
			std::filesystem::copy_file(inputPath, outputPath, 
				std::filesystem::copy_options::overwrite_existing);
			
			result.Success = true;
			result.OutputPath = outputPath;
			result.CompressedSize = std::filesystem::file_size(outputPath);
			result.CompressionRatio = (float)result.OriginalSize / (float)result.CompressedSize;
		}
		catch (const std::exception& e) {
			result.Success = false;
			result.ErrorMessage = e.what();
		}
		
		auto endTime = std::chrono::high_resolution_clock::now();
		result.CookTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
		
		if (result.Success) {
			LNX_LOG_INFO("TextureCooker: Cooked {0} -> {1} ({2:.2f}ms)", 
				inputPath.filename().string(), outputPath.filename().string(), result.CookTimeMs);
		}
		
		return result;
	}

	std::vector<TextureCookResult> TextureCooker::CookDirectory(
		const std::filesystem::path& inputDir,
		const std::filesystem::path& outputDir,
		const TextureCookSettings& settings,
		bool recursive)
	{
		std::vector<TextureCookResult> results;
		
		auto iterator = recursive ? 
			std::filesystem::recursive_directory_iterator(inputDir) :
			std::filesystem::recursive_directory_iterator(inputDir);
		
		for (const auto& entry : std::filesystem::directory_iterator(inputDir)) {
			if (!entry.is_regular_file()) continue;
			
			if (!IsInputFormatSupported(entry.path())) continue;
			
			auto relativePath = std::filesystem::relative(entry.path(), inputDir);
			auto outputPath = outputDir / relativePath;
			outputPath.replace_extension(".ktx2");
			
			results.push_back(Cook(entry.path(), outputPath, settings));
		}
		
		return results;
	}

	bool TextureCooker::NeedsRecook(
		const std::filesystem::path& inputPath,
		const std::filesystem::path& outputPath)
	{
		if (!std::filesystem::exists(outputPath)) {
			return true;
		}
		
		try {
			auto inputTime = std::filesystem::last_write_time(inputPath);
			auto outputTime = std::filesystem::last_write_time(outputPath);
			return inputTime > outputTime;
		}
		catch (const std::exception&) {
			return true;
		}
	}

	std::vector<std::string> TextureCooker::GetSupportedInputFormats() {
		return { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".exr" };
	}

	bool TextureCooker::IsInputFormatSupported(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		auto supported = GetSupportedInputFormats();
		return std::find(supported.begin(), supported.end(), ext) != supported.end();
	}

	// TextureImporter implementation
	
	bool TextureImporter::Import(
		const std::filesystem::path& path,
		std::vector<uint8_t>& outData,
		uint32_t& outWidth,
		uint32_t& outHeight,
		uint32_t& outChannels)
	{
		// TODO: Use stb_image for actual import
		// For now, return false to indicate not implemented
		LNX_LOG_WARN("TextureImporter::Import not fully implemented yet");
		return false;
	}

	std::vector<std::string> TextureImporter::GetSupportedExtensions() {
		return { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr", ".exr" };
	}

} // namespace Lunex
