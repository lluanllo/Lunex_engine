#include "stpch.h"
#include "MaterialGraph.h"

#include "MaterialAsset.h"
#include "Renderer/Texture.h"

#include <unordered_map>

namespace Lunex::MaterialGraph {

	using namespace Graph;

	// ============================================================================
	// NODE TYPES
	// ============================================================================

	static Pin MakePin(const char* name, PinType type, const glm::vec4& def = glm::vec4(0.0f)) {
		Pin p;
		p.Name = name;
		p.Type = type;
		p.DefaultValue = def;
		return p;
	}

	const NodeRegistry& GetRegistry() {
		static NodeRegistry registry = [] {
			NodeRegistry r;
			r.Register({ OutputNode, "Material Output", "Output", true, [](Node& n) {
				n.Inputs = {
					MakePin("Base Color", PinType::Color, glm::vec4(1.0f)),
					MakePin("Metallic", PinType::Float, glm::vec4(0.0f)),
					MakePin("Roughness", PinType::Float, glm::vec4(0.5f)),
					MakePin("Specular", PinType::Float, glm::vec4(0.5f)),
					MakePin("Normal", PinType::Texture),
					MakePin("Normal Strength", PinType::Float, glm::vec4(1.0f)),
					MakePin("Ambient Occlusion", PinType::Float, glm::vec4(1.0f)),
					MakePin("Emission", PinType::Color, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)),
					MakePin("Emission Strength", PinType::Float, glm::vec4(0.0f)),
					MakePin("Subsurface Color", PinType::Color, glm::vec4(1.0f, 0.35f, 0.25f, 1.0f)),
					MakePin("Subsurface", PinType::Float, glm::vec4(0.0f)),
				};
			} });
			r.Register({ TextureNode, "Texture Sample", "Input", false, [](Node& n) {
				n.Outputs = {
					MakePin("RGB", PinType::Texture),
					MakePin("R", PinType::Float),
					MakePin("G", PinType::Float),
					MakePin("B", PinType::Float),
					MakePin("A", PinType::Float),
				};
				n.Properties["Path"] = "";
			} });
			r.Register({ ColorNode, "Color", "Input", false, [](Node& n) {
				n.Outputs = { MakePin("Color", PinType::Color) };
				n.Value = glm::vec4(1.0f);
			} });
			r.Register({ FloatNode, "Value", "Input", false, [](Node& n) {
				n.Outputs = { MakePin("Value", PinType::Float) };
				n.Value = glm::vec4(0.5f);
			} });
			r.Register({ MultiplyNode, "Multiply", "Math", false, [](Node& n) {
				n.Inputs = { MakePin("A", PinType::Color, glm::vec4(1.0f)), MakePin("B", PinType::Color, glm::vec4(1.0f)) };
				n.Outputs = { MakePin("Result", PinType::Color) };
			} });
			return r;
		}();
		return registry;
	}

	// ============================================================================
	// MATERIAL -> GRAPH (migration from the slider workflow)
	// ============================================================================

	std::shared_ptr<NodeGraph> CreateFromMaterial(const MaterialAsset& m) {
		auto graph = std::make_shared<NodeGraph>(&GetRegistry());
		Node* out = graph->AddNode(OutputNode, { 520.0f, 0.0f });
		const ID outID = out->NodeID;
		auto outPin = [&](const char* name) { return graph->FindNode(outID)->FindInput(name); };

		outPin("Base Color")->DefaultValue = m.GetAlbedo();
		outPin("Metallic")->DefaultValue = glm::vec4(m.GetMetallic());
		outPin("Roughness")->DefaultValue = glm::vec4(m.GetRoughness());
		outPin("Specular")->DefaultValue = glm::vec4(m.GetSpecular());
		outPin("Normal Strength")->DefaultValue = glm::vec4(m.GetNormalIntensity());
		outPin("Emission")->DefaultValue = glm::vec4(m.GetEmissionColor(), 1.0f);
		outPin("Emission Strength")->DefaultValue = glm::vec4(m.GetEmissionIntensity());
		outPin("Subsurface Color")->DefaultValue = glm::vec4(m.GetSubsurfaceColor(), 1.0f);
		outPin("Subsurface")->DefaultValue = glm::vec4(m.GetSubsurfaceAmount());

		float y = 0.0f;
		auto addTexture = [&](const std::string& path) -> ID {
			Node* t = graph->AddNode(TextureNode, { 0.0f, y });
			t->Properties["Path"] = path;
			y += 190.0f;
			return t->NodeID;
		};
		auto connect = [&](ID node, const char* outName, const char* inName) {
			graph->Connect(graph->FindNode(node)->FindOutput(outName)->PinID, outPin(inName)->PinID);
		};
		// Texture * factor when the factor is not neutral
		auto connectScaled = [&](ID tex, const char* outName, const char* inName, const glm::vec4& factor, bool isColor) {
			if (factor == glm::vec4(1.0f) || (!isColor && factor.x == 1.0f)) { connect(tex, outName, inName); return; }
			Node* mul = graph->AddNode(MultiplyNode, { 270.0f, graph->FindNode(tex)->Position.y });
			const ID mulID = mul->NodeID;
			Node* k = graph->AddNode(isColor ? ColorNode : FloatNode, { 60.0f, graph->FindNode(tex)->Position.y + 150.0f });
			k->Value = factor;
			graph->Connect(graph->FindNode(tex)->FindOutput(outName)->PinID, graph->FindNode(mulID)->FindInput("A")->PinID);
			graph->Connect(k->Outputs[0].PinID, graph->FindNode(mulID)->FindInput("B")->PinID);
			graph->Connect(graph->FindNode(mulID)->Outputs[0].PinID, outPin(inName)->PinID);
		};

		if (m.HasAlbedoMap()) connectScaled(addTexture(m.GetAlbedoPath()), "RGB", "Base Color", m.GetAlbedo(), true);
		if (m.HasNormalMap()) connect(addTexture(m.GetNormalPath()), "RGB", "Normal");
		if (m.HasLayeredTexture()) {
			const auto& cfg = m.GetLayeredTextureConfig();
			ID tex = addTexture(cfg.Path);
			static const char* ch[] = { "R", "G", "B", "A", "R" };
			if (cfg.UseForMetallic) connectScaled(tex, ch[(int)cfg.MetallicChannel], "Metallic", glm::vec4(m.GetMetallicMultiplier()), false);
			if (cfg.UseForRoughness) connectScaled(tex, ch[(int)cfg.RoughnessChannel], "Roughness", glm::vec4(m.GetRoughnessMultiplier()), false);
			if (cfg.UseForAO) connectScaled(tex, ch[(int)cfg.AOChannel], "Ambient Occlusion", glm::vec4(m.GetAOMultiplier()), false);
		}
		if (m.HasMetallicMap()) connectScaled(addTexture(m.GetMetallicPath()), "R", "Metallic", glm::vec4(m.GetMetallicMultiplier()), false);
		if (m.HasRoughnessMap()) connectScaled(addTexture(m.GetRoughnessPath()), "R", "Roughness", glm::vec4(m.GetRoughnessMultiplier()), false);
		if (m.HasSpecularMap()) connectScaled(addTexture(m.GetSpecularPath()), "R", "Specular", glm::vec4(m.GetSpecularMultiplier()), false);
		if (m.HasAOMap()) connectScaled(addTexture(m.GetAOPath()), "R", "Ambient Occlusion", glm::vec4(m.GetAOMultiplier()), false);
		if (m.HasEmissionMap()) connectScaled(addTexture(m.GetEmissionPath()), "RGB", "Emission", glm::vec4(m.GetEmissionColor(), 1.0f), true);

		// Center the output vertically on the inputs
		graph->FindNode(outID)->Position.y = std::max(0.0f, (y - 190.0f) * 0.5f - 120.0f);
		return graph;
	}

	// ============================================================================
	// GRAPH -> MATERIAL (bake)
	// ============================================================================

	namespace {
		struct Source {
			bool IsTexture = false;
			std::string Path;
			int Channel = -1;              // -1 = RGB, 0..3 = R/G/B/A
			glm::vec4 Value{ 0.0f };       // constant, or factor applied to the texture
		};

		class Resolver {
		public:
			Resolver(const NodeGraph& g, std::vector<std::string>& warnings) : m_Graph(g), m_Warnings(warnings) {}

			Source Resolve(const Pin& input) {
				auto [node, pin] = m_Graph.GetSource(input.PinID);
				if (!node) { Source s; s.Value = input.DefaultValue; return s; }
				return ResolveOutput(*node, *pin, 0);
			}

		private:
			Source ResolveOutput(const Node& node, const Pin& pin, int depth) {
				Source s;
				if (depth > 16) { m_Warnings.push_back("Graph is too deep (cycle?)"); return s; }

				if (node.TypeName == ColorNode) { s.Value = node.Value; return s; }
				if (node.TypeName == FloatNode) { s.Value = glm::vec4(node.Value.x); return s; }
				if (node.TypeName == TextureNode) {
					auto it = node.Properties.find("Path");
					s.Path = it != node.Properties.end() ? it->second : "";
					if (s.Path.empty()) { m_Warnings.push_back("Texture Sample node without a texture"); s.Value = glm::vec4(1.0f); return s; }
					s.IsTexture = true;
					s.Value = glm::vec4(1.0f);
					s.Channel = pin.Name == "R" ? 0 : pin.Name == "G" ? 1 : pin.Name == "B" ? 2 : pin.Name == "A" ? 3 : -1;
					return s;
				}
				if (node.TypeName == MultiplyNode) {
					Source a = ResolveInput(node, "A", depth);
					Source b = ResolveInput(node, "B", depth);
					if (a.IsTexture && b.IsTexture) {
						m_Warnings.push_back("Multiply of two textures needs shader codegen (uses only A)");
						return a;
					}
					Source tex = a.IsTexture ? a : b;
					Source k = a.IsTexture ? b : a;
					if (!a.IsTexture && !b.IsTexture) { s.Value = a.Value * b.Value; return s; }
					tex.Value *= k.Value;
					return tex;
				}
				m_Warnings.push_back("Unsupported node: " + node.Title);
				return s;
			}

			Source ResolveInput(const Node& node, const char* name, int depth) {
				const Pin* in = node.FindInput(name);
				if (!in) return {};
				auto [src, pin] = m_Graph.GetSource(in->PinID);
				if (!src) { Source s; s.Value = in->DefaultValue; return s; }
				return ResolveOutput(*src, *pin, depth + 1);
			}

			const NodeGraph& m_Graph;
			std::vector<std::string>& m_Warnings;
		};

		Ref<Texture2D> LoadTexture(const std::string& path, const Ref<Texture2D>& current, const std::string& currentPath) {
			if (path.empty()) return nullptr;
			if (current && currentPath == path) return current;
			static std::unordered_map<std::string, Ref<Texture2D>> cache;
			auto& slot = cache[path];
			if (!slot || !slot->IsLoaded()) slot = Texture2D::Create(path);
			return (slot && slot->IsLoaded()) ? slot : nullptr;
		}
	}

	std::vector<std::string> Apply(const NodeGraph& graph, MaterialAsset& m) {
		std::vector<std::string> warnings;
		const Node* out = graph.FindNodeByType(OutputNode);
		if (!out) { warnings.push_back("The graph has no Material Output node"); return warnings; }

		Resolver resolver(graph, warnings);
		auto get = [&](const char* name) { return resolver.Resolve(*out->FindInput(name)); };

		// ---- Base color ----
		Source base = get("Base Color");
		if (base.IsTexture) {
			m.SetAlbedoMap(LoadTexture(base.Path, m.GetAlbedoMap(), m.GetAlbedoPath()));
			m.SetAlbedo(glm::vec4(glm::vec3(base.Value), 1.0f));
		}
		else {
			m.SetAlbedoMap(nullptr);
			m.SetAlbedo(glm::vec4(glm::vec3(base.Value), base.Value.a > 0.0f ? base.Value.a : 1.0f));
		}

		// ---- Normal ----
		Source normal = get("Normal");
		m.SetNormalMap(normal.IsTexture ? LoadTexture(normal.Path, m.GetNormalMap(), m.GetNormalPath()) : nullptr);
		m.SetNormalIntensity(get("Normal Strength").Value.x);

		// ---- Scalar channels (metallic / roughness / AO may share a packed ORM texture) ----
		Source metal = get("Metallic"), rough = get("Roughness"), ao = get("Ambient Occlusion"), spec = get("Specular");

		// The packed texture: first texture read through G/B/A, or shared by 2+ scalar inputs
		std::string packedPath;
		{
			std::unordered_map<std::string, int> users;
			for (const Source* s : { &metal, &rough, &ao }) if (s->IsTexture) users[s->Path]++;
			for (const Source* s : { &metal, &rough, &ao })
				if (s->IsTexture && (s->Channel > 0 || users[s->Path] > 1)) { packedPath = s->Path; break; }
		}
		LayeredTextureConfig cfg = m.GetLayeredTextureConfig();
		cfg.UseForMetallic = cfg.UseForRoughness = cfg.UseForAO = false;
		auto toChannel = [](int c) { return static_cast<TextureChannel>(c < 0 ? 0 : c); };
		auto usesPacked = [&](const Source& s) { return s.IsTexture && s.Path == packedPath; };

		if (usesPacked(metal)) { cfg.UseForMetallic = true; cfg.MetallicChannel = toChannel(metal.Channel); m.SetMetallicMap(nullptr); m.SetMetallicMultiplier(metal.Value.x); }
		else if (metal.IsTexture) {
			if (metal.Channel > 0) warnings.push_back("Metallic: only one packed texture is supported");
			m.SetMetallicMap(LoadTexture(metal.Path, m.GetMetallicMap(), m.GetMetallicPath())); m.SetMetallicMultiplier(metal.Value.x);
		}
		else { m.SetMetallicMap(nullptr); m.SetMetallic(metal.Value.x); }

		if (usesPacked(rough)) { cfg.UseForRoughness = true; cfg.RoughnessChannel = toChannel(rough.Channel); m.SetRoughnessMap(nullptr); m.SetRoughnessMultiplier(rough.Value.x); }
		else if (rough.IsTexture) {
			if (rough.Channel > 0) warnings.push_back("Roughness: only one packed texture is supported");
			m.SetRoughnessMap(LoadTexture(rough.Path, m.GetRoughnessMap(), m.GetRoughnessPath())); m.SetRoughnessMultiplier(rough.Value.x);
		}
		else { m.SetRoughnessMap(nullptr); m.SetRoughness(rough.Value.x); }

		if (usesPacked(ao)) { cfg.UseForAO = true; cfg.AOChannel = toChannel(ao.Channel); m.SetAOMap(nullptr); m.SetAOMultiplier(ao.Value.x); }
		else if (ao.IsTexture) {
			if (ao.Channel > 0) warnings.push_back("AO: only one packed texture is supported");
			m.SetAOMap(LoadTexture(ao.Path, m.GetAOMap(), m.GetAOPath())); m.SetAOMultiplier(ao.Value.x);
		}
		else {
			m.SetAOMap(nullptr);
			if (std::abs(ao.Value.x - 1.0f) > 1e-3f) warnings.push_back("Constant AO is ignored (only AO textures are supported)");
		}

		if (!packedPath.empty()) {
			cfg.Texture = LoadTexture(packedPath, cfg.Texture, cfg.Path);
			cfg.Path = cfg.Texture ? packedPath : "";
			cfg.Enabled = cfg.Texture != nullptr;
		}
		else {
			cfg.Enabled = false;
		}
		m.SetLayeredTextureConfig(cfg);

		if (spec.IsTexture) { m.SetSpecularMap(LoadTexture(spec.Path, m.GetSpecularMap(), m.GetSpecularPath())); m.SetSpecularMultiplier(spec.Value.x); }
		else { m.SetSpecularMap(nullptr); m.SetSpecular(spec.Value.x); }

		// ---- Emission ----
		Source emission = get("Emission");
		if (emission.IsTexture) {
			m.SetEmissionMap(LoadTexture(emission.Path, m.GetEmissionMap(), m.GetEmissionPath()));
			m.SetEmissionColor(glm::vec3(emission.Value));
		}
		else {
			m.SetEmissionMap(nullptr);
			m.SetEmissionColor(glm::vec3(emission.Value));
		}
		m.SetEmissionIntensity(get("Emission Strength").Value.x);

		// ---- Subsurface ----
		m.SetSubsurfaceColor(glm::vec3(get("Subsurface Color").Value));
		m.SetSubsurfaceAmount(get("Subsurface").Value.x);

		return warnings;
	}

} // namespace Lunex::MaterialGraph
