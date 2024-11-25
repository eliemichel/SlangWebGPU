#include <slang-webgpu/common/result.h>
#include <slang-webgpu/common/logger.h>
#include <slang-webgpu/common/io.h>
#include <slang-webgpu/common/variant-utils.h>
#include <slang-webgpu/common/slang-result-utils.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <magic_enum/magic_enum.hpp>
#include <CLI11.hpp>

#include <filesystem>
#include <functional>
#include <variant>

using namespace slang;
using magic_enum::enum_name;

/**
 * Command line arguments
 */
struct Arguments {
	std::string name;
	std::filesystem::path inputSlang;
	std::filesystem::path inputTemplate;
	std::filesystem::path outputWgsl;
	std::filesystem::path outputHpp;
	std::filesystem::path outputCpp;
	std::vector<std::string> entryPoints;
	std::vector<std::string> includeDirectories;
};

Result<Void, Error> run(const Arguments& args);

int main(int argc, char* argv[]) {
	CLI::App app{ "App description" };
	argv = app.ensure_utf8(argv);

	Arguments args;
	app.add_option("-n,--name", args.name, "Name of the shader module. This must be a valid C identifier.")
		->required();
	app.add_option("-i,--input-slang", args.inputSlang, "Path to the input Slang shader source")
		->required()
		->check(CLI::ExistingFile);
	auto inputTemplateOpt = app.add_option("-t,--input-template", args.inputTemplate, "Path to the template used to generate binding source")
		->check(CLI::ExistingFile);
	app.add_option("-w,--output-wgsl", args.outputWgsl, "Path to the output WGSL shader source");
	auto outputHppOpt = app.add_option("-d,--output-hpp", args.outputHpp, "Path to the output C++ header file that define kernels for each entry point");
	auto outputCppOpt = app.add_option("-c,--output-cpp", args.outputCpp, "Path to the output C++ source file that implements the header file");
	app.add_option("-e,--entrypoint,--entrypoints", args.entryPoints, "Entry points to generate kernel for")
		->required()
		->delimiter(',');
	app.add_option("-I,--include-directories", args.includeDirectories, "Directories where to look for includes in slang shader")
		->delimiter(',');

	// These options need each others
	outputHppOpt->needs(outputCppOpt, inputTemplateOpt);
	outputCppOpt->needs(outputHppOpt, inputTemplateOpt);
	inputTemplateOpt->needs(outputHppOpt, outputCppOpt);

	CLI11_PARSE(app, argc, argv);

	auto maybeError = run(args);

	if (isError(maybeError)) {
		LOG(ERROR) << std::get<Error>(maybeError).message;
		return 1;
	}

	return 0;
}

Result<Slang::ComPtr<ISession>, Error> createSlangSession(
	std::vector<std::string> includeDirectories
) {

	// This function is highly based on instructions found at
	// https://shader-slang.com/slang/user-guide/compiling#using-the-compilation-api

	LOG(INFO) << "Creating global Slang session...";
	Slang::ComPtr<IGlobalSession> globalSession;
	TRY_SLANG(createGlobalSession(globalSession.writeRef()));

	LOG(INFO) << "Creating Slang session...";
	SessionDesc sessionDesc;

	TargetDesc target;
	target.format = SLANG_WGSL;
	sessionDesc.targets = &target;
	sessionDesc.targetCount = 1;

	std::vector<const char*> includeDirectoriesData(includeDirectories.size());
	std::transform(
		includeDirectories.cbegin(),
		includeDirectories.cend(),
		includeDirectoriesData.begin(),
		[](const std::string& path) { return path.c_str(); }
	);
	sessionDesc.searchPaths = includeDirectoriesData.data();
	sessionDesc.searchPathCount = includeDirectoriesData.size();

	Slang::ComPtr<ISession> session;
	TRY_SLANG(globalSession->createSession(sessionDesc, session.writeRef()));

	return session;
}

Result<Slang::ComPtr<IComponentType>, Error> loadSlangModule(
	Slang::ComPtr<ISession> session,
	const std::string& name,
	const std::filesystem::path& inputSlang,
	const std::vector<std::string>& entryPoints
) {

	// This function is highly based on instructions found at
	// https://shader-slang.com/slang/user-guide/compiling#using-the-compilation-api

	LOG(INFO) << "Loading file '" << inputSlang << "'...";
	std::string source;
	TRY_ASSIGN(source, loadTextFile(inputSlang));

	LOG(INFO) << "Loading Slang module...";
	Slang::ComPtr<IBlob> diagnostics;
	IModule* module = session->loadModuleFromSourceString(
		name.c_str(),
		inputSlang.string().c_str(),
		source.c_str(),
		diagnostics.writeRef()
	);
	if (diagnostics) {
		std::string message = (const char*)diagnostics->getBufferPointer();
		return Error{ "Could not load slang module from file '" + inputSlang.string() + "': " + message };
	}

	LOG(INFO) << "Composing shader program...";
	std::vector<IComponentType*> components;
	components.reserve(1 + entryPoints.size());
	components.push_back(module);
	for (const std::string& entryPointName : entryPoints) {
		LOG(INFO) << "- Adding entry point '" << entryPointName << "'...";
		Slang::ComPtr<IEntryPoint> entryPoint;
		TRY_SLANG(module->findEntryPointByName(entryPointName.c_str(), entryPoint.writeRef()));

		components.push_back(entryPoint);
	}
	Slang::ComPtr<IComponentType> program;
	TRY_SLANG(session->createCompositeComponentType(components.data(), components.size(), program.writeRef()));

	return program;
}

Result<Void, Error> compileToWgsl(
	Slang::ComPtr<IComponentType> program,
	const std::filesystem::path& outputWgsl,
	const std::filesystem::path& inputSlang // only to give context in error messages
) {

	// This function is highly based on instructions found at
	// https://shader-slang.com/slang/user-guide/compiling#using-the-compilation-api

	LOG(INFO) << "Linking program...";
	Slang::ComPtr<IComponentType> linkedProgram;
	Slang::ComPtr<ISlangBlob> linkDiagnostics;
	program->link(linkedProgram.writeRef(), linkDiagnostics.writeRef());
	if (linkDiagnostics) {
		std::string message = (const char*)linkDiagnostics->getBufferPointer();
		return Error{ "Could not link slang module from file '" + inputSlang.string() + "': " + message };
	}

	Slang::ComPtr<IBlob> codeBlob;
	Slang::ComPtr<ISlangBlob> codeDiagnostics;
	int targetIndex = 0; // only one target
	TRY_SLANG(linkedProgram->getTargetCode(
		targetIndex,
		codeBlob.writeRef(),
		codeDiagnostics.writeRef()
	));
	if (codeDiagnostics) {
		std::string message = (const char*)codeDiagnostics->getBufferPointer();
		return Error{ "Could not generate WGSL source code from file '" + inputSlang.string() + "': " + message };
	}

	std::string wgslSource = (const char*)codeBlob->getBufferPointer();

	LOG(INFO) << "Writing generated WGSL source into '" << outputWgsl << "'...";
	saveTextFile(outputWgsl, wgslSource);

	return {};
}

/**
 * This is a very basic templating system.
 */
template<typename Generator>
Result<std::string, Error> generateFromTemplate(
	const std::string& tpl,
	const std::string& targetSectionName,
	Generator generator
) {
	enum class ParserState {
		Init,
		InSection,
		Done,
	};
	ParserState state = ParserState::Init;

	// Losely inspired by https://stackoverflow.com/a/2549643/1549389
	std::ostringstream out;
	size_t pos = 0;
	size_t section_end_pos = std::string::npos;
	while (state != ParserState::Done) {
		switch (state) {
		case ParserState::Init: {
			// Look for the beginning of the [[section]]
			size_t section_name_start_pos = tpl.find("[[", pos);
			if (section_name_start_pos == std::string::npos) {
				LOG(WARNING) << "Template does not contain any section named [[" << targetSectionName << "]]";
				state = ParserState::Done;
				break;
			}
			size_t section_name_end_pos = tpl.find("]]", section_name_start_pos);
			if (section_name_end_pos == std::string::npos) {
				return Error{ "Syntax error: Section name starting at position " + std::to_string(section_name_start_pos) + " never ends." };
			}
			section_name_start_pos += 2;
			std::string section_name = tpl.substr(section_name_start_pos, section_name_end_pos - section_name_start_pos);
			if (section_name == targetSectionName) {
				section_end_pos = tpl.find("[[", section_name_end_pos + 2);
				state = ParserState::InSection;
			}
			pos = section_name_end_pos + 2;
			break;
		}
		case ParserState::InSection: {
			// Look for the beginning of an {{expression}}
			size_t expr_start_pos = tpl.find("{{", pos);
			if (expr_start_pos >= section_end_pos) {
				size_t end =
					section_end_pos == std::string::npos
					? tpl.size()
					: section_end_pos;
				out.write(&*tpl.begin() + pos, end - pos);
				state = ParserState::Done;
				break;
			}
			size_t expr_end_pos = tpl.find("}}", expr_start_pos);
			if (expr_end_pos == std::string::npos) {
				return Error{ "Syntax error: Expression starting at position " + std::to_string(expr_start_pos) + " never ends." };
			}

			out.write(&*tpl.begin() + pos, expr_start_pos - pos);

			expr_start_pos += 2;
			std::string expr = tpl.substr(expr_start_pos, expr_end_pos - expr_start_pos);
			TRY(generator.processExpression(expr, out))
			pos = expr_end_pos + 2;
			break;
		}
		case ParserState::Done:
			break;
		}
	}

	return out.str();
}

/**
 * Generator class passed to generateFromTemplate to generate WebGPU C++ bindings.
 */
class BindingGenerator {
public:
	// Type that describe the reflection information that we extract from Slang reflection API.
	struct BufferBindingInfo {
		std::string type;
	};
	using BindingInfo = std::variant<BufferBindingInfo>;

public:
	BindingGenerator(
		const std::string& name,
		slang::ProgramLayout* layout,
		const std::filesystem::path& wgsl
	)
		: m_name(name)
		, m_layout(layout)
		, m_wgsl(wgsl)
	{}

	Result<Void, Error> processExpression(const std::string& expr, std::ostringstream& out) {
		if (expr == "kernelName") {
			out << m_name;
		}
		else if (expr == "kernelLabel") {
			out << m_name;
		}
		else if (expr == "sourcePath") {
			// TODO: make relative
			// TODO: Embbed source code?
			// TODO: Handle case where this path is not provided
			out << m_wgsl.string();
		}
		else if (expr == "workgroupSize") {
			// TODO: specify on a per-entrypoint basis
			EntryPointReflection* entryPoint = m_layout->getEntryPointByIndex(0);
			std::array<SlangUInt, 3> size;
			entryPoint->getComputeThreadGroupSize(3, size.data());
			out << "{ " << size[0] << ", " << size[1] << ", " << size[2] << " }";
		}
		else if (expr == "entryPoint") {
			// TODO: specify on a per-entrypoint basis
			EntryPointReflection* entryPoint = m_layout->getEntryPointByIndex(0);
			out << entryPoint->getName();
		}
		else if (expr == "bindGroupMembers") {
			TRY(visitBindings([&](unsigned i, VariableLayoutReflection* parameter, const BindingInfo& info) {
				if (i > 0) out << ",\n\t\t";
				std::visit(overloaded{
					[&](const BufferBindingInfo&) {
						out << "wgpu::Buffer " << parameter->getName();
					}
				}, info);
			}));
		}
		else if (expr == "bindGroupMembersImpl") {
			TRY(visitBindings([&](unsigned i, VariableLayoutReflection* parameter, const BindingInfo& info) {
				if (i > 0) out << ",\n\t";
				std::visit(overloaded{
					[&](const BufferBindingInfo&) {
						out << "Buffer " << parameter->getName();
					}
				}, info);
			}));
		}
		else if (expr == "bindGroupLayoutEntries") {
			static constexpr const char* nl = "\n\t";
			TRY(visitBindings([&](unsigned i, VariableLayoutReflection* parameter, const BindingInfo& info) {
				if (i > 0) out << nl << nl;
				out << "// Member '" << parameter->getName() << "'" << nl;
				out << "layoutEntries[" << i << "].binding = " << parameter->getBindingIndex() << ";" << nl;
				out << "layoutEntries[" << i << "].visibility = ShaderStage::Compute;" << nl;
				std::visit(overloaded{
					[&](const BufferBindingInfo& bufferBinding) {
						out << "layoutEntries[" << i << "].buffer.type = BufferBindingType::" << bufferBinding.type << ";";
					}
				}, info);
			}));
		}
		else if (expr == "bindGroupEntries") {
			static constexpr const char* nl = "\n\t";
			TRY(visitBindings([&](unsigned i, VariableLayoutReflection* parameter, const BindingInfo& info) {
				if (i > 0) out << nl << nl;
				out << "entries[" << i << "].binding = " << parameter->getBindingIndex() << ";" << nl;
				std::visit(overloaded{
					[&](const BufferBindingInfo&) {
						out << "entries[" << i << "].buffer = " << parameter->getName() << ";" << nl;
						out << "entries[" << i << "].size = " << parameter->getName() << ".getSize();";
					}
				}, info);
			}));
		}
		else {
			return Error{ "Invalid template expression: " + expr };
		}
		return {};
	};

private:
	/**
	 * An internal utility function that visits all the bindings and provides to
	 * the visitor the reflection information that we actually need.
	 */
	Result<Void, Error> visitBindings(
		const std::function<void(unsigned i, VariableLayoutReflection* parameter, const BindingInfo& info)>& visitor
	) {
		unsigned parameterCount = m_layout->getParameterCount();
		for (unsigned i = 0; i < parameterCount; ++i) {
			VariableLayoutReflection* parameter = m_layout->getParameterByIndex(i);
			ParameterCategory category = parameter->getCategory();
			TRY_ASSERT(
				category == ParameterCategory::DescriptorTableSlot,
				"Other categories than 'descriptor table slot' are not supported, but found category '" << enum_name(category) << "'"
			);
			TRY_ASSERT(
				parameter->getBindingSpace() == 0,
				"Use of more than one bind group is not supported."
			);

			TypeLayoutReflection* typeLayout = parameter->getTypeLayout();
			TypeReflection::Kind kind = typeLayout->getKind();
			TRY_ASSERT(
				kind == TypeReflection::Kind::Resource,
				"Only resource bindings are supported, but found kind '" << enum_name(kind) << "'"
			);

			size_t regCount = typeLayout->getSize((SlangParameterCategory)category);
			TRY_ASSERT(
				regCount == 1,
				"Use of multiple bind groups by a single parameter is not supported, but found regCount = " << regCount
			);

			SlangResourceShape shape = typeLayout->getResourceShape();
			TRY_ASSERT(
				shape == SLANG_STRUCTURED_BUFFER,
				"Only structured buffers are supported, but found resource shape '" << enum_name(shape) << "'"
			);

			SlangResourceAccess access = typeLayout->getResourceAccess();
			BufferBindingInfo bufferBinding;
			switch (access) {
			case SLANG_RESOURCE_ACCESS_READ:
				bufferBinding.type = "ReadOnlyStorage";
				break;
			case SLANG_RESOURCE_ACCESS_READ_WRITE:
				bufferBinding.type = "Storage";
				break;
			default:
				return Error{ "SlangResourceAccess '" + std::string(enum_name(access)) + "' is not supported." };
			}

			visitor(i, parameter, bufferBinding);
		}
		return {};
	}

private:
	const std::string m_name;
	slang::ProgramLayout* m_layout;
	const std::filesystem::path m_wgsl;
};

Result<Void, Error> generateCppBinding(
	Slang::ComPtr<IComponentType> program,
	const std::string& name,
	[[maybe_unused]] const std::vector<std::string>& entryPoints,
	const std::filesystem::path& inputTemplate,
	const std::filesystem::path& outputWgsl,
	const std::filesystem::path& outputHpp,
	const std::filesystem::path& outputCpp
) {
	LOG(INFO) << "Getting reflection information...";
	slang::ProgramLayout* layout = program->getLayout();
	LOG(INFO) << "layout: " << layout;

	LOG(INFO) << "Global program parameters:";
	{
		unsigned parameterCount = layout->getParameterCount();
		for (unsigned i = 0; i < parameterCount; ++i)
		{
			slang::VariableLayoutReflection* parameter = layout->getParameterByIndex(i);
			LOG(INFO) << "- #" << i << ": " << parameter->getName();
		}
	}

	LOG(INFO) << "Program entry points:";
	SlangUInt entryPointCount = layout->getEntryPointCount();
	for (SlangUInt i = 0; i < entryPointCount; ++i)
	{
		slang::EntryPointReflection* entryPoint = layout->getEntryPointByIndex(i);
		LOG(INFO) << "- #" << i << ": " << entryPoint->getName();
		entryPoint->getParameterCount();
		LOG(INFO) << "  Entry point parameters:";
		unsigned parameterCount = entryPoint->getParameterCount();
		for (unsigned j = 0; j < parameterCount; ++j)
		{
			slang::VariableLayoutReflection* parameter = entryPoint->getParameterByIndex(j);
			LOG(INFO) << "  - #" << j << ": " << parameter->getName();
		}
	}

	LOG(INFO) << "Loading binding template from " << inputTemplate << "...";
	std::string tpl;
	TRY_ASSIGN(tpl, loadTextFile(inputTemplate));

	BindingGenerator generator(name, layout, outputWgsl);

	LOG(INFO) << "Generating binding header into " << outputHpp << "...";
	std::string hpp;
	TRY_ASSIGN(hpp, generateFromTemplate(tpl, "header", generator));
	TRY(saveTextFile(outputHpp, hpp));

	LOG(INFO) << "Generating binding implementation into " << outputCpp << "...";
	std::string cpp;
	TRY_ASSIGN(cpp, generateFromTemplate(tpl, "implementation", generator));
	TRY(saveTextFile(outputCpp, cpp));

	return {};
}

Result<Void, Error> run(const Arguments& args) {

	Slang::ComPtr<ISession> session;
	TRY_ASSIGN(session, createSlangSession(args.includeDirectories));

	Slang::ComPtr<IComponentType> program;
	TRY_ASSIGN(program, loadSlangModule(
		session,
		args.name,
		args.inputSlang,
		args.entryPoints
	));

	if (!args.outputWgsl.empty()) {
		TRY(compileToWgsl(
			program,
			args.outputWgsl,
			args.inputSlang
		));
	}

	if (!args.outputHpp.empty()) {
		if (args.outputCpp.empty()) {
			return Error{ "Option --output-cpp must be non-empty when --output-hpp is non-empty."};
		}
		if (args.inputTemplate.empty()) {
			return Error{ "Option --input-template must be non-empty when --output-hpp is non-empty." };
		}
		
		TRY(generateCppBinding(
			program,
			args.name,
			args.entryPoints,
			args.inputTemplate,
			args.outputWgsl,
			args.outputHpp,
			args.outputCpp
		));
	}

	return {};
}
