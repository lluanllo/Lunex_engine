#include "stpch.h"
#include "ScriptEngine.h"

#include "mono/jit/jit.h"
#include "mono/metadata/assembly.h"
#include "mono/metadata/object.h"

namespace Lunex {
	struct ScriptEngineData {
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;
		MonoAssembly* CoreAssembly = nullptr;
	};

	static ScriptEngineData* s_Data = nullptr;

	void ScriptEngine::Init() {
		s_Data = new ScriptEngineData();
		InitMono();
	}

	void ScriptEngine::Shutdown() {
		ShutdownMono();
		delete s_Data;
	}

	char* ReadBytes(const std::string& filepath, uint32_t* outSize) {
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream)
			return nullptr;

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint32_t size = static_cast<uint32_t>(end - stream.tellg());

		if (size == 0)
			return nullptr;

		char* buffer = new char[size];
		stream.read((char*)buffer, size);
		stream.close();

		*outSize = size;
		return buffer;
	}

	MonoAssembly* LoadCSharpAssembly(const std::string& assemblyPath) {
		uint32_t fileSize = 0;
		char* fileData = ReadBytes(assemblyPath, &fileSize);
		if (!fileData)
			return nullptr;

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);
		if (status != MONO_IMAGE_OK) {
			const char* errorMessage = mono_image_strerror(status);
			LNX_LOG_ERROR("Failed to open Mono image: {0}", errorMessage);
			delete[] fileData;
			return nullptr;
		}

		MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.c_str(), &status, 0);
		mono_image_close(image);
		delete[] fileData;

		return assembly;
	}

	void PrintAssemblyTypes(MonoAssembly* assembly) {
		MonoImage* image = mono_assembly_get_image(assembly);
		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

		for (int32_t i = 0; i < numTypes; i++) {
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

			LNX_LOG_TRACE("{}.{}", nameSpace, name);
		}
	}

	void ScriptEngine::InitMono() {
		namespace fs = std::filesystem;

		// Detecta el directorio actual del ejecutable
		fs::path exePath = fs::current_path();

		// Retrocede hasta encontrar la carpeta raíz del engine (donde está "vendor")
		fs::path engineRoot = exePath;
		while (!fs::exists(engineRoot / "vendor") && engineRoot.has_parent_path())
			engineRoot = engineRoot.parent_path();

		// Rutas absolutas a Mono
		fs::path monoLibPath = engineRoot / "vendor/mono/lib";
		fs::path monoEtcPath = engineRoot / "vendor/mono/etc";
		fs::path monoAssembliesPath = monoLibPath / "mono/4.5";

		// Configurar Mono correctamente
		mono_set_dirs(monoLibPath.string().c_str(), monoEtcPath.string().c_str());
		mono_set_assemblies_path(monoAssembliesPath.string().c_str());

		// Inicializar el dominio raíz
		char runtimeName[] = "LunexJITRuntime";
		MonoDomain* rootDomain = mono_jit_init(runtimeName);
		LNX_CORE_ASSERT(rootDomain);
		s_Data->RootDomain = rootDomain;

		// Crear dominio de aplicación
		char appDomainName[] = "LunexScriptRuntime";
		s_Data->AppDomain = mono_domain_create_appdomain(appDomainName, nullptr);
		mono_domain_set(s_Data->AppDomain, true);

		// Cargar ensamblado principal
		s_Data->CoreAssembly = LoadCSharpAssembly("Resources/Scripts/Lunex-ScriptCore.dll");
		PrintAssemblyTypes(s_Data->CoreAssembly);

		MonoImage* assemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
		MonoClass* monoClass = mono_class_from_name(assemblyImage, "Lunex", "Main");

		// Crear instancia
		MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
		mono_runtime_object_init(instance);

		// Llamadas de prueba
		MonoMethod* printMessageFunc = mono_class_get_method_from_name(monoClass, "PrintMessage", 0);
		mono_runtime_invoke(printMessageFunc, instance, nullptr, nullptr);

		MonoMethod* printIntFunc = mono_class_get_method_from_name(monoClass, "PrintInt", 1);
		int value = 5;
		void* param = &value;
		mono_runtime_invoke(printIntFunc, instance, &param, nullptr);

		MonoMethod* printIntsFunc = mono_class_get_method_from_name(monoClass, "PrintInts", 2);
		int value2 = 508;
		void* params[2] = { &value, &value2 };
		mono_runtime_invoke(printIntsFunc, instance, params, nullptr);

		MonoString* monoString = mono_string_new(s_Data->AppDomain, "Hello World from C++!");
		MonoMethod* printCustomMessageFunc = mono_class_get_method_from_name(monoClass, "PrintCustomMessage", 1);
		void* stringParam = monoString;
		mono_runtime_invoke(printCustomMessageFunc, instance, &stringParam, nullptr);
	}


	void ScriptEngine::ShutdownMono() {
		s_Data->AppDomain = nullptr;
		s_Data->RootDomain = nullptr;
	}
}
