#include "stpch.h"
#include "Blueprint.h"

#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Physics/Components/RigidBodyComponent.h"
#include "Core/Input.h"
#include "Log/Log.h"

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <functional>
#include <glm/gtc/constants.hpp>

namespace Lunex::Blueprint {

	using namespace Graph;

	// ============================================================================
	// EXECUTION CONTEXT
	// ============================================================================

	namespace {
		struct Exec;
		using RunFn = std::function<void(Exec&, const Node&, const std::string& entered)>;

		std::unordered_map<std::string, RunFn>& Impls() {
			static std::unordered_map<std::string, RunFn> impls;
			return impls;
		}

		bool IsPure(const Node& n) {
			for (const auto& p : n.Inputs) if (p.Type == PinType::Flow) return false;
			for (const auto& p : n.Outputs) if (p.Type == PinType::Flow) return false;
			return true;
		}

		struct Exec {
			Scene* SceneRef = nullptr;
			Entity Self;
			const NodeGraph& G;
			Instance& Inst;
			float DeltaTime = 0.0f;
			int Budget = 20000;                       // runaway loop guard
			std::unordered_map<ID, Value> Out;        // output pin -> value

			Exec(Scene* scene, Entity self, const NodeGraph& g, Instance& inst, float dt)
				: SceneRef(scene), Self(self), G(g), Inst(inst), DeltaTime(dt) {}

			Value In(const Node& n, const char* name) {
				const Pin* pin = n.FindInput(name);
				if (!pin) return {};
				if (const Link* link = G.GetLinkInto(pin->PinID)) {
					const Node* src = G.FindPinOwner(link->FromPin);
					if (src && IsPure(*src)) {
						auto it = Impls().find(src->TypeName);
						if (it != Impls().end() && --Budget > 0) it->second(*this, *src, "");
					}
					auto v = Out.find(link->FromPin);
					return v != Out.end() ? v->second : Value{};
				}
				Value v;
				v.V = pin->DefaultValue;
				v.S = pin->StringValue;
				return v;
			}
			float F(const Node& n, const char* name) { return In(n, name).V.x; }
			int I(const Node& n, const char* name) { return static_cast<int>(std::round(In(n, name).V.x)); }
			bool B(const Node& n, const char* name) { return In(n, name).V.x != 0.0f; }
			glm::vec3 V3(const Node& n, const char* name) { return glm::vec3(In(n, name).V); }
			glm::vec4 C(const Node& n, const char* name) { return In(n, name).V; }
			std::string S(const Node& n, const char* name) {
				Value v = In(n, name);
				if (!v.S.empty()) return v.S;
				char buf[64]; snprintf(buf, sizeof(buf), "%g", v.V.x);
				return buf;
			}
			Entity E(const Node& n, const char* name = "Target") {
				const Value v = In(n, name);
				if (v.Entity == 0) return Self;
				return SceneRef->GetEntityByUUID(UUID(v.Entity));
			}

			void Set(const Node& n, const char* outName, const Value& v) {
				if (const Pin* p = n.FindOutput(outName)) Out[p->PinID] = v;
			}
			void SetF(const Node& n, const char* outName, float f) { Value v; v.V = glm::vec4(f); Set(n, outName, v); }
			void SetV(const Node& n, const char* outName, const glm::vec3& f) { Value v; v.V = glm::vec4(f, 0.0f); Set(n, outName, v); }
			void SetB(const Node& n, const char* outName, bool b) { SetF(n, outName, b ? 1.0f : 0.0f); }

			// Follow an execution output
			void Then(const Node& n, const char* execOut = "Then") {
				const Pin* out = n.FindOutput(execOut);
				if (!out) return;
				for (const auto& link : G.GetLinks()) {
					if (link.FromPin != out->PinID) continue;
					const Node* target = G.FindPinOwner(link.ToPin);
					const Pin* entered = G.FindPin(link.ToPin);
					if (target && entered) Run(*target, entered->Name);
				}
			}

			void Run(const Node& n, const std::string& entered) {
				if (--Budget <= 0) return;
				auto it = Impls().find(n.TypeName);
				if (it != Impls().end()) it->second(*this, n, entered);
			}
		};

		std::string Prop(const Node& n, const char* key, const char* fallback = "") {
			auto it = n.Properties.find(key);
			return it != n.Properties.end() ? it->second : fallback;
		}

		MaterialInstance* UniqueMaterial(Entity e) {
			if (!e || !e.HasComponent<MaterialComponent>()) return nullptr;
			auto& mc = e.GetComponent<MaterialComponent>();
			if (!mc.Instance) return nullptr;
			if (mc.Instance.use_count() > 1) mc.Instance = mc.Instance->Clone();
			return mc.Instance.get();
		}

		RigidBodyComponent* Body(Entity e) {
			if (!e || !e.HasComponent<Rigidbody3DComponent>()) return nullptr;
			return static_cast<RigidBodyComponent*>(e.GetComponent<Rigidbody3DComponent>().RuntimeBody);
		}

		// Keys offered in the drop-downs (Lunex key codes = GLFW)
		const std::vector<std::pair<std::string, int>>& Keys() {
			static const std::vector<std::pair<std::string, int>> keys = [] {
				std::vector<std::pair<std::string, int>> k = {
					{ "Space", 32 }, { "Enter", 257 }, { "Escape", 256 }, { "Tab", 258 }, { "Left Shift", 340 }, { "Left Ctrl", 341 },
					{ "Up", 265 }, { "Down", 264 }, { "Left", 263 }, { "Right", 262 },
				};
				for (char c = 'A'; c <= 'Z'; c++) k.push_back({ std::string(1, c), static_cast<int>(c) });
				for (char c = '0'; c <= '9'; c++) k.push_back({ std::string(1, c), static_cast<int>(c) });
				return k;
			}();
			return keys;
		}
		std::vector<std::string> KeyNames() { std::vector<std::string> n; for (auto& k : Keys()) n.push_back(k.first); return n; }
		int KeyCodeOf(const Node& n) {
			const std::string name = Prop(n, "Key", "Space");
			for (auto& k : Keys()) if (k.first == name) return k.second;
			return 32;
		}
		bool KeyDown(int code) { return Input::IsKeyPressed(static_cast<KeyCode>(code)); }

		// ---- Pin builders ----
		Pin P(const char* name, PinType t, const glm::vec4& def = glm::vec4(0.0f)) { Pin p; p.Name = name; p.Type = t; p.DefaultValue = def; return p; }
		Pin FlowIn(const char* name = "In") { Pin p = P(name, PinType::Flow); p.Label = name == std::string("In") ? " " : name; return p; }
		Pin FlowOut(const char* name = "Then") { Pin p = P(name, PinType::Flow); p.Label = name == std::string("Then") ? " " : name; return p; }
		Pin Target() { Pin p = P("Target", PinType::Entity); p.Label = "Target (self)"; return p; }
		Pin Str(const char* name, const char* def = "") { Pin p = P(name, PinType::String); p.StringValue = def; return p; }
	}

	// ============================================================================
	// REGISTRY
	// ============================================================================

	const NodeRegistry& GetRegistry() {
		static NodeRegistry registry = [] {
			NodeRegistry r;
			auto def = [&](const char* type, const char* title, const char* category, float width,
				std::function<void(Node&)> build, RunFn run) -> NodeTypeInfo& {
				NodeTypeInfo info;
				info.TypeName = type; info.Title = title; info.Category = category; info.Width = width;
				info.Build = std::move(build);
				r.Register(info);
				Impls()[type] = std::move(run);
				return const_cast<NodeTypeInfo&>(*r.Get(type));
			};
			const std::vector<std::string> keyNames = KeyNames();

			// ---------------- Events ----------------
			def("BP.OnStart", "On Start", "Events", 150.0f,
				[](Node& n) { n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) { x.Then(n); }).Description = "Runs once when Play starts";
			def("BP.OnUpdate", "On Update", "Events", 160.0f,
				[](Node& n) { n.Outputs = { FlowOut(), P("Delta Time", PinType::Float) }; },
				[](Exec& x, const Node& n, const std::string&) { x.SetF(n, "Delta Time", x.DeltaTime); x.Then(n); }).Description = "Runs every frame";
			def("BP.OnKeyPressed", "On Key Pressed", "Events", 160.0f,
				[](Node& n) { n.Outputs = { FlowOut() }; n.Properties["Key"] = "Space"; },
				[](Exec& x, const Node& n, const std::string&) { x.Then(n); }).Enums = { { "Key", keyNames } };
			def("BP.OnKeyReleased", "On Key Released", "Events", 160.0f,
				[](Node& n) { n.Outputs = { FlowOut() }; n.Properties["Key"] = "Space"; },
				[](Exec& x, const Node& n, const std::string&) { x.Then(n); }).Enums = { { "Key", keyNames } };
			def("BP.OnTimer", "On Timer", "Events", 160.0f,
				[](Node& n) { n.Outputs = { FlowOut() }; n.Inputs = { P("Interval", PinType::Float, glm::vec4(1.0f)) }; },
				[](Exec& x, const Node& n, const std::string&) { x.Then(n); }).Description = "Runs every Interval seconds";

			// ---------------- Flow ----------------
			def("BP.Branch", "Branch", "Flow", 150.0f,
				[](Node& n) { n.Inputs = { FlowIn(), P("Condition", PinType::Bool) }; n.Outputs = { FlowOut("True"), FlowOut("False") }; },
				[](Exec& x, const Node& n, const std::string&) { x.Then(n, x.B(n, "Condition") ? "True" : "False"); });
			def("BP.Sequence", "Sequence", "Flow", 140.0f,
				[](Node& n) { n.Inputs = { FlowIn() }; n.Outputs = { FlowOut("Then 0"), FlowOut("Then 1"), FlowOut("Then 2") }; },
				[](Exec& x, const Node& n, const std::string&) { x.Then(n, "Then 0"); x.Then(n, "Then 1"); x.Then(n, "Then 2"); });
			def("BP.ForLoop", "For Loop", "Flow", 160.0f,
				[](Node& n) {
					n.Inputs = { FlowIn(), P("First", PinType::Int), P("Last", PinType::Int, glm::vec4(9.0f)) };
					n.Outputs = { FlowOut("Loop Body"), P("Index", PinType::Int), FlowOut("Completed") };
				},
				[](Exec& x, const Node& n, const std::string&) {
					const int first = x.I(n, "First"), last = x.I(n, "Last");
					for (int i = first; i <= last && x.Budget > 0; i++) { x.SetF(n, "Index", static_cast<float>(i)); x.Then(n, "Loop Body"); }
					x.Then(n, "Completed");
				});
			def("BP.Delay", "Delay", "Flow", 150.0f,
				[](Node& n) { n.Inputs = { FlowIn(), P("Duration", PinType::Float, glm::vec4(1.0f)) }; n.Outputs = { FlowOut("Completed") }; },
				[](Exec& x, const Node& n, const std::string& entered) {
					if (entered == "__resume") { x.Then(n, "Completed"); return; }
					x.Inst.Delays.push_back({ n.NodeID, std::max(0.0f, x.F(n, "Duration")) });
				}).Description = "Continues after Duration seconds";
			def("BP.DoOnce", "Do Once", "Flow", 140.0f,
				[](Node& n) { n.Inputs = { FlowIn(), FlowIn("Reset") }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string& entered) {
					bool& done = x.Inst.Flags[n.NodeID];
					if (entered == "Reset") { done = false; return; }
					if (!done) { done = true; x.Then(n); }
				});

			// ---------------- Transform ----------------
			def("BP.Self", "Self", "Transform", 120.0f,
				[](Node& n) { n.Outputs = { P("Entity", PinType::Entity) }; },
				[](Exec& x, const Node& n, const std::string&) { Value v; v.Entity = static_cast<uint64_t>(x.Self.GetUUID()); x.Set(n, "Entity", v); });
			def("BP.FindEntity", "Find Entity", "Transform", 170.0f,
				[](Node& n) { n.Inputs = { Str("Name", "Player") }; n.Outputs = { P("Entity", PinType::Entity), P("Found", PinType::Bool) }; },
				[](Exec& x, const Node& n, const std::string&) {
					Value v;
					Entity e;
					const std::string name = x.S(n, "Name");
					auto view = x.SceneRef->GetAllEntitiesWith<TagComponent>();
					for (auto h : view) {
						if (view.get<TagComponent>(h).Tag == name) { e = Entity(h, x.SceneRef); break; }
					}
					if (e) v.Entity = static_cast<uint64_t>(e.GetUUID());
					x.Set(n, "Entity", v);
					x.SetB(n, "Found", static_cast<bool>(e));
				});
			auto transformGet = [&](const char* type, const char* title, const char* out, int which) {
				def(type, title, "Transform", 160.0f,
					[out](Node& n) { n.Inputs = { Target() }; n.Outputs = { P(out, PinType::Vec3) }; },
					[out, which](Exec& x, const Node& n, const std::string&) {
						Entity e = x.E(n);
						if (!e) return;
						auto& tc = e.GetComponent<TransformComponent>();
						x.SetV(n, out, which == 0 ? tc.Translation : which == 1 ? glm::degrees(tc.Rotation) : tc.Scale);
					});
			};
			transformGet("BP.GetPosition", "Get Position", "Position", 0);
			transformGet("BP.GetRotation", "Get Rotation", "Rotation", 1);
			transformGet("BP.GetScale", "Get Scale", "Scale", 2);
			auto transformSet = [&](const char* type, const char* title, const char* in, int which, bool relative) {
				def(type, title, "Transform", 170.0f,
					[in, which](Node& n) { n.Inputs = { FlowIn(), Target(), P(in, PinType::Vec3, which == 2 ? glm::vec4(1.0f) : glm::vec4(0.0f)) }; n.Outputs = { FlowOut() }; },
					[in, which, relative](Exec& x, const Node& n, const std::string&) {
						if (Entity e = x.E(n)) {
							auto& tc = e.GetComponent<TransformComponent>();
							glm::vec3 v = x.V3(n, in);
							glm::vec3& field = which == 0 ? tc.Translation : which == 1 ? tc.Rotation : tc.Scale;
							if (which == 1) v = glm::radians(v);
							field = relative ? field + v : v;
						}
						x.Then(n);
					});
			};
			transformSet("BP.SetPosition", "Set Position", "Position", 0, false);
			transformSet("BP.Translate", "Translate", "Delta", 0, true);
			transformSet("BP.SetRotation", "Set Rotation", "Rotation", 1, false);
			transformSet("BP.Rotate", "Rotate", "Delta", 1, true);
			transformSet("BP.SetScale", "Set Scale", "Scale", 2, false);

			// ---------------- Physics ----------------
			auto physicsAction = [&](const char* type, const char* title, const char* in, int which) {
				def(type, title, "Physics", 170.0f,
					[in](Node& n) { n.Inputs = { FlowIn(), Target(), P(in, PinType::Vec3) }; n.Outputs = { FlowOut() }; },
					[in, which](Exec& x, const Node& n, const std::string&) {
						if (RigidBodyComponent* body = Body(x.E(n))) {
							const glm::vec3 v = x.V3(n, in);
							body->Activate();
							if (which == 0) body->ApplyCentralForce(v);
							else if (which == 1) body->ApplyCentralImpulse(v);
							else body->SetLinearVelocity(v);
						}
						x.Then(n);
					});
			};
			physicsAction("BP.AddForce", "Add Force", "Force", 0);
			physicsAction("BP.AddImpulse", "Add Impulse", "Impulse", 1);
			physicsAction("BP.SetVelocity", "Set Velocity", "Velocity", 2);
			def("BP.GetVelocity", "Get Velocity", "Physics", 160.0f,
				[](Node& n) { n.Inputs = { Target() }; n.Outputs = { P("Velocity", PinType::Vec3) }; },
				[](Exec& x, const Node& n, const std::string&) {
					RigidBodyComponent* body = Body(x.E(n));
					x.SetV(n, "Velocity", body ? body->GetLinearVelocity() : glm::vec3(0.0f));
				});

			// ---------------- Material ----------------
			def("BP.SetMaterialColor", "Set Material Color", "Material", 190.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), P("Color", PinType::Color, glm::vec4(1.0f)) }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) { if (auto* m = UniqueMaterial(x.E(n))) m->SetAlbedo(x.C(n, "Color"), true); x.Then(n); });
			def("BP.SetMaterialFloat", "Set Material Value", "Material", 200.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), P("Value", PinType::Float, glm::vec4(0.5f)) }; n.Outputs = { FlowOut() }; n.Properties["Property"] = "Roughness"; },
				[](Exec& x, const Node& n, const std::string&) {
					if (auto* m = UniqueMaterial(x.E(n))) {
						const float v = x.F(n, "Value");
						const std::string p = Prop(n, "Property", "Roughness");
						if (p == "Roughness") m->SetRoughness(v, true);
						else if (p == "Metallic") m->SetMetallic(v, true);
						else if (p == "Specular") m->SetSpecular(v, true);
						else if (p == "Emission Intensity") m->SetEmissionIntensity(v, true);
						else if (p == "Normal Intensity") m->SetNormalIntensity(v, true);
					}
					x.Then(n);
				}).Enums = { { "Property", { "Roughness", "Metallic", "Specular", "Emission Intensity", "Normal Intensity" } } };
			def("BP.SetMaterialParameter", "Set Material Parameter", "Material", 210.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), Str("Name", "Wetness"), P("Value", PinType::Float, glm::vec4(1.0f)) }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) { if (auto* m = UniqueMaterial(x.E(n))) m->SetGraphParameter(x.S(n, "Name"), glm::vec4(x.F(n, "Value"))); x.Then(n); })
				.Description = "Changes a named Value node of a node-graph material on this entity";
			def("BP.SetMaterialParameterColor", "Set Material Parameter Color", "Material", 230.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), Str("Name", "Tint"), P("Color", PinType::Color, glm::vec4(1.0f)) }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) { if (auto* m = UniqueMaterial(x.E(n))) m->SetGraphParameter(x.S(n, "Name"), x.C(n, "Color")); x.Then(n); });
			def("BP.ResetMaterial", "Reset Material", "Material", 160.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target() }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) { if (auto* m = UniqueMaterial(x.E(n))) m->ResetOverrides(); x.Then(n); });

			// ---------------- Particles ----------------
			auto particles = [](Entity e) -> ParticleSystemComponent* { return (e && e.HasComponent<ParticleSystemComponent>()) ? &e.GetComponent<ParticleSystemComponent>() : nullptr; };
			def("BP.EmitParticles", "Emit Particles", "Particles", 170.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), P("Count", PinType::Int, glm::vec4(10.0f)) }; n.Outputs = { FlowOut() }; },
				[particles](Exec& x, const Node& n, const std::string&) { if (auto* ps = particles(x.E(n))) ps->Emit(x.I(n, "Count")); x.Then(n); });
			def("BP.PlayParticles", "Play Particles", "Particles", 160.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target() }; n.Outputs = { FlowOut() }; },
				[particles](Exec& x, const Node& n, const std::string&) { if (auto* ps = particles(x.E(n))) ps->Play(); x.Then(n); });
			def("BP.StopParticles", "Stop Particles", "Particles", 160.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target() }; n.Outputs = { FlowOut() }; },
				[particles](Exec& x, const Node& n, const std::string&) { if (auto* ps = particles(x.E(n))) ps->Stop(); x.Then(n); });
			def("BP.SetParticleRate", "Set Particle Rate", "Particles", 170.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), P("Rate", PinType::Float, glm::vec4(20.0f)) }; n.Outputs = { FlowOut() }; },
				[particles](Exec& x, const Node& n, const std::string&) { if (auto* ps = particles(x.E(n))) ps->Rate = std::max(0.0f, x.F(n, "Rate")); x.Then(n); });

			// ---------------- Animation ----------------
			def("BP.PlayAnimation", "Play Animation", "Animation", 170.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target(), P("Clip", PinType::Int, glm::vec4(-1.0f)) }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) {
					Entity e = x.E(n);
					if (e && e.HasComponent<AnimatorComponent>()) e.GetComponent<AnimatorComponent>().Play(x.I(n, "Clip"));
					x.Then(n);
				}).Description = "Clip -1 keeps the current clip";
			def("BP.StopAnimation", "Stop Animation", "Animation", 160.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Target() }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) {
					Entity e = x.E(n);
					if (e && e.HasComponent<AnimatorComponent>()) e.GetComponent<AnimatorComponent>().Stop();
					x.Then(n);
				});

			// ---------------- Input ----------------
			def("BP.IsKeyDown", "Is Key Down", "Input Devices", 150.0f,
				[](Node& n) { n.Outputs = { P("Down", PinType::Bool) }; n.Properties["Key"] = "Space"; },
				[](Exec& x, const Node& n, const std::string&) { x.SetB(n, "Down", KeyDown(KeyCodeOf(n))); }).Enums = { { "Key", keyNames } };
			def("BP.InputAxis", "Input Axis", "Input Devices", 150.0f,
				[](Node& n) { n.Outputs = { P("Value", PinType::Float) }; n.Properties["Axis"] = "Horizontal"; },
				[](Exec& x, const Node& n, const std::string&) {
					const bool h = Prop(n, "Axis", "Horizontal") == "Horizontal";
					const float pos = (KeyDown(h ? 'D' : 'W') || KeyDown(h ? 262 : 265)) ? 1.0f : 0.0f;
					const float neg = (KeyDown(h ? 'A' : 'S') || KeyDown(h ? 263 : 264)) ? 1.0f : 0.0f;
					x.SetF(n, "Value", pos - neg);
				}).Enums = { { "Axis", { "Horizontal", "Vertical" } } };

			// ---------------- Math ----------------
			def("BP.Float", "Float", "Math", 130.0f,
				[](Node& n) { n.Inputs = { P("Value", PinType::Float) }; n.Outputs = { P("Out", PinType::Float) }; },
				[](Exec& x, const Node& n, const std::string&) { x.SetF(n, "Out", x.F(n, "Value")); });
			def("BP.Vector", "Vector", "Math", 150.0f,
				[](Node& n) { n.Inputs = { P("Value", PinType::Vec3) }; n.Outputs = { P("Out", PinType::Vec3) }; },
				[](Exec& x, const Node& n, const std::string&) { x.SetV(n, "Out", x.V3(n, "Value")); });
			auto& math = def("BP.Math", "Math", "Math", 150.0f,
				[](Node& n) { n.Inputs = { P("A", PinType::Float), P("B", PinType::Float) }; n.Outputs = { P("Result", PinType::Float) }; n.Properties["Operation"] = "Add"; },
				[](Exec& x, const Node& n, const std::string&) {
					const float a = x.F(n, "A"), b = x.F(n, "B");
					const std::string op = Prop(n, "Operation", "Add");
					float r = 0.0f;
					if (op == "Add") r = a + b; else if (op == "Subtract") r = a - b; else if (op == "Multiply") r = a * b;
					else if (op == "Divide") r = b != 0.0f ? a / b : 0.0f; else if (op == "Power") r = std::pow(a, b);
					else if (op == "Minimum") r = std::min(a, b); else if (op == "Maximum") r = std::max(a, b);
					else if (op == "Modulo") r = b != 0.0f ? std::fmod(a, b) : 0.0f;
					else if (op == "Sine") r = std::sin(a); else if (op == "Cosine") r = std::cos(a);
					else if (op == "Absolute") r = std::abs(a); else if (op == "Square Root") r = a > 0.0f ? std::sqrt(a) : 0.0f;
					else if (op == "Floor") r = std::floor(a); else if (op == "Ceil") r = std::ceil(a); else if (op == "Negate") r = -a;
					x.SetF(n, "Result", r);
				});
			math.Enums = { { "Operation", { "Add", "Subtract", "Multiply", "Divide", "Power", "Minimum", "Maximum", "Modulo",
				"Sine", "Cosine", "Absolute", "Square Root", "Floor", "Ceil", "Negate" } } };
			math.PinVisible = [](const Node& n, const Pin& p) {
				static const char* unary[] = { "Sine", "Cosine", "Absolute", "Square Root", "Floor", "Ceil", "Negate" };
				if (p.Name != "B") return true;
				const std::string op = Prop(n, "Operation", "Add");
				for (const char* u : unary) if (op == u) return false;
				return true;
			};
			def("BP.Compare", "Compare", "Math", 150.0f,
				[](Node& n) { n.Inputs = { P("A", PinType::Float), P("B", PinType::Float) }; n.Outputs = { P("Result", PinType::Bool) }; n.Properties["Operation"] = ">"; },
				[](Exec& x, const Node& n, const std::string&) {
					const float a = x.F(n, "A"), b = x.F(n, "B");
					const std::string op = Prop(n, "Operation", ">");
					bool r = op == ">" ? a > b : op == "<" ? a < b : op == ">=" ? a >= b : op == "<=" ? a <= b : op == "==" ? std::abs(a - b) < 1e-5f : std::abs(a - b) >= 1e-5f;
					x.SetB(n, "Result", r);
				}).Enums = { { "Operation", { ">", "<", ">=", "<=", "==", "!=" } } };
			def("BP.BoolOp", "Boolean", "Math", 140.0f,
				[](Node& n) { n.Inputs = { P("A", PinType::Bool), P("B", PinType::Bool) }; n.Outputs = { P("Result", PinType::Bool) }; n.Properties["Operation"] = "And"; },
				[](Exec& x, const Node& n, const std::string&) {
					const bool a = x.B(n, "A"), b = x.B(n, "B");
					const std::string op = Prop(n, "Operation", "And");
					x.SetB(n, "Result", op == "And" ? (a && b) : op == "Or" ? (a || b) : op == "Xor" ? (a != b) : !a);
				}).Enums = { { "Operation", { "And", "Or", "Xor", "Not" } } };
			def("BP.MakeVector", "Make Vector", "Math", 140.0f,
				[](Node& n) { n.Inputs = { P("X", PinType::Float), P("Y", PinType::Float), P("Z", PinType::Float) }; n.Outputs = { P("Vector", PinType::Vec3) }; },
				[](Exec& x, const Node& n, const std::string&) { x.SetV(n, "Vector", glm::vec3(x.F(n, "X"), x.F(n, "Y"), x.F(n, "Z"))); });
			def("BP.BreakVector", "Break Vector", "Math", 140.0f,
				[](Node& n) { n.Inputs = { P("Vector", PinType::Vec3) }; n.Outputs = { P("X", PinType::Float), P("Y", PinType::Float), P("Z", PinType::Float) }; },
				[](Exec& x, const Node& n, const std::string&) { const glm::vec3 v = x.V3(n, "Vector"); x.SetF(n, "X", v.x); x.SetF(n, "Y", v.y); x.SetF(n, "Z", v.z); });
			def("BP.VectorMath", "Vector Math", "Math", 160.0f,
				[](Node& n) { n.Inputs = { P("A", PinType::Vec3), P("B", PinType::Vec3), P("Scale", PinType::Float, glm::vec4(1.0f)) }; n.Outputs = { P("Vector", PinType::Vec3), P("Value", PinType::Float) }; n.Properties["Operation"] = "Add"; },
				[](Exec& x, const Node& n, const std::string&) {
					const glm::vec3 a = x.V3(n, "A"), b = x.V3(n, "B");
					const std::string op = Prop(n, "Operation", "Add");
					glm::vec3 r(0.0f); float v = 0.0f;
					if (op == "Add") r = a + b; else if (op == "Subtract") r = a - b; else if (op == "Multiply") r = a * b;
					else if (op == "Scale") r = a * x.F(n, "Scale"); else if (op == "Normalize") r = glm::length(a) > 0.0f ? glm::normalize(a) : a;
					else if (op == "Cross") r = glm::cross(a, b);
					else if (op == "Length") v = glm::length(a); else if (op == "Distance") v = glm::distance(a, b); else if (op == "Dot") v = glm::dot(a, b);
					x.SetV(n, "Vector", r); x.SetF(n, "Value", v);
				}).Enums = { { "Operation", { "Add", "Subtract", "Multiply", "Scale", "Normalize", "Cross", "Length", "Distance", "Dot" } } };
			def("BP.Lerp", "Lerp", "Math", 140.0f,
				[](Node& n) { n.Inputs = { P("A", PinType::Float), P("B", PinType::Float, glm::vec4(1.0f)), P("T", PinType::Float, glm::vec4(0.5f)) }; n.Outputs = { P("Result", PinType::Float) }; },
				[](Exec& x, const Node& n, const std::string&) { x.SetF(n, "Result", glm::mix(x.F(n, "A"), x.F(n, "B"), x.F(n, "T"))); });
			def("BP.Random", "Random Range", "Math", 150.0f,
				[](Node& n) { n.Inputs = { P("Min", PinType::Float), P("Max", PinType::Float, glm::vec4(1.0f)) }; n.Outputs = { P("Value", PinType::Float) }; },
				[](Exec& x, const Node& n, const std::string&) {
					static uint32_t s = 0x1234567u;
					s ^= s << 13; s ^= s >> 17; s ^= s << 5;
					x.SetF(n, "Value", glm::mix(x.F(n, "Min"), x.F(n, "Max"), static_cast<float>(s & 0xFFFFFF) / 16777215.0f));
				});
			def("BP.Time", "Time", "Math", 120.0f,
				[](Node& n) { n.Outputs = { P("Seconds", PinType::Float) }; },
				[](Exec& x, const Node& n, const std::string&) { x.SetF(n, "Seconds", x.Inst.Variables["__time"].V.x); });

			// ---------------- Variables ----------------
			def("BP.GetVariable", "Get Variable", "Variables", 160.0f,
				[](Node& n) { n.Outputs = { P("Value", PinType::Float), P("Vector", PinType::Vec3) }; n.Properties["Name"] = "Speed"; },
				[](Exec& x, const Node& n, const std::string&) {
					const Value& v = x.Inst.Variables[Prop(n, "Name")];
					x.SetF(n, "Value", v.V.x); x.SetV(n, "Vector", glm::vec3(v.V));
				}).Description = "Reads a blueprint variable (per entity)";
			def("BP.SetVariable", "Set Variable", "Variables", 160.0f,
				[](Node& n) { n.Inputs = { FlowIn(), P("Value", PinType::Float) }; n.Outputs = { FlowOut() }; n.Properties["Name"] = "Speed"; },
				[](Exec& x, const Node& n, const std::string&) { x.Inst.Variables[Prop(n, "Name")].V = glm::vec4(x.F(n, "Value")); x.Then(n); });
			def("BP.SetVectorVariable", "Set Vector Variable", "Variables", 180.0f,
				[](Node& n) { n.Inputs = { FlowIn(), P("Value", PinType::Vec3) }; n.Outputs = { FlowOut() }; n.Properties["Name"] = "Direction"; },
				[](Exec& x, const Node& n, const std::string&) { x.Inst.Variables[Prop(n, "Name")].V = glm::vec4(x.V3(n, "Value"), 0.0f); x.Then(n); });

			// ---------------- Debug ----------------
			def("BP.Print", "Print", "Debug", 180.0f,
				[](Node& n) { n.Inputs = { FlowIn(), Str("Text", "Hello"), P("Value", PinType::Float) }; n.Outputs = { FlowOut() }; },
				[](Exec& x, const Node& n, const std::string&) {
					const Pin* vp = n.FindInput("Value");
					if (vp && x.G.GetLinkInto(vp->PinID)) LNX_LOG_INFO("[Blueprint] {0} {1}", x.S(n, "Text"), x.F(n, "Value"));
					else LNX_LOG_INFO("[Blueprint] {0}", x.S(n, "Text"));
					x.Then(n);
				});
			return r;
		}();
		return registry;
	}

	std::shared_ptr<NodeGraph> CreateDefault() {
		auto g = std::make_shared<NodeGraph>(&GetRegistry());
		g->AddNode("BP.OnStart", { 0.0f, 0.0f });
		const ID print = g->AddNode("BP.Print", { 260.0f, 0.0f })->NodeID;
		g->FindNode(print)->FindInput("Text")->StringValue = "Blueprint started";
		g->Connect(g->FindNodeByType("BP.OnStart")->FindOutput("Then")->PinID, g->FindNode(print)->FindInput("In")->PinID);
		g->AddNode("BP.OnUpdate", { 0.0f, 200.0f });
		return g;
	}

	// ============================================================================
	// ASSET
	// ============================================================================

	bool BlueprintAsset::Save() const {
		if (!Graph) return false;
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Blueprint" << YAML::Value << Name;
		out << YAML::Key << "Graph" << YAML::Value;
		Graph->Serialize(out);
		out << YAML::EndMap;
		std::error_code ec;
		std::filesystem::create_directories(Path.parent_path(), ec);
		std::ofstream file(Path);
		if (!file) return false;
		file << out.c_str();
		return true;
	}

	Ref<BlueprintAsset> BlueprintAsset::Load(const std::filesystem::path& path) {
		YAML::Node data;
		try { data = YAML::LoadFile(path.string()); }
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Blueprint: cannot load {0}: {1}", path.string(), e.what());
			return nullptr;
		}
		auto asset = CreateRef<BlueprintAsset>();
		asset->Path = path;
		asset->Name = data["Blueprint"] ? data["Blueprint"].as<std::string>() : path.stem().string();
		asset->Graph = std::make_shared<NodeGraph>(&GetRegistry());
		if (data["Graph"]) asset->Graph->Deserialize(data["Graph"]);
		std::error_code ec;
		asset->m_LoadedTime = std::filesystem::last_write_time(path, ec);
		return asset;
	}

	Ref<BlueprintAsset> BlueprintAsset::GetOrLoad(const std::filesystem::path& path) {
		static std::unordered_map<std::string, Ref<BlueprintAsset>> cache;
		const std::string key = std::filesystem::weakly_canonical(path).string();
		std::error_code ec;
		const auto time = std::filesystem::last_write_time(path, ec);
		auto it = cache.find(key);
		if (it != cache.end() && it->second && (ec || it->second->m_LoadedTime == time)) return it->second;
		auto asset = Load(path);
		if (asset) cache[key] = asset;
		return asset;
	}

	Ref<BlueprintAsset> BlueprintAsset::CreateNew(const std::filesystem::path& path) {
		auto asset = CreateRef<BlueprintAsset>();
		asset->Path = path;
		asset->Name = path.stem().string();
		asset->Graph = CreateDefault();
		asset->Save();
		std::error_code ec;
		asset->m_LoadedTime = std::filesystem::last_write_time(path, ec);
		return asset;
	}

	// ============================================================================
	// INSTANCE
	// ============================================================================

	void Instance::FireEvent(Scene* scene, Entity self, const Node& node, float deltaTime) {
		Exec x(scene, self, *m_Asset->Graph, *this, deltaTime);
		x.Run(node, "");
	}

	void Instance::Start(Scene* scene, Entity self) {
		if (!m_Asset || !m_Asset->Graph) return;
		Variables.clear(); Timers.clear(); Flags.clear(); Delays.clear();
		for (const auto& n : m_Asset->Graph->GetNodes())
			if (n.TypeName == "BP.OnStart") FireEvent(scene, self, n, 0.0f);
	}

	void Instance::Update(Scene* scene, Entity self, float dt) {
		if (!m_Asset || !m_Asset->Graph) return;
		Variables["__time"].V.x += dt;
		const NodeGraph& g = *m_Asset->Graph;

		for (const auto& n : g.GetNodes()) {
			if (n.TypeName == "BP.OnUpdate") FireEvent(scene, self, n, dt);
			else if (n.TypeName == "BP.OnKeyPressed" || n.TypeName == "BP.OnKeyReleased") {
				const bool down = KeyDown(KeyCodeOf(n));
				bool& was = Flags[n.NodeID];
				if (n.TypeName == "BP.OnKeyPressed" ? (down && !was) : (!down && was)) FireEvent(scene, self, n, dt);
				was = down;
			}
			else if (n.TypeName == "BP.OnTimer") {
				Exec x(scene, self, g, *this, dt);
				const float interval = std::max(0.01f, x.F(n, "Interval"));
				float& t = Timers[n.NodeID];
				t += dt;
				if (t >= interval) { t -= interval; FireEvent(scene, self, n, dt); }
			}
		}

		// Latent Delay nodes
		if (!Delays.empty()) {
			std::vector<PendingDelay> ready;
			for (auto it = Delays.begin(); it != Delays.end();) {
				it->Remaining -= dt;
				if (it->Remaining <= 0.0f) { ready.push_back(*it); it = Delays.erase(it); }
				else ++it;
			}
			for (const auto& d : ready) {
				if (const Node* n = g.FindNode(d.Node)) {
					Exec x(scene, self, g, *this, dt);
					x.Run(*n, "__resume");
				}
			}
		}
	}

} // namespace Lunex::Blueprint
