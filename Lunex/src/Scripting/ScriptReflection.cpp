#include "stpch.h"
#include "ScriptReflection.h"

#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace Lunex {

	const char* ScriptFieldTypeToString(ScriptFieldType type) {
		switch (type) {
			case ScriptFieldType::Float:  return "Float";
			case ScriptFieldType::Int:    return "Int";
			case ScriptFieldType::Bool:   return "Bool";
			case ScriptFieldType::Vec2:   return "Vec2";
			case ScriptFieldType::Vec3:   return "Vec3";
			case ScriptFieldType::String: return "String";
		}
		return "Float";
	}

	bool ScriptFieldTypeFromString(const std::string& s, ScriptFieldType& out) {
		static const std::pair<const char*, ScriptFieldType> table[] = {
			{ "Float", ScriptFieldType::Float }, { "Int", ScriptFieldType::Int }, { "Bool", ScriptFieldType::Bool },
			{ "Vec2", ScriptFieldType::Vec2 }, { "Vec3", ScriptFieldType::Vec3 }, { "String", ScriptFieldType::String },
		};
		for (const auto& [name, type] : table) {
			if (s == name) { out = type; return true; }
		}
		return false;
	}

	namespace {

		std::string StripComments(const std::string& src) {
			std::string out;
			out.reserve(src.size());
			for (size_t i = 0; i < src.size(); i++) {
				if (src[i] == '"') { // keep string literals intact
					out += src[i++];
					while (i < src.size() && src[i] != '"') {
						if (src[i] == '\\' && i + 1 < src.size()) out += src[i++];
						out += src[i++];
					}
					if (i < src.size()) out += src[i];
				}
				else if (src.compare(i, 2, "//") == 0) {
					while (i < src.size() && src[i] != '\n') i++;
					out += '\n';
				}
				else if (src.compare(i, 2, "/*") == 0) {
					size_t end = src.find("*/", i + 2);
					i = (end == std::string::npos) ? src.size() : end + 1;
					out += ' ';
				}
				else {
					out += src[i];
				}
			}
			return out;
		}

		std::vector<float> ExtractNumbers(const std::string& text) {
			std::vector<float> nums;
			// Not preceded by an identifier character, so "Vec3(" does not yield a 3
			static const std::regex number(R"((?:^|[^\w.])([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?))");
			for (auto it = std::sregex_iterator(text.begin(), text.end(), number); it != std::sregex_iterator(); ++it) {
				try { nums.push_back(std::stof((*it)[1].str())); } catch (...) {}
			}
			return nums;
		}

		bool MapType(const std::string& cppType, ScriptFieldType& out) {
			std::string t = cppType;
			for (const char* ns : { "Lunex::", "glm::", "std::" }) {
				if (t.rfind(ns, 0) == 0) t = t.substr(strlen(ns));
			}
			if (t == "float" || t == "double") { out = ScriptFieldType::Float; return true; }
			if (t == "int" || t == "int32_t" || t == "uint32_t" || t == "unsigned") { out = ScriptFieldType::Int; return true; }
			if (t == "bool") { out = ScriptFieldType::Bool; return true; }
			if (t == "Vec2" || t == "vec2") { out = ScriptFieldType::Vec2; return true; }
			if (t == "Vec3" || t == "vec3") { out = ScriptFieldType::Vec3; return true; }
			if (t == "string") { out = ScriptFieldType::String; return true; }
			return false;
		}

		void ParseDefault(const std::string& init, ScriptFieldValue& value) {
			switch (value.Type) {
				case ScriptFieldType::Bool:
					value.Number.x = (init.find("true") != std::string::npos) ? 1.0f : 0.0f;
					break;
				case ScriptFieldType::String: {
					auto a = init.find('"'), b = init.rfind('"');
					if (a != std::string::npos && b > a) value.String = init.substr(a + 1, b - a - 1);
					break;
				}
				default: {
					auto nums = ExtractNumbers(init);
					int count = value.Type == ScriptFieldType::Vec3 ? 3 : value.Type == ScriptFieldType::Vec2 ? 2 : 1;
					for (int i = 0; i < count; i++) {
						if (i < static_cast<int>(nums.size())) value.Number[i] = nums[i];
						else if (nums.size() == 1) value.Number[i] = nums[0]; // Vec3(1.0f) broadcast
					}
					if (value.Type == ScriptFieldType::Int) value.Number.x = std::floor(value.Number.x);
					break;
				}
			}
		}
	}

	namespace ScriptReflection {

		std::vector<ScriptFieldInfo> ParseSource(const std::string& rawSource) {
			std::vector<ScriptFieldInfo> fields;
			const std::string source = StripComments(rawSource);

			// RegisterVar("Name", &Member [, a, b])  /  RegisterVarRange(..., min, max)  /  RegisterVarTooltip(..., "tip")
			static const std::regex reg(R"re(RegisterVar(Range|Tooltip)?\s*\(\s*"([^"]+)"\s*,\s*&\s*([A-Za-z_]\w*)\s*((?:,[^;]*?)?)\)\s*;)re");

			for (auto it = std::sregex_iterator(source.begin(), source.end(), reg); it != std::sregex_iterator(); ++it) {
				const std::string kind = (*it)[1].str();
				ScriptFieldInfo info;
				info.Name = (*it)[2].str();
				const std::string member = (*it)[3].str();
				const std::string extra = (*it)[4].str();

				// Member declaration: <type> Member = init;  or  <type> Member{init};  or  <type> Member;
				const std::regex decl("\\b((?:Lunex::|glm::|std::)?\\w+)\\s+" + member + "\\s*(?:=\\s*([^;]+)|\\{([^}]*)\\})?\\s*;");
				std::smatch m;
				bool typed = false;
				for (auto d = std::sregex_iterator(source.begin(), source.end(), decl); d != std::sregex_iterator(); ++d) {
					if (MapType((*d)[1].str(), info.Default.Type)) {
						std::string init = (*d)[2].matched ? (*d)[2].str() : (*d)[3].str();
						ParseDefault(init, info.Default);
						typed = true;
						break;
					}
				}
				if (!typed) continue; // unknown type: can't edit it safely

				if (kind == "Range") {
					auto nums = ExtractNumbers(extra);
					if (nums.size() >= 2) { info.HasRange = true; info.Min = nums[0]; info.Max = nums[1]; }
				}
				else if (kind == "Tooltip") {
					auto a = extra.find('"'), b = extra.rfind('"');
					if (a != std::string::npos && b > a) info.Tooltip = extra.substr(a + 1, b - a - 1);
				}

				// RegisterVar may be called several times (e.g. in Start) - keep the first
				bool duplicate = false;
				for (const auto& f : fields) duplicate |= (f.Name == info.Name);
				if (!duplicate) fields.push_back(std::move(info));
			}
			return fields;
		}

		const std::vector<ScriptFieldInfo>& GetFields(const std::filesystem::path& scriptFile) {
			struct CacheEntry {
				std::filesystem::file_time_type Time{};
				std::vector<ScriptFieldInfo> Fields;
			};
			static std::unordered_map<std::string, CacheEntry> s_Cache;
			static std::mutex s_Mutex;
			static const std::vector<ScriptFieldInfo> s_Empty;

			std::error_code ec;
			auto time = std::filesystem::last_write_time(scriptFile, ec);
			if (ec) return s_Empty;

			std::scoped_lock lock(s_Mutex);
			auto& entry = s_Cache[scriptFile.lexically_normal().string()];
			if (entry.Time != time) {
				std::ifstream in(scriptFile, std::ios::binary);
				std::stringstream ss;
				ss << in.rdbuf();
				entry.Fields = ParseSource(ss.str());
				entry.Time = time;
			}
			return entry.Fields;
		}

	} // namespace ScriptReflection

} // namespace Lunex
