#include "stpch.h"
#include "MaterialGraph.h"

#include "MaterialAsset.h"
#include "Renderer/Texture.h"

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
			reg(MaterialOutput, "Material Output", "Output", kOutput, 150.0f, [](Node& n) {
				n.Inputs = { Implicit("Surface", PinType::Shader) };
			}).Unique = true;

			// ---------------- Shader ----------------
			reg(PrincipledBSDF, "Principled BSDF", "Shader", kShader, 240.0f, [](Node& n) {
				n.Outputs = { Out("BSDF", PinType::Shader) };
				n.Inputs = {
					C("Base Color", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)),
					F("Metallic", 0.0f, 0.0f, 1.0f),
					F("Roughness", 0.5f, 0.0f, 1.0f),
					F("Specular", 0.5f, 0.0f, 1.0f),
					F("Alpha", 1.0f, 0.0f, 1.0f),
					Implicit("Normal", PinType::Vec3),
					F("Ambient Occlusion", 1.0f, 0.0f, 1.0f),
					F("Subsurface Weight", 0.0f, 0.0f, 1.0f),
					C("Subsurface Color", glm::vec4(1.0f, 0.35f, 0.25f, 1.0f)),
					C("Emission Color", glm::vec4(1.0f)),
					F("Emission Strength", 0.0f, 0.0f, 0.0f),
				};
			}).Description = "Physically based surface (metal/roughness workflow)";
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

			// ---------------- Input ----------------
			reg(TexCoordNode, "Texture Coordinate", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("UV", PinType::Vec3), Out("Generated", PinType::Vec3), Out("Normal", PinType::Vec3), Out("Reflection", PinType::Vec3) };
			});
			reg(GeometryNode, "Geometry", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Position", PinType::Vec3), Out("Normal", PinType::Vec3), Out("Tangent", PinType::Vec3), Out("Incoming", PinType::Vec3) };
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
				n.Inputs = { F("IOR", 1.45f, 1.0f, 3.0f), Implicit("Normal", PinType::Vec3) };
			});
			reg(LayerWeight, "Layer Weight", "Input", kInput, 150.0f, [](Node& n) {
				n.Outputs = { Out("Fresnel", PinType::Float), Out("Facing", PinType::Float) };
				n.Inputs = { F("Blend", 0.5f, 0.0f, 1.0f), Implicit("Normal", PinType::Vec3) };
			});
			reg(TimeNode, "Time", "Input", kInput, 140.0f, [](Node& n) {
				n.Outputs = { Out("Seconds", PinType::Float) };
			}).Description = "Elapsed time in seconds (animated materials)";

			// ---------------- Texture ----------------
			auto& img = reg(ImageTexture, "Image Texture", "Texture", kTexture, 240.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Alpha", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3) };
				n.Properties["Path"] = "";
				n.Properties["Interpolation"] = "Linear";
			});
			img.Enums = { { "Interpolation", { "Linear", "Closest" } } };
			auto& noise = reg(NoiseTexture, "Noise Texture", "Texture", kTexture, 170.0f, [](Node& n) {
				n.Outputs = { Out("Fac", PinType::Float), Out("Color", PinType::Color) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Scale", 5.0f), F("Detail", 2.0f, 0.0f, 15.0f), F("Roughness", 0.5f, 0.0f, 1.0f), F("Distortion", 0.0f) };
				n.Properties["Dimensions"] = "3D";
			});
			noise.Enums = { { "Dimensions", { "3D", "2D" } } };
			auto& voronoi = reg(VoronoiTexture, "Voronoi Texture", "Texture", kTexture, 170.0f, [](Node& n) {
				n.Outputs = { Out("Distance", PinType::Float), Out("Color", PinType::Color), Out("Position", PinType::Vec3) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Scale", 5.0f), F("Randomness", 1.0f, 0.0f, 1.0f) };
				n.Properties["Feature"] = "F1";
			});
			voronoi.Enums = { { "Feature", { "F1", "F2", "Distance to Edge" } } };
			auto& wave = reg(WaveTexture, "Wave Texture", "Texture", kTexture, 170.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), F("Scale", 5.0f), F("Distortion", 0.0f), F("Detail", 2.0f, 0.0f, 15.0f), F("Phase Offset", 0.0f) };
				n.Properties["Type"] = "Bands";
				n.Properties["Profile"] = "Sine";
			});
			wave.Enums = { { "Type", { "Bands", "Rings" } }, { "Profile", { "Sine", "Saw", "Triangle" } } };
			reg(CheckerTexture, "Checker Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3), C("Color1", glm::vec4(0.8f, 0.8f, 0.8f, 1.0f)), C("Color2", glm::vec4(0.2f, 0.2f, 0.2f, 1.0f)), F("Scale", 5.0f) };
			});
			auto& grad = reg(GradientTexture, "Gradient Texture", "Texture", kTexture, 160.0f, [](Node& n) {
				n.Outputs = { Out("Color", PinType::Color), Out("Fac", PinType::Float) };
				n.Inputs = { Implicit("Vector", PinType::Vec3) };
				n.Properties["Type"] = "Linear";
			});
			grad.Enums = { { "Type", { "Linear", "Quadratic", "Easing", "Diagonal", "Spherical", "Radial" } } };

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
		auto addImage = [&](const std::string& path) -> ID {
			Node* t = graph->AddNode(ImageTexture, { -20.0f, y });
			t->Properties["Path"] = path;
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
			const ID tex = addImage(m.GetAlbedoPath());
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
			const ID tex = addImage(cfg.Path);
			if (cfg.UseForMetallic) linkScalar(tex, kChannel[(int)cfg.MetallicChannel], "Metallic", m.GetMetallicMultiplier());
			if (cfg.UseForRoughness) linkScalar(tex, kChannel[(int)cfg.RoughnessChannel], "Roughness", m.GetRoughnessMultiplier());
			if (cfg.UseForAO) linkScalar(tex, kChannel[(int)cfg.AOChannel], "Ambient Occlusion", m.GetAOMultiplier());
		}
		if (m.HasMetallicMap()) linkScalar(addImage(m.GetMetallicPath()), "Red", "Metallic", m.GetMetallicMultiplier());
		if (m.HasRoughnessMap()) linkScalar(addImage(m.GetRoughnessPath()), "Red", "Roughness", m.GetRoughnessMultiplier());
		if (m.HasSpecularMap()) linkScalar(addImage(m.GetSpecularPath()), "Red", "Specular", m.GetSpecularMultiplier());
		if (m.HasAOMap()) linkScalar(addImage(m.GetAOPath()), "Red", "Ambient Occlusion", m.GetAOMultiplier());
		if (m.HasEmissionMap()) link(addImage(m.GetEmissionPath()), "Color", bsdfID, "Emission Color");

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
			static std::unordered_map<std::string, Ref<Texture2D>> cache;
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
