#include "stpch.h"
#include "MaterialGraph.h"

#include "MaterialAsset.h"
#include "Renderer/Texture.h"

#include <filesystem>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace Lunex::MaterialGraph {

	using namespace Graph;

	// ============================================================================
	// PIN HELPERS
	// ============================================================================

	namespace {
		Pin In(const char* name, PinType type, const glm::vec4& def = glm::vec4(0.0f), glm::vec2 range = glm::vec2(0.0f)) {
			Pin p; p.Name = name; p.Type = type; p.DefaultValue = def; p.Range = range; return p;
		}
		Pin F(const char* name, float def, float min = 0.0f, float max = 0.0f) { return In(name, PinType::Float, glm::vec4(def), { min, max }); }
		Pin C(const char* name, const glm::vec4& def) { return In(name, PinType::Color, def); }
		Pin V(const char* name, const glm::vec3& def = glm::vec3(0.0f)) { return In(name, PinType::Vec3, glm::vec4(def, 0.0f)); }
		Pin Implicit(const char* name, PinType type) { Pin p = In(name, type); p.HideValue = true; return p; }
		Pin Out(const char* name, PinType type) { Pin p; p.Name = name; p.Type = type; return p; }
		Pin Labeled(Pin p, const char* label) { p.Label = label; return p; }
		Pin P(Pin p, const char* panel) { p.Panel = panel; return p; }

		// Blender node header colors
		const glm::vec4 kInput(0.514f, 0.192f, 0.290f, 1.0f);
		const glm::vec4 kOutput(0.388f, 0.075f, 0.153f, 1.0f);
		const glm::vec4 kShader(0.169f, 0.396f, 0.169f, 1.0f);
		const glm::vec4 kTexture(0.475f, 0.275f, 0.114f, 1.0f);
		const glm::vec4 kColor(0.427f, 0.412f, 0.153f, 1.0f);
		const glm::vec4 kVector(0.235f, 0.235f, 0.514f, 1.0f);
		const glm::vec4 kConverter(0.141f, 0.384f, 0.514f, 1.0f);
		const glm::vec4 kLegacy(0.30f, 0.30f, 0.30f, 1.0f);

		std::string Prop(const Node& n, const char* key, const char* fallback = "") {
			auto it = n.Properties.find(key);
			return it != n.Properties.end() ? it->second : fallback;
		}

		bool IsUnaryMath(const std::string& op) {
			static const char* unary[] = { "Square Root", "Inverse Square Root", "Absolute", "Exponent", "Sign", "Round", "Floor",
				"Ceil", "Truncate", "Fraction", "Sine", "Cosine", "Tangent", "Arcsine", "Arccosine", "Arctangent",
				"Hyperbolic Sine", "Hyperbolic Cosine", "Hyperbolic Tangent", "To Radians", "To Degrees" };
			for (const char* u : unary) if (op == u) return true;
			return false;
		}
		bool MathUsesC(const std::string& op) {
			return op == "Multiply Add" || op == "Compare" || op == "Smooth Minimum" || op == "Smooth Maximum" || op == "Wrap";
		}
	}

	std::string GuessColorSpace(const std::string& path) {
		std::string stem = std::filesystem::path(path).stem().string();
		for (char& c : stem) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		for (char& c : stem) if (c == '-' || c == '.' || c == ' ') c = '_';
		std::vector<std::string> tokens;
		std::stringstream ss(stem);
		for (std::string t; std::getline(ss, t, '_');) if (!t.empty()) tokens.push_back(t);
		static const char* data[] = { "normal", "nor", "nrm", "norm", "nrml", "normalgl", "normaldx", "rough", "roughness", "rgh",
			"metal", "metallic", "metalness", "mtl", "ao", "occlusion", "ambientocclusion", "height", "disp", "displacement", "bump",
			"orm", "arm", "rma", "mra", "mask", "gloss", "glossiness", "spec", "specular", "opacity", "alpha", "data", "packed", "id" };
		for (const auto& t : tokens)
			for (const char* d : data) if (t == d) return "Non-Color";
		return "sRGB";
	}

	// ============================================================================
	// REGISTRY
	// ============================================================================

	const NodeRegistry& GetRegistry() {
		static NodeRegistry registry = [] {
			NodeRegistry r;
			auto reg = [&](const char* type, const char* title, const char* category, glm::vec4 color, float width,
				std::function<void(Node&)> build) -> NodeTypeInfo& {
				NodeTypeInfo info;
				info.TypeName = type; info.Title = title; info.Category = category;
				info.HeaderColor = color; info.Width = width; info.Build = std::move(build);
				r.Register(info);
				return const_cast<NodeTypeInfo&>(*r.Get(type));
			};

			// ---------------- Output ----------------
			auto& output = reg(MaterialOutput, "Material Output", "Output", kOutput, 170.0f, [](Node& n) {
				n.Inputs = { Implicit("Surface", PinType::Shader), Implicit("Volume", PinType::Shader), Implicit("Displacement", PinType::Vec3) };
				n.Properties["Render Method"] = "Auto";
			});
			output.Unique = true;
			output.Enums = { { "Render Method", { "Auto", "Dithered", "Blended" } }, { "Target", { "All", "EEVEE", "Cycles" } } };
			output.Description = "Render Method: Auto blends when the surface uses alpha or transmission; Dithered keeps alpha clip (0.5) in the deferred pass; Blended always uses the sorted transparent pass";
			reg(AOVOutput, "AOV Output", "Output", kOutput, 150.0f, [](Node& n) {
				n.Inputs = { C("Color", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), F("Value", 0.0f) };
				n.Properties["Name"] = "";
			}).Description = "Arbitrary output value (render passes). No effect in the realtime viewport.";

			// ---------------- Shader ----------------
			auto& principled = reg(PrincipledBSDF, "Principled BSDF", "Shader", kShader, 250.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = {
					C("Base Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)),
					F("Metallic", 0.0f, 0.0f, 1.0f),
					F("Roughness", 0.5f, 0.0f, 1.0f),
					F("IOR", 1.5f, 1.0f, 3.0f),
					F("Alpha", 1.0f, 0.0f, 1.0f),
					Implicit("Normal", PinType::Vec3),
					F("Ambient Occlusion", 1.0f, 0.0f, 1.0f),
					P(Labeled(F("Subsurface Weight", 0.0f, 0.0f, 1.0f), "Weight"), "Subsurface"),
					P(Labeled(V("Subsurface Radius", glm::vec3(1.0f, 0.2f, 0.1f)), "Radius"), "Subsurface"),
					P(Labeled(F("Subsurface Scale", 0.05f, 0.0f, 0.0f), "Scale"), "Subsurface"),
					P(Labeled(F("Subsurface IOR", 1.4f, 1.0f, 3.8f), "IOR"), "Subsurface"),
					P(Labeled(F("Subsurface Anisotropy", 0.0f, 0.0f, 1.0f), "Anisotropy"), "Subsurface"),
					P(C("Subsurface Color", glm::vec4(1.0f, 0.35f, 0.25f, 1.0f)), "Subsurface"),
					P(Labeled(F("Specular", 0.5f, 0.0f, 1.0f), "IOR Level"), "Specular"),
					P(Labeled(C("Specular Tint", glm::vec4(1.0f)), "Tint"), "Specular"),
					P(F("Anisotropic", 0.0f, 0.0f, 1.0f), "Specular"),
					P(F("Anisotropic Rotation", 0.0f, 0.0f, 1.0f), "Specular"),
					P(Implicit("Tangent", PinType::Vec3), "Specular"),
					P(Labeled(F("Transmission Weight", 0.0f, 0.0f, 1.0f), "Weight"), "Transmission"),
					P(Labeled(F("Coat Weight", 0.0f, 0.0f, 1.0f), "Weight"), "Coat"),
					P(Labeled(F("Coat Roughness", 0.03f, 0.0f, 1.0f), "Roughness"), "Coat"),
					P(Labeled(F("Coat IOR", 1.5f, 1.0f, 4.0f), "IOR"), "Coat"),
					P(Labeled(C("Coat Tint", glm::vec4(1.0f)), "Tint"), "Coat"),
					P(Labeled(Implicit("Coat Normal", PinType::Vec3), "Normal"), "Coat"),
					P(Labeled(F("Sheen Weight", 0.0f, 0.0f, 1.0f), "Weight"), "Sheen"),
					P(Labeled(F("Sheen Roughness", 0.5f, 0.0f, 1.0f), "Roughness"), "Sheen"),
					P(Labeled(C("Sheen Tint", glm::vec4(1.0f)), "Tint"), "Sheen"),
					P(Labeled(C("Emission Color", glm::vec4(1.0f)), "Color"), "Emission"),
					P(Labeled(F("Emission Strength", 0.0f, 0.0f, 0.0f), "Strength"), "Emission"),
					P(Labeled(F("Thin Film Thickness", 0.0f, 0.0f, 100000.0f), "Thickness"), "Thin Film"),
					P(Labeled(F("Thin Film IOR", 1.33f, 1.0f, 3.0f), "IOR"), "Thin Film"),
				};
				n.Properties["Distribution"] = "Multiscatter GGX";
				n.Properties["Subsurface Method"] = "Random Walk";
			});
			principled.Enums = { { "Distribution", { "GGX", "Multiscatter GGX" } }, { "Subsurface Method", { "Christensen-Burley", "Random Walk", "Random Walk (Skin)" } } };
			principled.Description = "Blender 4.x Principled BSDF: base, IOR, alpha, subsurface, specular, transmission, coat, sheen, emission, thin film";
			reg(EmissionShader, "Emission", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("Emission", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), F("Strength", 1.0f) };
			});
			reg(MixShader, "Mix Shader", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("Shader", PinType::Shader) };
				n.Inputs = { F("Fac", 0.5f, 0.0f, 1.0f), Labeled(Implicit("Shader A", PinType::Shader), "Shader"), Labeled(Implicit("Shader B", PinType::Shader), "Shader") };
			});
			reg(AddShader, "Add Shader", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("Shader", PinType::Shader) };
				n.Inputs = { Labeled(Implicit("Shader A", PinType::Shader), "Shader"), Labeled(Implicit("Shader B", PinType::Shader), "Shader") };
			});
			reg(DiffuseBSDF, "Diffuse BSDF", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Roughness", 0.0f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3) };
			});
			auto& glossy = reg(GlossyBSDF, "Glossy BSDF", "Shader", kShader, 170.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Roughness", 0.5f, 0.0f, 1.0f), F("Anisotropy", 0.0f, -1.0f, 1.0f),
					F("Rotation", 0.0f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3), Implicit("Tangent", PinType::Vec3) };
				n.Properties["Distribution"] = "Multiscatter GGX";
			});
			glossy.Enums = { { "Distribution", { "Sharp", "Beckmann", "GGX", "Ashikhmin-Shirley", "Multiscatter GGX" } } };
			auto& metallic = reg(MetallicBSDF, "Metallic BSDF", "Shader", kShader, 180.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Base Color", glm::vec4(0.617f, 0.577f, 0.540f, 1.0f)), C("Edge Tint", glm::vec4(0.695f, 0.726f, 0.770f, 1.0f)),
					F("Roughness", 0.5f, 0.0f, 1.0f), F("Anisotropy", 0.0f, -1.0f, 1.0f), F("Rotation", 0.0f, 0.0f, 1.0f),
					Implicit("Normal", PinType::Vec3), Implicit("Tangent", PinType::Vec3) };
				n.Properties["Fresnel Type"] = "F82 Tint";
			});
			metallic.Enums = { { "Fresnel Type", { "Physical Conductor", "F82 Tint" } }, { "Distribution", { "Beckmann", "GGX", "Multiscatter GGX" } } };
			auto& glass = reg(GlassBSDF, "Glass BSDF", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), F("Roughness", 0.0f, 0.0f, 1.0f), F("IOR", 1.5f, 1.0f, 3.0f), Implicit("Normal", PinType::Vec3) };
				n.Properties["Distribution"] = "Multiscatter GGX";
			});
			glass.Enums = { { "Distribution", { "Sharp", "Beckmann", "GGX", "Multiscatter GGX" } } };
			glass.Description = "Reflection + refraction (IOR). Rendered in the sorted transparent pass with screen-space refraction";
			reg(RefractionBSDF, "Refraction BSDF", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), F("Roughness", 0.0f, 0.0f, 1.0f), F("IOR", 1.45f, 1.0f, 3.0f), Implicit("Normal", PinType::Vec3) };
			});
			reg(TransparentBSDF, "Transparent BSDF", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(1.0f)) };
			}).Description = "Light passes straight through, tinted by Color (mix with another shader for partial transparency)";
			reg(TranslucentBSDF, "Translucent BSDF", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), Implicit("Normal", PinType::Vec3) };
			}).Description = "Diffuse light transmitted through thin surfaces (leaves, paper, cloth)";
			reg(SubsurfaceBSDF, "Subsurface Scattering", "Shader", kShader, 180.0f, [](Node& n) {
				n.Outputs = { Out("BSSRDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Scale", 0.05f), V("Radius", glm::vec3(1.0f, 0.2f, 0.1f)),
					F("IOR", 1.4f, 1.0f, 3.8f), F("Roughness", 1.0f, 0.0f, 1.0f), F("Anisotropy", 0.0f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3) };
			});
			reg(SheenBSDF, "Sheen BSDF", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Roughness", 0.5f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3) };
			});
			auto& toon = reg(ToonBSDF, "Toon BSDF", "Shader", kShader, 150.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Size", 0.5f, 0.0f, 1.0f), F("Smooth", 0.0f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3) };
				n.Properties["Component"] = "Diffuse";
			});
			toon.Enums = { { "Component", { "Diffuse", "Glossy" } } };
			reg(HairBSDF, "Hair BSDF", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Offset", 0.0f), F("Roughness U", 0.1f, 0.0f, 1.0f), F("Roughness V", 1.0f, 0.0f, 1.0f), Implicit("Tangent", PinType::Vec3) };
			});
			reg(PrincipledHair, "Principled Hair BSDF", "Shader", kShader, 190.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.017513f, 0.005763f, 0.002059f, 1.0f)), F("Melanin", 0.8f, 0.0f, 1.0f), F("Melanin Redness", 1.0f, 0.0f, 1.0f),
					C("Tint", glm::vec4(1.0f)), F("Roughness", 0.3f, 0.0f, 1.0f), F("Radial Roughness", 0.3f, 0.0f, 1.0f), F("Coat", 0.0f, 0.0f, 1.0f), F("IOR", 1.55f, 0.0f, 3.0f) };
			});
			reg(SpecularBSDF, "Specular BSDF", "Shader", kShader, 190.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Base Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), C("Specular", glm::vec4(0.03f, 0.03f, 0.03f, 1.0f)), F("Roughness", 0.2f, 0.0f, 1.0f),
					C("Emissive Color", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), F("Transparency", 0.0f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3),
					F("Clear Coat", 0.0f, 0.0f, 1.0f), F("Clear Coat Roughness", 0.0f, 0.0f, 1.0f), Implicit("Clear Coat Normal", PinType::Vec3), F("Ambient Occlusion", 1.0f, 0.0f, 1.0f) };
			}).Description = "EEVEE specular workflow";
			reg(HoldoutShader, "Holdout", "Shader", kShader, 140.0f, [](Node& n) {
				n.Outputs = { Out("Holdout", PinType::Shader) };
				n.Inputs = { F("Weight", 1.0f, 0.0f, 1.0f) };
			});
			reg(RayPortalBSDF, "Ray Portal BSDF", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), Implicit("Position", PinType::Vec3), Implicit("Direction", PinType::Vec3) };
			}).Description = "Cycles only: rendered as a transparent surface in the realtime viewport";
			reg(PrincipledVolume, "Principled Volume", "Shader", kShader, 190.0f, [](Node& n) {
				n.Outputs = { Out("Volume", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)), F("Density", 1.0f), F("Anisotropy", 0.0f, -1.0f, 1.0f),
					C("Absorption Color", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), F("Emission Strength", 0.0f), C("Emission Color", glm::vec4(1.0f)),
					F("Blackbody Intensity", 0.0f, 0.0f, 1.0f), C("Blackbody Tint", glm::vec4(1.0f)), F("Temperature", 1000.0f) };
			}).Description = "Volumes are not rendered by the raster engine yet (connect to Material Output > Volume)";
			reg(VolumeAbsorption, "Volume Absorption", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("Volume", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Density", 1.0f) };
			});
			reg(VolumeScatter, "Volume Scatter", "Shader", kShader, 160.0f, [](Node& n) {
				n.Outputs = { Out("Volume", PinType::Shader) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), F("Density", 1.0f), F("Anisotropy", 0.0f, -1.0f, 1.0f) };
			});

			// ---------------- Input ----------------
			reg(TexCoordNode, "Texture Coordinate", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Generated", PinType::Vec3), Out("Normal", PinType::Vec3), Out("UV", PinType::Vec3), Out("Object", PinType::Vec3),
					Out("Camera", PinType::Vec3), Out("Window", PinType::Vec3), Out("Reflection", PinType::Vec3) };
			});
			reg(GeometryNode, "Geometry", "Input", kInput, 160.0f, [](Node& n) {
				n.Outputs = { Out("Position", PinType::Vec3), Out("Normal", PinType::Vec3), Out("Tangent", PinType::Vec3), Out("True Normal", PinType::Vec3),
					Out("Incoming", PinType::Vec3), Out("Parametric", PinType::Vec3), Out("Backfacing", PinType::Float), Out("Pointiness", PinType::Float),
					Out("Random Per Island", PinType::Float) };
			});
			reg(ValueNode, "Value", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Value", PinType::Float) };
				n.Value = glm::vec4(0.5f);
				n.Properties["Name"] = "";
			}).Description = "Constant. Give it a Name to edit it per entity from scripts.";
			reg(RGBNode, "RGB", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Value = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
				n.Properties["Name"] = "";
			}).Description = "Constant color. Give it a Name to edit it per entity from scripts.";
			reg(FresnelNode, "Fresnel", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Fac", PinType::Float) };
				n.Inputs = { F("IOR", 1.5f, 1.0f, 3.0f), Implicit("Normal", PinType::Vec3) };
			});
			reg(LayerWeight, "Layer Weight", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Fresnel", PinType::Float), Out("Facing", PinType::Float) };
				n.Inputs = { F("Blend", 0.5f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3) };
			});
			reg(TimeNode, "Time", "Input", kInput, 140.0f, [](Node& n) {
				n.Outputs = { Out("Seconds", PinType::Float) };
			}).Description = "Elapsed time in seconds (animated materials)";
			auto& ao = reg(AmbientOcclusionNode, "Ambient Occlusion", "Input", kInput, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("AO", PinType::Float) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), F("Distance", 1.0f), Implicit("Normal", PinType::Vec3) };
				n.Properties["Inside"] = "0";
				n.Properties["Only Local"] = "0";
			});
			ao.Toggles = { "Inside", "Only Local" };
			ao.Description = "Screen-space AO is applied by the renderer; AO output = 1 in the raster engine";
			auto& attr = reg(AttributeNode, "Attribute", "Input", kInput, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Vector", PinType::Vec3), Out("Fac", PinType::Float), Out("Alpha", PinType::Float) };
				n.Properties["Name"] = "UVMap";
				n.Properties["Type"] = "Geometry";
			});
			attr.Enums = { { "Type", { "Geometry", "Object", "Instancer", "View Layer" } } };
			attr.Description = "Supported names: UVMap / uv (texture coordinates), position, normal";
			reg(BevelNode, "Bevel", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Normal", PinType::Vec3) };
				n.Inputs = { F("Radius", 0.05f), Implicit("Normal", PinType::Vec3) };
				n.Properties["Samples"] = "4";
			});
			reg(CameraDataNode, "Camera Data", "Input", kInput, 160.0f, [](Node& n) {
				n.Outputs = { Out("View Vector", PinType::Vec3), Out("View Z Depth", PinType::Float), Out("View Distance", PinType::Float) };
			});
			reg(ColorAttributeNode, "Color Attribute", "Input", kInput, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Alpha", PinType::Float) };
				n.Properties["Layer"] = "";
			}).Description = "Vertex colors are not imported yet: white";
			reg(CurvesInfoNode, "Curves Info", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Is Strand", PinType::Float), Out("Intercept", PinType::Float), Out("Length", PinType::Float), Out("Thickness", PinType::Float),
					Out("Tangent Normal", PinType::Vec3), Out("Random", PinType::Float) };
			});
			reg(LightPathNode, "Light Path", "Input", kInput, 170.0f, [](Node& n) {
				n.Outputs = { Out("Is Camera Ray", PinType::Float), Out("Is Shadow Ray", PinType::Float), Out("Is Diffuse Ray", PinType::Float),
					Out("Is Glossy Ray", PinType::Float), Out("Is Singular Ray", PinType::Float), Out("Is Reflection Ray", PinType::Float),
					Out("Is Transmission Ray", PinType::Float), Out("Ray Length", PinType::Float), Out("Ray Depth", PinType::Float),
					Out("Diffuse Depth", PinType::Float), Out("Glossy Depth", PinType::Float), Out("Transparent Depth", PinType::Float),
					Out("Transmission Depth", PinType::Float) };
			}).Description = "Raster engine: only camera rays (Is Camera Ray = 1)";
			reg(ObjectInfoNode, "Object Info", "Input", kInput, 160.0f, [](Node& n) {
				n.Outputs = { Out("Location", PinType::Vec3), Out("Color", PinType::Color), Out("Alpha", PinType::Float), Out("Object Index", PinType::Float),
					Out("Material Index", PinType::Float), Out("Random", PinType::Float) };
			});
			reg(ParticleInfoNode, "Particle Info", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Index", PinType::Float), Out("Random", PinType::Float), Out("Age", PinType::Float), Out("Lifetime", PinType::Float),
					Out("Location", PinType::Vec3), Out("Size", PinType::Float), Out("Velocity", PinType::Vec3), Out("Angular Velocity", PinType::Vec3) };
			});
			reg(PointInfoNode, "Point Info", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Position", PinType::Vec3), Out("Radius", PinType::Float), Out("Random", PinType::Float) };
			});
			auto& tangent = reg(TangentNode, "Tangent", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Tangent", PinType::Vec3) };
				n.Properties["Direction Type"] = "UV Map";
				n.Properties["Axis"] = "Z";
			});
			tangent.Enums = { { "Direction Type", { "Radial", "UV Map" } }, { "Axis", { "X", "Y", "Z" } } };
			reg(UVMapNode, "UV Map", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("UV", PinType::Vec3) };
				n.Properties["UV Map"] = "";
				n.Properties["From Instancer"] = "0";
			}).Toggles = { "From Instancer" };
			reg(VolumeInfoNode, "Volume Info", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Density", PinType::Float), Out("Flame", PinType::Float), Out("Temperature", PinType::Float) };
			});
			reg(WireframeNode, "Wireframe", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Fac", PinType::Float) };
				n.Inputs = { F("Size", 0.01f) };
				n.Properties["Pixel Size"] = "0";
			}).Toggles = { "Pixel Size" };

			// ---------------- Texture ----------------
			auto& img = reg(ImageTexture, "Image Texture", "Texture", kTexture, 240.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Alpha", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3) };
				n.Properties["Path"] = "";
				n.Properties["Interpolation"] = "Linear";
				n.Value.x = 0.0f;   // box projection blend
				// "Color Space" is left unset on purpose: guessed from the file name until the user picks one
			});
			img.Enums = {
				{ "Interpolation", { "Linear", "Closest", "Cubic" } },
				{ "Projection", { "Flat", "Box", "Sphere", "Tube" } },
				{ "Extension", { "Repeat", "Extend", "Clip", "Mirror" } },
				{ "Color Space", { "sRGB", "Linear Rec.709", "Non-Color" } },
				{ "Alpha", { "Straight", "Premultiplied", "Channel Packed", "None" } },
			};
			img.Description = "Color Space: sRGB for colors (albedo, emission), Non-Color for data (normal, roughness, metallic, AO, height, packed ORM)";
			auto& noise = reg(NoiseTexture, "Noise Texture", "Texture", kTexture, 170.0f, [](Node& n) {
				n.Outputs = { Out("Fac", PinType::Float), Out("Color", PinType::Color) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("W", 0.0f), F("Scale", 5.0f), F("Detail", 2.0f, 0.0f, 15.0f), F("Roughness", 0.5f, 0.0f, 1.0f),
					F("Lacunarity", 2.0f, 0.0f, 0.0f), F("Offset", 0.0f), F("Gain", 1.0f), F("Distortion", 0.0f) };
				n.Properties["Dimensions"] = "3D";
				n.Properties["Type"] = "fBM";
				n.Properties["Normalize"] = "1";
			});
			noise.Enums = { { "Dimensions", { "1D", "2D", "3D", "4D" } }, { "Type", { "Multifractal", "Ridged Multifractal", "Hybrid Multifractal", "fBM", "Hetero Terrain" } } };
			noise.Toggles = { "Normalize" };
			noise.PinVisible = [](const Node& n, const Pin& p) {
				const std::string d = Prop(n, "Dimensions", "3D"), t = Prop(n, "Type", "fBM");
				if (p.Name == "W") return d == "1D" || d == "4D";
				if (p.Name == "Vector") return d != "1D";
				if (p.Name == "Offset") return t == "Ridged Multifractal" || t == "Hybrid Multifractal" || t == "Hetero Terrain";
				if (p.Name == "Gain") return t == "Ridged Multifractal" || t == "Hybrid Multifractal";
				return true;
			};
			auto& voronoi = reg(VoronoiTexture, "Voronoi Texture", "Texture", kTexture, 180.0f, [](Node& n) {
				n.Outputs = { Out("Distance", PinType::Float), Out("Color", PinType::Color), Out("Position", PinType::Vec3), Out("W", PinType::Float), Out("Radius", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("W", 0.0f), F("Scale", 5.0f), F("Detail", 0.0f, 0.0f, 15.0f), F("Roughness", 0.5f, 0.0f, 1.0f),
					F("Lacunarity", 2.0f), F("Smoothness", 1.0f, 0.0f, 1.0f), F("Exponent", 0.5f, 0.0f, 32.0f), F("Randomness", 1.0f, 0.0f, 1.0f) };
				n.Properties["Dimensions"] = "3D";
				n.Properties["Feature"] = "F1";
				n.Properties["Distance"] = "Euclidean";
				n.Properties["Normalize"] = "0";
			});
			voronoi.Enums = { { "Dimensions", { "1D", "2D", "3D", "4D" } }, { "Feature", { "F1", "F2", "Smooth F1", "Distance to Edge", "N-Sphere Radius" } },
				{ "Distance", { "Euclidean", "Manhattan", "Chebychev", "Minkowski" } } };
			voronoi.Toggles = { "Normalize" };
			voronoi.PinVisible = [](const Node& n, const Pin& p) {
				const std::string d = Prop(n, "Dimensions", "3D"), f = Prop(n, "Feature", "F1"), m = Prop(n, "Distance", "Euclidean");
				if (p.Name == "W") return d == "1D" || d == "4D";
				if (p.Name == "Vector") return d != "1D";
				if (p.Name == "Smoothness") return f == "Smooth F1";
				if (p.Name == "Exponent") return m == "Minkowski";
				if (p.Name == "Radius") return f == "N-Sphere Radius";
				if (p.Name == "Color" || p.Name == "Position") return f != "Distance to Edge" && f != "N-Sphere Radius";
				return true;
			};
			auto& wave = reg(WaveTexture, "Wave Texture", "Texture", kTexture, 170.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Scale", 5.0f), F("Distortion", 0.0f), F("Detail", 2.0f, 0.0f, 15.0f), F("Detail Scale", 1.0f),
					F("Detail Roughness", 0.5f, 0.0f, 1.0f), F("Phase Offset", 0.0f) };
				n.Properties["Type"] = "Bands";
				n.Properties["Direction"] = "X";
				n.Properties["Profile"] = "Sine";
			});
			wave.Enums = { { "Type", { "Bands", "Rings" } }, { "Direction", { "X", "Y", "Z", "Diagonal" } }, { "Profile", { "Sine", "Saw", "Triangle" } } };
			reg(CheckerTexture, "Checker Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), C("Color1", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), C("Color2", glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)), F("Scale", 5.0f) };
			});
			auto& grad = reg(GradientTexture, "Gradient Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3) };
				n.Properties["Type"] = "Linear";
			});
			grad.Enums = { { "Type", { "Linear", "Quadratic", "Easing", "Diagonal", "Spherical", "Quadratic Sphere", "Radial" } } };
			auto& brick = reg(BrickTexture, "Brick Texture", "Texture", kTexture, 180.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), C("Color1", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), C("Color2", glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)),
					C("Mortar", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), F("Scale", 5.0f), F("Mortar Size", 0.02f, 0.0f, 0.125f), F("Mortar Smooth", 0.1f, 0.0f, 1.0f),
					F("Bias", 0.0f, -1.0f, 1.0f), F("Brick Width", 0.5f, 0.01f, 100.0f), F("Row Height", 0.25f, 0.01f, 100.0f) };
				n.Properties["Offset"] = "0.5";
				n.Properties["Offset Frequency"] = "2";
				n.Properties["Squash"] = "1.0";
				n.Properties["Squash Frequency"] = "2";
			});
			brick.Description = "Offset / Squash (and their frequencies) are edited in the node details";
			auto& env = reg(EnvironmentTexture, "Environment Texture", "Texture", kTexture, 240.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { Implicit("Vector", PinType::Vec3) };
				n.Properties["Path"] = "";
				n.Properties["Projection"] = "Equirectangular";
			});
			env.Enums = { { "Interpolation", { "Linear", "Closest", "Cubic" } }, { "Projection", { "Equirectangular", "Mirror Ball" } }, { "Color Space", { "sRGB", "Linear Rec.709", "Non-Color" } } };
			auto& gabor = reg(GaborTexture, "Gabor Texture", "Texture", kTexture, 170.0f, [](Node& n) {
				n.Outputs = { Out("Value", PinType::Float), Out("Phase", PinType::Float), Out("Intensity", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Scale", 5.0f), F("Frequency", 2.0f), F("Anisotropy", 1.0f, 0.0f, 1.0f),
					F("Orientation", 0.785398f), V("Orientation 3D", glm::vec3(1.414214f, 1.414214f, 0.0f)) };
				n.Properties["Type"] = "2D";
			});
			gabor.Enums = { { "Type", { "2D", "3D" } } };
			gabor.PinVisible = [](const Node& n, const Pin& p) {
				const bool is3D = Prop(n, "Type", "2D") == "3D";
				if (p.Name == "Orientation") return !is3D;
				if (p.Name == "Orientation 3D") return is3D;
				return true;
			};
			reg(IESTexture, "IES Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Strength", 1.0f) };
				n.Properties["Path"] = "";
			}).Description = "Light profiles: used on lights; on surfaces it returns Strength";
			reg(MagicTexture, "Magic Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Scale", 5.0f), F("Distortion", 1.0f) };
				n.Properties["Depth"] = "2";
			}).Description = "Depth (0-10) is edited in the node details";
			reg(PointDensityTexture, "Point Density", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Density", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3) };
			}).Description = "Cycles only (particle / vertex density): returns 0";
			auto& sky = reg(SkyTexture, "Sky Texture", "Texture", kTexture, 180.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Sun Size", 0.545f, 0.0f, 90.0f), F("Sun Intensity", 1.0f), F("Sun Elevation", 15.0f, -90.0f, 90.0f),
					F("Sun Rotation", 0.0f, 0.0f, 360.0f), F("Altitude", 0.0f), F("Air", 1.0f, 0.0f, 10.0f), F("Dust", 1.0f, 0.0f, 10.0f), F("Ozone", 1.0f, 0.0f, 10.0f) };
				n.Properties["Sky Type"] = "Nishita";
				n.Properties["Sun Disc"] = "1";
			});
			sky.Enums = { { "Sky Type", { "Nishita", "Preetham", "Hosek / Wilkie" } } };
			sky.Toggles = { "Sun Disc" };
			auto& white = reg(WhiteNoiseTexture, "White Noise Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Value", PinType::Float), Out("Color", PinType::Color) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("W", 0.0f) };
				n.Properties["Dimensions"] = "3D";
			});
			white.Enums = { { "Dimensions", { "1D", "2D", "3D", "4D" } } };
			white.PinVisible = [](const Node& n, const Pin& p) {
				const std::string d = Prop(n, "Dimensions", "3D");
				if (p.Name == "W") return d == "1D" || d == "4D";
				if (p.Name == "Vector") return d != "1D";
				return true;
			};

			// ---------------- Color ----------------
			auto& mix = reg(MixNode, "Mix", "Color", kColor, 160.0f, [](Node& n) {
				n.Outputs = { Labeled(Out("Result", PinType::Color), "Result"), Labeled(Out("Result Float", PinType::Float), "Result"), Labeled(Out("Result Vector", PinType::Vec3), "Result") };
				n.Inputs = {
					F("Factor", 0.5f, 0.0f, 1.0f),
					Labeled(C("A", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)), "A"), Labeled(C("B", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)), "B"),
					Labeled(F("A Float", 0.0f), "A"), Labeled(F("B Float", 0.0f), "B"),
					Labeled(V("A Vector"), "A"), Labeled(V("B Vector"), "B"),
				};
				n.Properties["Data Type"] = "Color";
				n.Properties["Blending"] = "Mix";
				n.Properties["Clamp Result"] = "0";
			});
			mix.Enums = {
				{ "Data Type", { "Color", "Float", "Vector" } },
				{ "Blending", { "Mix", "Darken", "Multiply", "Color Burn", "Lighten", "Screen", "Color Dodge", "Add", "Overlay",
				                "Soft Light", "Linear Light", "Difference", "Exclusion", "Subtract", "Divide", "Hue", "Saturation", "Color", "Value" } },
			};
			mix.Toggles = { "Clamp Result" };
			mix.PinVisible = [](const Node& n, const Pin& p) {
				const std::string t = Prop(n, "Data Type", "Color");
				if (p.Name == "Factor") return true;
				const bool isFloat = p.Name.find("Float") != std::string::npos;
				const bool isVector = p.Name.find("Vector") != std::string::npos;
				if (t == "Float") return isFloat;
				if (t == "Vector") return isVector;
				return !isFloat && !isVector;
			};
			reg(InvertNode, "Invert Color", "Color", kColor, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { F("Fac", 1.0f, 0.0f, 1.0f), C("Color", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) };
			});
			reg(HueSatValNode, "Hue/Saturation/Value", "Color", kColor, 170.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { F("Hue", 0.5f, 0.0f, 1.0f), F("Saturation", 1.0f, 0.0f, 2.0f), F("Value", 1.0f, 0.0f, 2.0f), F("Fac", 1.0f, 0.0f, 1.0f), C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)) };
			});
			reg(BrightContrast, "Brightness/Contrast", "Color", kColor, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), F("Bright", 0.0f), F("Contrast", 0.0f) };
			});
			reg(GammaNode, "Gamma", "Color", kColor, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { C("Color", glm::vec4(1.0f)), F("Gamma", 1.0f, 0.001f, 10.0f) };
			});

			// ---------------- Vector ----------------
			auto& mapping = reg(MappingNode, "Mapping", "Vector", kVector, 170.0f, [](Node& n) {
				n.Outputs = { Out("Vector", PinType::Vec3) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), V("Location"), V("Rotation"), V("Scale", glm::vec3(1.0f)) };
				n.Properties["Type"] = "Point";
			});
			mapping.Enums = { { "Type", { "Point", "Texture", "Vector", "Normal" } } };
			reg(NormalMapNode, "Normal Map", "Vector", kVector, 160.0f, [](Node& n) {
				n.Outputs = { Out("Normal", PinType::Vec3) };
				n.Inputs = { F("Strength", 1.0f, 0.0f, 10.0f), C("Color", glm::vec4(0.5f, 0.5f, 1.0f, 1.0f)) };
			});
			auto& bump = reg(BumpNode, "Bump", "Vector", kVector, 160.0f, [](Node& n) {
				n.Outputs = { Out("Normal", PinType::Vec3) };
				n.Inputs = { F("Strength", 1.0f, 0.0f, 1.0f), F("Distance", 1.0f), F("Height", 1.0f), Implicit("Normal", PinType::Vec3) };
				n.Properties["Invert"] = "0";
			});
			bump.Toggles = { "Invert" };
			auto& vmath = reg(VectorMathNode, "Vector Math", "Vector", kVector, 160.0f, [](Node& n) {
				n.Outputs = { Out("Vector", PinType::Vec3), Out("Value", PinType::Float) };
				n.Inputs = { V("A"), V("B"), V("C"), F("Scale", 1.0f) };
				n.Properties["Operation"] = "Add";
			});
			vmath.Enums = { { "Operation", { "Add", "Subtract", "Multiply", "Divide", "Multiply Add", "Cross Product", "Project", "Reflect",
				"Dot Product", "Distance", "Length", "Scale", "Normalize", "Absolute", "Minimum", "Maximum", "Floor", "Ceil",
				"Fraction", "Modulo", "Snap", "Sine", "Cosine", "Tangent" } } };
			vmath.PinVisible = [](const Node& n, const Pin& p) {
				const std::string op = Prop(n, "Operation", "Add");
				const bool unary = op == "Length" || op == "Normalize" || op == "Absolute" || op == "Floor" || op == "Ceil" ||
					op == "Fraction" || op == "Sine" || op == "Cosine" || op == "Tangent" || op == "Scale";
				if (p.Name == "A") return true;
				if (p.Name == "B") return !unary;
				if (p.Name == "C") return op == "Multiply Add";
				if (p.Name == "Scale") return op == "Scale";
				if (p.Name == "Value") return op == "Dot Product" || op == "Distance" || op == "Length";
				if (p.Name == "Vector") return !(op == "Dot Product" || op == "Distance" || op == "Length");
				return true;
			};

			// ---------------- Converter ----------------
			auto& math = reg(MathNode, "Math", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Value", PinType::Float) };
				n.Inputs = { Labeled(F("A", 0.5f), "Value"), Labeled(F("B", 0.5f), "Value"), Labeled(F("C", 0.5f), "Value") };
				n.Properties["Operation"] = "Add";
				n.Properties["Clamp"] = "0";
			});
			math.Enums = { { "Operation", { "Add", "Subtract", "Multiply", "Divide", "Multiply Add", "Power", "Logarithm",
				"Square Root", "Inverse Square Root", "Absolute", "Exponent", "Minimum", "Maximum", "Less Than", "Greater Than",
				"Sign", "Compare", "Smooth Minimum", "Smooth Maximum", "Round", "Floor", "Ceil", "Truncate", "Fraction",
				"Truncated Modulo", "Floored Modulo", "Wrap", "Snap", "Ping-Pong", "Sine", "Cosine", "Tangent", "Arcsine",
				"Arccosine", "Arctangent", "Arctan2", "Hyperbolic Sine", "Hyperbolic Cosine", "Hyperbolic Tangent",
				"To Radians", "To Degrees" } } };
			math.Toggles = { "Clamp" };
			math.PinVisible = [](const Node& n, const Pin& p) {
				const std::string op = Prop(n, "Operation", "Add");
				if (p.Name == "B") return !IsUnaryMath(op);
				if (p.Name == "C") return MathUsesC(op);
				return true;
			};
			auto& mapRange = reg(MapRangeNode, "Map Range", "Converter", kConverter, 160.0f, [](Node& n) {
				n.Outputs = { Out("Result", PinType::Float) };
				n.Inputs = { F("Value", 1.0f), F("From Min", 0.0f), F("From Max", 1.0f), F("To Min", 0.0f), F("To Max", 1.0f), F("Steps", 4.0f) };
				n.Properties["Interpolation"] = "Linear";
				n.Properties["Clamp"] = "1";
			});
			mapRange.Enums = { { "Interpolation", { "Linear", "Stepped Linear", "Smooth Step", "Smoother Step" } } };
			mapRange.Toggles = { "Clamp" };
			mapRange.PinVisible = [](const Node& n, const Pin& p) {
				return p.Name != "Steps" || Prop(n, "Interpolation", "Linear") == "Stepped Linear";
			};
			auto& clamp = reg(ClampNode, "Clamp", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Labeled(Out("Result", PinType::Float), "Result") };
				n.Inputs = { F("Value", 1.0f), F("Min", 0.0f), F("Max", 1.0f) };
				n.Properties["Clamp Type"] = "Min Max";
			});
			clamp.Enums = { { "Clamp Type", { "Min Max", "Range" } } };
			auto& ramp = reg(ColorRampNode, "Color Ramp", "Converter", kConverter, 240.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Alpha", PinType::Float) };
				n.Inputs = { F("Fac", 0.5f, 0.0f, 1.0f) };
				n.Properties["Stops"] = "0:0,0,0,1;1:1,1,1,1";
				n.Properties["Interpolation"] = "Linear";
			});
			ramp.Enums = { { "Interpolation", { "Linear", "Ease", "Constant" } } };
			auto& sep = reg(SeparateColor, "Separate Color", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Red", PinType::Float), Out("Green", PinType::Float), Out("Blue", PinType::Float), Out("Alpha", PinType::Float) };
				n.Inputs = { C("Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)) };
				n.Properties["Mode"] = "RGB";
			});
			sep.Enums = { { "Mode", { "RGB", "HSV" } } };
			auto& comb = reg(CombineColor, "Combine Color", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { F("Red", 0.0f, 0.0f, 1.0f), F("Green", 0.0f, 0.0f, 1.0f), F("Blue", 0.0f, 0.0f, 1.0f) };
				n.Properties["Mode"] = "RGB";
			});
			comb.Enums = { { "Mode", { "RGB", "HSV" } } };
			reg(SeparateXYZ, "Separate XYZ", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("X", PinType::Float), Out("Y", PinType::Float), Out("Z", PinType::Float) };
				n.Inputs = { V("Vector") };
			});
			reg(CombineXYZ, "Combine XYZ", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Vector", PinType::Vec3) };
				n.Inputs = { F("X", 0.0f), F("Y", 0.0f), F("Z", 0.0f) };
			});
			reg(RGBToBW, "RGB to BW", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Val", PinType::Float) };
				n.Inputs = { C("Color", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)) };
			});

			// ---- Color (Blender 4.x additions) ----
			auto& falloff = reg(LightFalloffNode, "Light Falloff", "Color", kColor, 160.0f, [](Node& n) {
				n.Outputs = { Out("Quadratic", PinType::Float), Out("Linear", PinType::Float), Out("Constant", PinType::Float) };
				n.Inputs = { F("Strength", 100.0f), F("Smooth", 0.0f) };
			});
			falloff.Description = "Ray length = distance to the camera in the raster engine";
			reg(RGBCurvesNode, "RGB Curves", "Color", kColor, 240.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { F("Fac", 1.0f, 0.0f, 1.0f), C("Color", glm::vec4(1.0f)) };
				n.Properties["Curve C"] = "0,0;1,1";
				n.Properties["Curve R"] = "0,0;1,1";
				n.Properties["Curve G"] = "0,0;1,1";
				n.Properties["Curve B"] = "0,0;1,1";
				n.Properties["Channel"] = "C";
			});

			// ---- Vector (Blender 4.x additions) ----
			auto& disp = reg(DisplacementNode, "Displacement", "Vector", kVector, 160.0f, [](Node& n) {
				n.Outputs = { Out("Displacement", PinType::Vec3) };
				n.Inputs = { F("Height", 0.0f), F("Midlevel", 0.5f), F("Scale", 1.0f), Implicit("Normal", PinType::Vec3) };
				n.Properties["Space"] = "Object";
			});
			disp.Enums = { { "Space", { "Object", "World" } } };
			disp.Description = "Connect to Material Output > Displacement (applied as bump in the raster engine, like EEVEE)";
			auto& vdisp = reg(VectorDisplacementNode, "Vector Displacement", "Vector", kVector, 170.0f, [](Node& n) {
				n.Outputs = { Out("Displacement", PinType::Vec3) };
				n.Inputs = { C("Vector", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), F("Midlevel", 0.0f), F("Scale", 1.0f) };
				n.Properties["Space"] = "Tangent";
			});
			vdisp.Enums = { { "Space", { "Tangent", "Object", "World" } } };
			reg(NormalNode, "Normal", "Vector", kVector, 150.0f, [](Node& n) {
				n.Outputs = { Out("Normal", PinType::Vec3), Out("Dot", PinType::Float) };
				n.Inputs = { V("Normal", glm::vec3(0.0f, 0.0f, 1.0f)) };
				n.Value = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
			}).Description = "Normal output = the direction set in the node; Dot = dot(input, direction)";
			reg(VectorCurvesNode, "Vector Curves", "Vector", kVector, 240.0f, [](Node& n) {
				n.Outputs = { Out("Vector", PinType::Vec3) };
				n.Inputs = { F("Fac", 1.0f, 0.0f, 1.0f), V("Vector") };
				n.Properties["Curve X"] = "-1,-1;1,1";
				n.Properties["Curve Y"] = "-1,-1;1,1";
				n.Properties["Curve Z"] = "-1,-1;1,1";
				n.Properties["Channel"] = "X";
			});
			auto& vrot = reg(VectorRotateNode, "Vector Rotate", "Vector", kVector, 170.0f, [](Node& n) {
				n.Outputs = { Out("Vector", PinType::Vec3) };
				n.Inputs = { V("Vector"), V("Center"), V("Axis", glm::vec3(0.0f, 0.0f, 1.0f)), F("Angle", 0.0f), V("Rotation") };
				n.Properties["Type"] = "Axis Angle";
				n.Properties["Invert"] = "0";
			});
			vrot.Enums = { { "Type", { "Axis Angle", "X Axis", "Y Axis", "Z Axis", "Euler" } } };
			vrot.Toggles = { "Invert" };
			vrot.PinVisible = [](const Node& n, const Pin& p) {
				const std::string t = Prop(n, "Type", "Axis Angle");
				if (p.Name == "Axis") return t == "Axis Angle";
				if (p.Name == "Angle") return t != "Euler";
				if (p.Name == "Rotation") return t == "Euler";
				return true;
			};
			auto& vxf = reg(VectorTransformNode, "Vector Transform", "Vector", kVector, 170.0f, [](Node& n) {
				n.Outputs = { Out("Vector", PinType::Vec3) };
				n.Inputs = { V("Vector", glm::vec3(0.5f)) };
				n.Properties["Type"] = "Vector";
				n.Properties["Convert From"] = "World";
				n.Properties["Convert To"] = "Object";
			});
			vxf.Enums = { { "Type", { "Point", "Vector", "Normal" } }, { "Convert From", { "World", "Object", "Camera" } }, { "Convert To", { "World", "Object", "Camera" } } };

			// ---- Converter (Blender 4.x additions) ----
			reg(BlackbodyNode, "Blackbody", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { F("Temperature", 1500.0f, 800.0f, 12000.0f) };
			});
			reg(FloatCurveNode, "Float Curve", "Converter", kConverter, 240.0f, [](Node& n) {
				n.Outputs = { Out("Value", PinType::Float) };
				n.Inputs = { F("Factor", 1.0f, 0.0f, 1.0f), F("Value", 1.0f) };
				n.Properties["Curve V"] = "0,0;1,1";
				n.Properties["Channel"] = "V";
			});
			reg(ShaderToRGB, "Shader to RGB", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Alpha", PinType::Float) };
				n.Inputs = { Implicit("Shader", PinType::Shader) };
			}).Description = "EEVEE: evaluates the lighting of a shader and returns it as a color (toon / NPR). The material renders in the forward pass";
			reg(WavelengthNode, "Wavelength", "Converter", kConverter, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Inputs = { F("Wavelength", 500.0f, 380.0f, 780.0f) };
			});

			const_cast<NodeTypeInfo*>(r.Get(BrickTexture))->Numbers = { { "Offset", 0.0f, 1.0f, false }, { "Offset Frequency", 1.0f, 99.0f, true },
				{ "Squash", 0.0f, 99.0f, false }, { "Squash Frequency", 1.0f, 99.0f, true } };
			const_cast<NodeTypeInfo*>(r.Get(MagicTexture))->Numbers = { { "Depth", 0.0f, 10.0f, true } };
			const_cast<NodeTypeInfo*>(r.Get(BevelNode))->Numbers = { { "Samples", 2.0f, 128.0f, true } };

			// ---------------- Legacy (v1 graphs) ----------------
			reg(OutputNode, "Material Output (v1)", "Legacy", kLegacy, 220.0f, [](Node& n) {
				n.Inputs = {
					C("Base Color", glm::vec4(1.0f)), F("Metallic", 0.0f, 0.0f, 1.0f), F("Roughness", 0.5f, 0.0f, 1.0f),
					F("Specular", 0.5f, 0.0f, 1.0f), In("Normal", PinType::Texture), F("Normal Strength", 1.0f),
					F("Ambient Occlusion", 1.0f, 0.0f, 1.0f), C("Emission", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)),
					F("Emission Strength", 0.0f), C("Subsurface Color", glm::vec4(1.0f, 0.35f, 0.25f, 1.0f)), F("Subsurface", 0.0f, 0.0f, 1.0f),
				};
			}).HiddenInMenu = true;
			const_cast<NodeTypeInfo*>(r.Get(OutputNode))->Unique = true;
			reg(TextureNode, "Texture Sample (v1)", "Legacy", kLegacy, 220.0f, [](Node& n) {
				n.Outputs = { Out("RGB", PinType::Texture), Out("R", PinType::Float), Out("G", PinType::Float), Out("B", PinType::Float), Out("A", PinType::Float) };
				n.Properties["Path"] = "";
			}).HiddenInMenu = true;
			reg(ColorNode, "Color (v1)", "Legacy", kLegacy, 150.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color) };
				n.Value = glm::vec4(1.0f);
			}).HiddenInMenu = true;
			reg(FloatNode, "Value (v1)", "Legacy", kLegacy, 150.0f, [](Node& n) {
				n.Outputs = { Out("Value", PinType::Float) };
				n.Value = glm::vec4(0.5f);
			}).HiddenInMenu = true;
			reg(MultiplyNode, "Multiply (v1)", "Legacy", kLegacy, 150.0f, [](Node& n) {
				n.Inputs = { C("A", glm::vec4(1.0f)), C("B", glm::vec4(1.0f)) };
				n.Outputs = { Out("Result", PinType::Color) };
			}).HiddenInMenu = true;
			return r;
		}();
		return registry;
	}

	// ============================================================================
	// COLOR RAMP
	// ============================================================================

	std::vector<CurvePoint> ParseCurve(const std::string& text) {
		std::vector<CurvePoint> points;
		std::stringstream ss(text);
		std::string item;
		while (std::getline(ss, item, ';')) {
			const size_t comma = item.find(',');
			if (comma == std::string::npos) continue;
			CurvePoint pt;
			pt.X = static_cast<float>(std::atof(item.substr(0, comma).c_str()));
			pt.Y = static_cast<float>(std::atof(item.substr(comma + 1).c_str()));
			points.push_back(pt);
		}
		std::sort(points.begin(), points.end(), [](const CurvePoint& a, const CurvePoint& b) { return a.X < b.X; });
		if (points.empty()) points = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
		if (points.size() == 1) points.push_back({ points[0].X + 1.0f, points[0].Y });
		return points;
	}

	std::string WriteCurve(const std::vector<CurvePoint>& points) {
		std::ostringstream ss;
		for (size_t i = 0; i < points.size(); i++) {
			if (i) ss << ';';
			ss << points[i].X << ',' << points[i].Y;
		}
		return ss.str();
	}

	std::vector<float> CurveTangents(const std::vector<CurvePoint>& p) {
		const size_t n = p.size();
		std::vector<float> m(n, 0.0f);
		if (n < 2) return m;
		std::vector<float> d(n - 1);
		for (size_t i = 0; i + 1 < n; i++) d[i] = (p[i + 1].Y - p[i].Y) / std::max(p[i + 1].X - p[i].X, 1e-6f);
		m[0] = d[0];
		m[n - 1] = d[n - 2];
		for (size_t i = 1; i + 1 < n; i++) m[i] = (d[i - 1] * d[i] <= 0.0f) ? 0.0f : (d[i - 1] + d[i]) * 0.5f;
		// Fritsch-Carlson: no overshoot
		for (size_t i = 0; i + 1 < n; i++) {
			if (std::abs(d[i]) < 1e-7f) { m[i] = m[i + 1] = 0.0f; continue; }
			const float a = m[i] / d[i], b = m[i + 1] / d[i], h = a * a + b * b;
			if (h > 9.0f) { const float t = 3.0f / std::sqrt(h); m[i] = t * a * d[i]; m[i + 1] = t * b * d[i]; }
		}
		return m;
	}

	float EvaluateCurve(const std::vector<CurvePoint>& p, float x) {
		if (p.empty()) return x;
		if (x <= p.front().X) return p.front().Y;
		if (x >= p.back().X) return p.back().Y;
		const std::vector<float> m = CurveTangents(p);
		for (size_t i = 0; i + 1 < p.size(); i++) {
			if (x > p[i + 1].X) continue;
			const float h = std::max(p[i + 1].X - p[i].X, 1e-6f), t = (x - p[i].X) / h;
			const float t2 = t * t, t3 = t2 * t;
			return (2 * t3 - 3 * t2 + 1) * p[i].Y + (t3 - 2 * t2 + t) * h * m[i] + (-2 * t3 + 3 * t2) * p[i + 1].Y + (t3 - t2) * h * m[i + 1];
		}
		return p.back().Y;
	}

	std::vector<RampStop> ParseRamp(const std::string& text) {
		std::vector<RampStop> stops;
		std::stringstream ss(text);
		std::string item;
		while (std::getline(ss, item, ';')) {
			RampStop s;
			if (sscanf_s(item.c_str(), "%f:%f,%f,%f,%f", &s.Position, &s.Color.r, &s.Color.g, &s.Color.b, &s.Color.a) == 5)
				stops.push_back(s);
		}
		if (stops.empty()) stops = { { 0.0f, glm::vec4(0, 0, 0, 1) }, { 1.0f, glm::vec4(1.0f) } };
		std::sort(stops.begin(), stops.end(), [](const RampStop& a, const RampStop& b) { return a.Position < b.Position; });
		return stops;
	}

	std::string WriteRamp(const std::vector<RampStop>& stops) {
		std::string out;
		char buf[96];
		for (const auto& s : stops) {
			snprintf(buf, sizeof(buf), "%s%.4f:%.4f,%.4f,%.4f,%.4f", out.empty() ? "" : ";", s.Position, s.Color.r, s.Color.g, s.Color.b, s.Color.a);
			out += buf;
		}
		return out;
	}

	// ============================================================================
	// MATERIAL -> GRAPH (migration from the slider workflow)
	// ============================================================================

	std::shared_ptr<NodeGraph> CreateFromMaterial(const MaterialAsset& m) {
		auto graph = std::make_shared<NodeGraph>(&GetRegistry());
		const ID outID = graph->AddNode(MaterialOutput, { 760.0f, 0.0f })->NodeID;
		const ID bsdfID = graph->AddNode(PrincipledBSDF, { 440.0f, 0.0f })->NodeID;
		auto bsdfIn = [&](const char* name) { return graph->FindNode(bsdfID)->FindInput(name); };
		graph->Connect(graph->FindNode(bsdfID)->FindOutput("BSDF")->PinID, graph->FindNode(outID)->FindInput("Surface")->PinID);

		bsdfIn("Base Color")->DefaultValue = m.GetAlbedo();
		bsdfIn("Metallic")->DefaultValue = glm::vec4(m.GetMetallic());
		bsdfIn("Roughness")->DefaultValue = glm::vec4(m.GetRoughness());
		bsdfIn("Specular")->DefaultValue = glm::vec4(m.GetSpecular());
		bsdfIn("Emission Color")->DefaultValue = glm::vec4(m.GetEmissionColor(), 1.0f);
		bsdfIn("Emission Strength")->DefaultValue = glm::vec4(m.GetEmissionIntensity());
		bsdfIn("Subsurface Color")->DefaultValue = glm::vec4(m.GetSubsurfaceColor(), 1.0f);
		bsdfIn("Subsurface Weight")->DefaultValue = glm::vec4(m.GetSubsurfaceAmount());

		float y = 0.0f;
		auto addImage = [&](const std::string& path, bool color = false) -> ID {
			Node* t = graph->AddNode(ImageTexture, { -20.0f, y });
			t->Properties["Path"] = path;
			t->Properties["Color Space"] = color ? "sRGB" : "Non-Color";
			y += 230.0f;
			return t->NodeID;
		};
		auto link = [&](ID from, const char* outName, ID to, const char* inName) {
			graph->Connect(graph->FindNode(from)->FindOutput(outName)->PinID, graph->FindNode(to)->FindInput(inName)->PinID);
		};
		// Scalar = texture channel * multiplier (Math node only when the multiplier matters)
		auto linkScalar = [&](ID tex, const char* channel, const char* inName, float multiplier) {
			ID src = tex; const char* srcOut = channel;
			if (std::string(channel) != "Color" && std::string(channel) != "Alpha") {
				Node* sepNode = graph->AddNode(SeparateColor, { 200.0f, graph->FindNode(tex)->Position.y });
				const ID sep = sepNode->NodeID;
				link(tex, "Color", sep, "Color");
				src = sep;
			}
			if (std::abs(multiplier - 1.0f) > 1e-4f) {
				Node* mul = graph->AddNode(MathNode, { 300.0f, graph->FindNode(tex)->Position.y + 110.0f });
				mul->Properties["Operation"] = "Multiply";
				mul->FindInput("B")->DefaultValue = glm::vec4(multiplier);
				const ID mulID = mul->NodeID;
				link(src, srcOut, mulID, "A");
				link(mulID, "Value", bsdfID, inName);
			}
			else {
				link(src, srcOut, bsdfID, inName);
			}
		};
		static const char* kChannel[] = { "Red", "Green", "Blue", "Alpha", "Red" };

		if (m.HasAlbedoMap()) {
			const ID tex = addImage(m.GetAlbedoPath(), true);
			if (glm::vec3(m.GetAlbedo()) != glm::vec3(1.0f)) {
				Node* mixNode = graph->AddNode(MixNode, { 220.0f, graph->FindNode(tex)->Position.y });
				mixNode->Properties["Blending"] = "Multiply";
				mixNode->FindInput("Factor")->DefaultValue = glm::vec4(1.0f);
				mixNode->FindInput("B")->DefaultValue = m.GetAlbedo();
				const ID mixID = mixNode->NodeID;
				link(tex, "Color", mixID, "A");
				link(mixID, "Result", bsdfID, "Base Color");
			}
			else {
				link(tex, "Color", bsdfID, "Base Color");
			}
		}
		if (m.HasNormalMap()) {
			const ID tex = addImage(m.GetNormalPath());
			Node* nm = graph->AddNode(NormalMapNode, { 220.0f, graph->FindNode(tex)->Position.y });
			nm->FindInput("Strength")->DefaultValue = glm::vec4(m.GetNormalIntensity());
			const ID nmID = nm->NodeID;
			link(tex, "Color", nmID, "Color");
			link(nmID, "Normal", bsdfID, "Normal");
		}
		if (m.HasLayeredTexture()) {
			const auto& cfg = m.GetLayeredTextureConfig();
			const ID tex = addImage(cfg.Path);   // packed channels: Non-Color
			if (cfg.UseForMetallic) linkScalar(tex, kChannel[(int)cfg.MetallicChannel], "Metallic", m.GetMetallicMultiplier());
			if (cfg.UseForRoughness) linkScalar(tex, kChannel[(int)cfg.RoughnessChannel], "Roughness", m.GetRoughnessMultiplier());
			if (cfg.UseForAO) linkScalar(tex, kChannel[(int)cfg.AOChannel], "Ambient Occlusion", m.GetAOMultiplier());
		}
		if (m.HasMetallicMap()) linkScalar(addImage(m.GetMetallicPath()), "Red", "Metallic", m.GetMetallicMultiplier());
		if (m.HasRoughnessMap()) linkScalar(addImage(m.GetRoughnessPath()), "Red", "Roughness", m.GetRoughnessMultiplier());
		if (m.HasSpecularMap()) linkScalar(addImage(m.GetSpecularPath()), "Red", "Specular", m.GetSpecularMultiplier());
		if (m.HasAOMap()) linkScalar(addImage(m.GetAOPath()), "Red", "Ambient Occlusion", m.GetAOMultiplier());
		if (m.HasEmissionMap()) link(addImage(m.GetEmissionPath(), true), "Color", bsdfID, "Emission Color");

		graph->FindNode(bsdfID)->Position.y = std::max(0.0f, (y - 230.0f) * 0.5f - 150.0f);
		graph->FindNode(outID)->Position.y = graph->FindNode(bsdfID)->Position.y;
		return graph;
	}

	// ============================================================================
	// GRAPH -> MATERIAL (bake fallback)
	// ============================================================================

	namespace {
		struct Source {
			bool IsTexture = false;
			std::string Path;
			int Channel = -1;              // -1 = RGB, 0..3 = R/G/B/A
			glm::vec4 Value{ 0.0f };       // constant, or factor applied to the texture
			bool ViaNormalMap = false;
		};

		class Resolver {
		public:
			Resolver(const NodeGraph& g, std::vector<std::string>& warnings) : m_Graph(g), m_Warnings(warnings) {}

			Source ResolveInput(const Node& node, const char* name, int depth = 0) {
				const Pin* in = node.FindInput(name);
				if (!in) return {};
				auto [src, pin] = m_Graph.GetSource(in->PinID);
				if (!src) { Source s; s.Value = in->DefaultValue; return s; }
				return ResolveOutput(*src, *pin, depth + 1);
			}

		private:
			Source ResolveOutput(const Node& node, const Pin& pin, int depth) {
				Source s;
				if (depth > 24) { m_Warnings.push_back("Graph is too deep (cycle?)"); return s; }
				const std::string& t = node.TypeName;

				if (t == ColorNode || t == RGBNode) { s.Value = node.Value; return s; }
				if (t == FloatNode || t == ValueNode) { s.Value = glm::vec4(node.Value.x); return s; }
				if (t == TextureNode || t == ImageTexture) {
					s.Path = Prop(node, "Path");
					if (s.Path.empty()) { s.Value = glm::vec4(1.0f); return s; }
					s.IsTexture = true;
					s.Value = glm::vec4(1.0f);
					const std::string& n = pin.Name;
					s.Channel = (n == "R") ? 0 : (n == "G") ? 1 : (n == "B") ? 2 : (n == "A" || n == "Alpha") ? 3 : -1;
					return s;
				}
				if (t == SeparateColor) {
					Source c = ResolveInput(node, "Color", depth);
					const std::string& n = pin.Name;
					int ch = n == "Red" ? 0 : n == "Green" ? 1 : n == "Blue" ? 2 : 3;
					if (c.IsTexture) { c.Channel = ch; return c; }
					s.Value = glm::vec4(c.Value[ch]); return s;
				}
				if (t == NormalMapNode) {
					Source c = ResolveInput(node, "Color", depth);
					c.ViaNormalMap = true;
					c.Value = ResolveInput(node, "Strength", depth).Value;
					return c;
				}
				if (t == MultiplyNode || (t == MathNode && Prop(node, "Operation") == "Multiply") ||
					(t == MixNode && Prop(node, "Blending") == "Multiply")) {
					const bool isMix = t == MixNode;
					Source a = ResolveInput(node, isMix ? "A" : "A", depth);
					Source b = ResolveInput(node, isMix ? "B" : "B", depth);
					if (a.IsTexture && b.IsTexture) return a;
					if (!a.IsTexture && !b.IsTexture) { s.Value = a.Value * b.Value; return s; }
					Source tex = a.IsTexture ? a : b;
					tex.Value *= (a.IsTexture ? b : a).Value;
					return tex;
				}
				m_Warnings.push_back("Rendered by the graph shader: " + node.Title);
				s.Value = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
				return s;
			}

			const NodeGraph& m_Graph;
			std::vector<std::string>& m_Warnings;
		};

		Ref<Texture2D> LoadTexture(const std::string& path, const Ref<Texture2D>& current, const std::string& currentPath) {
			if (path.empty()) return nullptr;
			if (current && currentPath == path) return current;
			static auto& cache = *new std::unordered_map<std::string, Ref<Texture2D>>();   // leaked on purpose (GL objects outlive the context otherwise)
			auto& slot = cache[path];
			if (!slot || !slot->IsLoaded()) slot = Texture2D::Create(path);
			return (slot && slot->IsLoaded()) ? slot : nullptr;
		}
	}

	std::vector<std::string> Apply(const NodeGraph& graph, MaterialAsset& m) {
		std::vector<std::string> warnings;
		Resolver resolver(graph, warnings);

		// The node whose inputs describe the surface: legacy output, or the BSDF behind Material Output
		const Node* surface = graph.FindNodeByType(OutputNode);
		bool legacy = surface != nullptr;
		if (!surface) {
			const Node* out = graph.FindNodeByType(MaterialOutput);
			if (!out) { warnings.push_back("The graph has no Material Output node"); return warnings; }
			auto [src, pin] = graph.GetSource(out->FindInput("Surface")->PinID);
			if (!src) return warnings;
			if (src->TypeName != PrincipledBSDF) {
				// Emission / mixed shaders: rendered by the graph shader only
				m.SetAlbedoMap(nullptr);
				return warnings;
			}
			surface = src;
		}
		auto get = [&](const char* legacyName, const char* name) { return resolver.ResolveInput(*surface, legacy ? legacyName : name); };

		// ---- Base color ----
		Source base = get("Base Color", "Base Color");
		if (base.IsTexture) {
			m.SetAlbedoMap(LoadTexture(base.Path, m.GetAlbedoMap(), m.GetAlbedoPath()));
			m.SetAlbedo(glm::vec4(glm::vec3(base.Value), 1.0f));
		}
		else {
			m.SetAlbedoMap(nullptr);
			m.SetAlbedo(glm::vec4(glm::vec3(base.Value), base.Value.a > 0.0f ? base.Value.a : 1.0f));
		}

		// ---- Normal ----
		Source normal = get("Normal", "Normal");
		m.SetNormalMap(normal.IsTexture ? LoadTexture(normal.Path, m.GetNormalMap(), m.GetNormalPath()) : nullptr);
		if (legacy) m.SetNormalIntensity(resolver.ResolveInput(*surface, "Normal Strength").Value.x);
		else if (normal.ViaNormalMap) m.SetNormalIntensity(normal.Value.x);

		// ---- Scalar channels (metallic / roughness / AO may share a packed ORM texture) ----
		Source metal = get("Metallic", "Metallic"), rough = get("Roughness", "Roughness");
		Source ao = get("Ambient Occlusion", "Ambient Occlusion"), spec = get("Specular", "Specular");

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
		else if (metal.IsTexture) { m.SetMetallicMap(LoadTexture(metal.Path, m.GetMetallicMap(), m.GetMetallicPath())); m.SetMetallicMultiplier(metal.Value.x); }
		else { m.SetMetallicMap(nullptr); m.SetMetallic(metal.Value.x); }

		if (usesPacked(rough)) { cfg.UseForRoughness = true; cfg.RoughnessChannel = toChannel(rough.Channel); m.SetRoughnessMap(nullptr); m.SetRoughnessMultiplier(rough.Value.x); }
		else if (rough.IsTexture) { m.SetRoughnessMap(LoadTexture(rough.Path, m.GetRoughnessMap(), m.GetRoughnessPath())); m.SetRoughnessMultiplier(rough.Value.x); }
		else { m.SetRoughnessMap(nullptr); m.SetRoughness(rough.Value.x); }

		if (usesPacked(ao)) { cfg.UseForAO = true; cfg.AOChannel = toChannel(ao.Channel); m.SetAOMap(nullptr); m.SetAOMultiplier(ao.Value.x); }
		else if (ao.IsTexture) { m.SetAOMap(LoadTexture(ao.Path, m.GetAOMap(), m.GetAOPath())); m.SetAOMultiplier(ao.Value.x); }
		else { m.SetAOMap(nullptr); }

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
		Source emission = get("Emission", "Emission Color");
		m.SetEmissionMap(emission.IsTexture ? LoadTexture(emission.Path, m.GetEmissionMap(), m.GetEmissionPath()) : nullptr);
		m.SetEmissionColor(glm::vec3(emission.Value));
		m.SetEmissionIntensity(get("Emission Strength", "Emission Strength").Value.x);

		// ---- Subsurface ----
		m.SetSubsurfaceColor(glm::vec3(get("Subsurface Color", "Subsurface Color").Value));
		m.SetSubsurfaceAmount(get("Subsurface", "Subsurface Weight").Value.x);

		return warnings;
	}

} // namespace Lunex::MaterialGraph
