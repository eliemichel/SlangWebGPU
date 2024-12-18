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
#include <string_view>
#include <optional>
#include <algorithm>
#include <deque>

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
	std::filesystem::path outputDepfile;
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
	auto outputHppOpt = app.add_option("-g,--output-hpp", args.outputHpp, "Path to the output C++ header file that define kernels for each entry point");
	auto outputCppOpt = app.add_option("-c,--output-cpp", args.outputCpp, "Path to the output C++ source file that implements the header file");
	app.add_option("-d,--output-depfile", args.outputDepfile, "Path to the depfile that lists dependencies of the shader through import statements. This is designed to be used with CMake's DEPFILE option in add_custom_command().");
	app.add_option("-e,--entrypoint,--entrypoints", args.entryPoints, "Entry points to generate kernel for")
		->required()
		->delimiter(';');
	app.add_option("-I,--include-directories", args.includeDirectories, "Directories where to look for includes in slang shader")
		->delimiter(';');

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

struct SessionInfo {
	// We need to keep the global session alive throughout the entire program.
	Slang::ComPtr<IGlobalSession> globalSession;
	Slang::ComPtr<ISession> session;
};

Result<SessionInfo, Error> createSlangSession(
	std::vector<std::string> includeDirectories
) {

	// This function is highly based on instructions found at
	// https://shader-slang.com/slang/user-guide/compiling#using-the-compilation-api

	LOG(INFO) << "Creating global Slang session...";
	SessionInfo sessionInfo;
	TRY_SLANG(createGlobalSession(sessionInfo.globalSession.writeRef()));

	LOG(INFO) << "Creating Slang session...";
	SessionDesc sessionDesc;

	TargetDesc target;
	target.format = SLANG_WGSL;
	sessionDesc.targets = &target;
	sessionDesc.targetCount = 1;

	if (!includeDirectories.empty()) {
		LOG(INFO) << "Extra include directories:";
		for (const auto& dir : includeDirectories) {
			LOG(INFO) << " - " << dir;
		}
	}
	std::vector<const char*> includeDirectoriesData(includeDirectories.size());
	std::transform(
		includeDirectories.cbegin(),
		includeDirectories.cend(),
		includeDirectoriesData.begin(),
		[](const std::string& path) { return path.c_str(); }
	);
	sessionDesc.searchPaths = includeDirectoriesData.data();
	sessionDesc.searchPathCount = includeDirectoriesData.size();

	TRY_SLANG(sessionInfo.globalSession->createSession(sessionDesc, sessionInfo.session.writeRef()));

	return sessionInfo;
}

struct ModuleInfo {
	Slang::ComPtr<IComponentType> program;
	std::vector<std::string> dependencyFiles;
};

Result<ModuleInfo, Error> loadSlangModule(
	const Slang::ComPtr<ISession>& session,
	const std::string& name,
	const std::filesystem::path& inputSlang,
	const std::vector<std::string>& entryPoints
) {

	// This function is highly based on instructions found at
	// https://shader-slang.com/slang/user-guide/compiling#using-the-compilation-api

	LOG(INFO) << "Loading file " << inputSlang << "...";
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
		return Error{ "Could not load slang module from file '" + inputSlang.string() + "':\n" + message };
	}

	// Collect dependencies
	int depCount = module->getDependencyFileCount();
	std::vector<std::string> dependencyFiles(depCount);
	LOG(INFO) << "Found " << depCount << " dependencies:";
	for (int i = 0; i < depCount; ++i) {
		dependencyFiles[i] = module->getDependencyFilePath(i);
		LOG(INFO) << " - " << dependencyFiles[i];
	}

	LOG(INFO) << "Composing shader program...";
	std::vector<IComponentType*> components;
	components.reserve(1 + entryPoints.size());
	components.push_back(module);
	for (const std::string& entryPointName : entryPoints) {
		LOG(INFO) << "- Adding entry point '" << entryPointName << "'...";
		Slang::ComPtr<IEntryPoint> entryPoint;
		SlangResult res = module->findEntryPointByName(entryPointName.c_str(), entryPoint.writeRef());
		if (SLANG_FAILED(res)) {
			return Error{ "Entrypoint '" + entryPointName + "' not found in shader '" + inputSlang.string() + "'."};
		}

		components.push_back(entryPoint);
	}
	Slang::ComPtr<IComponentType> program;
	TRY_SLANG(session->createCompositeComponentType(components.data(), components.size(), program.writeRef()));

	return ModuleInfo{
		program,
		dependencyFiles
	};
}

Result<std::string, Error> compileToWgsl(
	const Slang::ComPtr<IComponentType>& program,
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

	return wgslSource;
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

	// For loop/condition stack
	struct Frame {
		std::string iterator_name = "";
		size_t begin_pos = std::string::npos;
		bool parent_emit = true; // value of "emit" outside of the loop
		bool is_loop = false; // either a loop or a condition
	};

	// Losely inspired by https://stackoverflow.com/a/2549643/1549389
	std::ostringstream out;
	size_t pos = 0;
	size_t section_end_pos = std::string::npos;
	std::vector<Frame> execution_stack;
	bool emit = true; // switched to false when skipping an empty loop's body
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
				if (emit) out.write(&*tpl.begin() + pos, end - pos);
				state = ParserState::Done;
				break;
			}
			size_t expr_end_pos = tpl.find("}}", expr_start_pos);
			if (expr_end_pos == std::string::npos) {
				return Error{ "Syntax error: Expression starting at position " + std::to_string(expr_start_pos) + " never ends." };
			}

			if (emit) out.write(&*tpl.begin() + pos, expr_start_pos - pos);

			expr_start_pos += 2;
			std::string expr = tpl.substr(expr_start_pos, expr_end_pos - expr_start_pos);

			pos = expr_end_pos + 2;
			bool is_foreach = expr.rfind("foreach ", 0) == 0;
			bool is_if = expr.rfind("if ", 0) == 0;
			if (is_foreach || is_if) {
				// NB: 'if' is just a 'foreach' on an iterator that has a single entry
				std::string iterator_name = expr.substr(is_foreach ? 8 : 3);
				Frame frame = {
					/*iterator_name=*/iterator_name,
					/*begin_pos=*/pos,
					/*parent_emit=*/emit,
					/*is_loop=*/is_foreach,
				};
				TRY(generator.resetIterator(frame.iterator_name));
				bool ended;
				TRY_ASSIGN(ended, generator.iteratorEnded(frame.iterator_name));
				emit = !ended;
				execution_stack.push_back(frame);
			}
			else if (expr == "end") {
				if (execution_stack.empty()) {
					return Error{ "Syntax error: Statement {{end}} found while there was no ongoing loop or condition, at position " + std::to_string(expr_start_pos) + "." };
				}
				Frame& frame = execution_stack.back();
				TRY(generator.stepIterator(frame.iterator_name));
				bool ended = true;
				if (frame.is_loop) {
					TRY_ASSIGN(ended, generator.iteratorEnded(frame.iterator_name));
				}
				if (ended) {
					emit = frame.parent_emit;
					execution_stack.pop_back();
				}
				else {
					pos = frame.begin_pos;
				}
			}
			else {
				if (emit) TRY(generator.processExpression(expr, out));
			}
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
		std::optional<size_t> minBindingSize;
	};
	using BindingDetails = std::variant<BufferBindingInfo>;
	struct BindingInfo {
		uint32_t index;
		std::string name;
		BindingDetails details;
	};

	struct UniformInfo {
		size_t minBindingSize = 0;
	};

	struct LayoutInfo {
		std::optional<UniformInfo> uniforms;
		std::deque<BindingInfo> bindings;
	};

public:
	BindingGenerator(
		const std::string& name,
		slang::ProgramLayout* layout,
		const std::string& wgslSource
	)
		: m_name(name)
		, m_layout(layout)
		, m_wgslSource(wgslSource)
	{
		m_initError = buildLayoutInfo();
	}

	Result<Void, Error> check() const {
		return m_initError;
	}

	Result<Void, Error> processExpression(const std::string& expr, std::ostringstream& out) {
		if (expr == "kernelName") {
			out << m_name;
		}
		else if (expr == "kernelLabel") {
			out << m_name;
		}
		else if (expr == "wgslSource") {
			out << m_wgslSource;
		}
		else if (expr == "workgroupSize") {
			EntryPointReflection* entryPoint = m_layout->getEntryPointByIndex(m_currentEntryPoint);
			std::array<SlangUInt, 3> size;
			entryPoint->getComputeThreadGroupSize(3, size.data());
			out << "{ " << size[0] << ", " << size[1] << ", " << size[2] << " }";
		}
		else if (expr == "entryPoint") {
			EntryPointReflection* entryPoint = m_layout->getEntryPointByIndex(m_currentEntryPoint);
			out << entryPoint->getName();
		}
		else if (expr == "EntryPoint") {
			std::string entryPointName = m_layout->getEntryPointByIndex(m_currentEntryPoint)->getName();
			TRY_ASSERT(entryPointName.size() > 0, "An entry point's name should not be empty");
			entryPointName[0] = (char)std::toupper((int)entryPointName[0]);
			out << entryPointName;
		}
		else if (expr == "entryPointCount") {
			out << m_layout->getEntryPointCount();
		}
		else if (expr == "entryPointIndex") {
			out << m_currentEntryPoint;
		}
		else if (expr == "bindGroupEntryCount") {
			size_t count = 0;
			TRY(visitBindings([&count](unsigned, const BindingInfo&) {
				count += 1;
			}));
			out << count;
		}
		else if (expr == "bindGroupMembers") {
			TRY(visitBindings([&](unsigned i, const BindingInfo& binding) {
				if (i > 0) out << ",\n\t\t";
				std::visit(overloaded{
					[&](const BufferBindingInfo&) {
						out << "wgpu::Buffer " << binding.name;
					}
				}, binding.details);
			}));
		}
		else if (expr == "bindGroupMembersImpl") {
			TRY(visitBindings([&](unsigned i, const BindingInfo& binding) {
				if (i > 0) out << ",\n\t";
				std::visit(overloaded{
					[&](const BufferBindingInfo&) {
						out << "Buffer " << binding.name;
					}
				}, binding.details);
			}));
		}
		else if (expr == "bindGroupLayoutEntries") {
			static constexpr const char* nl = "\n\t";
			TRY(visitBindings([&](unsigned i, const BindingInfo& binding) {
				if (i > 0) out << nl << nl;
				out << "// Member '" << binding.name << "'" << nl;
				out << "layoutEntries[" << i << "].binding = " << binding.index << ";" << nl;
				out << "layoutEntries[" << i << "].visibility = ShaderStage::Compute;" << nl;
				std::visit(overloaded{
					[&](const BufferBindingInfo& bufferBinding) {
						if (bufferBinding.minBindingSize.has_value()) {
							out << "layoutEntries[" << i << "].buffer.minBindingSize = " << bufferBinding.minBindingSize.value() << ";" << nl;
						}
						out << "layoutEntries[" << i << "].buffer.type = BufferBindingType::" << bufferBinding.type << ";";
					}
				}, binding.details);
			}));
		}
		else if (expr == "bindGroupEntries") {
			static constexpr const char* nl = "\n\t";
			TRY(visitBindings([&](unsigned i, const BindingInfo& binding) {
				if (i > 0) out << nl << nl;
				out << "entries[" << i << "].binding = " << binding.index << ";" << nl;
				std::visit(overloaded{
					[&](const BufferBindingInfo&) {
						out << "entries[" << i << "].buffer = " << binding.name << ";" << nl;
						out << "entries[" << i << "].size = " << binding.name << ".getSize();";
					}
				}, binding.details);
			}));
		}
		else if (expr == "uniformStructDefinition") {
			static constexpr const char* nl = "\n\t";
			out << "struct Uniforms {" << nl;
			out << "\t// TODO" << nl;
			out << "};";
		}
		else {
			return Error{ "Invalid template expression: " + expr };
		}
		return {};
	};

	Result<Void,Error> resetIterator(const std::string& iterator_name) {
		if (iterator_name == "entryPoints") {
			m_currentEntryPoint = 0;
		}
		else if (iterator_name == "entryPointCount == 1") {
			// Nothing to reset in theory, because this is in effect a "if"
			// that executes the bloc only when there is a single entry point
			// in the kernel. Nonetheless, we reset the entry point index
			// so that we may use {{entryPoint}} and other expressions that
			// rely on the current entry point index.
			m_currentEntryPoint = 0;
		}
		else if (iterator_name == "hasUniforms") {
			// Nothing to step, this is in effect a "if".
		}
		else {
			return Error{ "Invalid iterator name: " + iterator_name };
		}
		return {};
	}

	Result<Void, Error> stepIterator(const std::string& iterator_name) {
		if (iterator_name == "entryPoints") {
			m_currentEntryPoint += 1;
		}
		else if (iterator_name == "entryPointCount == 1") {
			// Nothing to step, this is in effect a "if".
		}
		else if (iterator_name == "hasUniforms") {
			// Nothing to step, this is in effect a "if".
		}
		else {
			return Error{ "Invalid iterator name: " + iterator_name };
		}
		return {};
	}

	Result<bool, Error> iteratorEnded(const std::string& iterator_name) const {
		if (iterator_name == "entryPoints") {
			size_t entryPointCount = size_t(m_layout->getEntryPointCount());
			return m_currentEntryPoint >= entryPointCount;
		}
		else if (iterator_name == "entryPointCount == 1") {
			size_t entryPointCount = size_t(m_layout->getEntryPointCount());
			return entryPointCount != 1; // 'iteratorEnded' is the inverse of the if condition
		}
		else if (iterator_name == "hasUniforms") {
			return !m_layoutInfo.uniforms.has_value(); // 'iteratorEnded' is the inverse of the if condition
		}
		else {
			return Error{ "Invalid iterator name: " + iterator_name };
		}
	}

private:
	Result<Void, Error> buildLayoutInfo() {
		unsigned parameterCount = m_layout->getParameterCount();
		for (unsigned i = 0; i < parameterCount; ++i) {
			VariableLayoutReflection* parameter = m_layout->getParameterByIndex(i);
			ParameterCategory category = parameter->getCategory();
			TypeLayoutReflection* typeLayout = parameter->getTypeLayout();
			TypeReflection::Kind kind = typeLayout->getKind();

			TRY_ASSERT(
				parameter->getBindingSpace() == 0,
				"Use of more than one bind group is not supported."
			);

			switch (category) {

			case ParameterCategory::DescriptorTableSlot: {
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

				BindingInfo binding;
				binding.index = parameter->getBindingIndex();
				binding.name = parameter->getName();
				BufferBindingInfo bufferBinding;
				SlangResourceAccess access = typeLayout->getResourceAccess();
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
				binding.details = bufferBinding;
				m_layoutInfo.bindings.push_back(binding);
				break;
			}

			case ParameterCategory::Uniform: {
				TRY_ASSERT(
					kind == TypeReflection::Kind::Struct || kind == TypeReflection::Kind::Scalar,
					"Only Struct and Scalar uniforms are supported, but found kind '" << enum_name(kind) << "'"
				);

				size_t byteOffset = parameter->getBindingIndex();
				size_t byteSize = typeLayout->getSize((SlangParameterCategory)category);

				if (!m_layoutInfo.uniforms.has_value()) {
					m_layoutInfo.uniforms = UniformInfo{};
				}
				auto& minBindingSize = m_layoutInfo.uniforms->minBindingSize;
				minBindingSize = std::max(minBindingSize, byteOffset + byteSize);

				break;
			}

			default:
				return Error{ "Parameter category '" + std::string(enum_name(category)) + "' is not supported" };
			}
		}

		// If there are global parameters, add them as a first binding:
		if (m_layoutInfo.uniforms.has_value()) {
			BindingInfo binding;
			binding.index = 0;
			binding.name = "uniforms";
			BufferBindingInfo uniformBufferBinding;
			uniformBufferBinding.minBindingSize = m_layoutInfo.uniforms->minBindingSize;
			uniformBufferBinding.type = "Uniform";
			binding.details = uniformBufferBinding;
			m_layoutInfo.bindings.push_front(binding);
		}

		return {};
	}

	/**
	 * An internal utility function that visits all the bindings and provides to
	 * the visitor the reflection information that we actually need.
	 */
	Result<Void, Error> visitBindings(
		const std::function<void(unsigned i, const BindingInfo& info)>& visitor
	) {
		TRY(check());
		unsigned i = 0;
		for (const auto& binding : m_layoutInfo.bindings) {
			visitor(i, binding);
			++i;
		}
		return {};
	}

private:
	const std::string m_name;
	slang::ProgramLayout* m_layout;
	const std::string m_wgslSource;

	// Information extracted from m_layout in a form better suited for our generator
	LayoutInfo m_layoutInfo;
	Result<Void, Error> m_initError;

	// Iterators
	size_t m_currentEntryPoint;
};

Result<Void, Error> generateCppBinding(
	const Slang::ComPtr<IComponentType>& program,
	const std::string& name,
	[[maybe_unused]] const std::vector<std::string>& entryPoints,
	const std::filesystem::path& inputTemplate,
	const std::string& wgslSource,
	const std::filesystem::path& outputHpp,
	const std::filesystem::path& outputCpp
) {
	LOG(INFO) << "Getting reflection information...";
	slang::ProgramLayout* layout = program->getLayout();
	
	LOG(INFO) << "Loading binding template from " << inputTemplate << "...";
	std::string tpl;
	TRY_ASSIGN(tpl, loadTextFile(inputTemplate));

	BindingGenerator generator(name, layout, wgslSource);
	TRY(generator.check());

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

Result<Void, Error> generateDepfile(
	const std::vector<std::string>& dependencyFiles,
	const std::filesystem::path& outputDepfile,
	const std::filesystem::path& outputHpp,
	const std::filesystem::path& outputCpp
) {
	LOG(INFO) << "Generating dependency file into " << outputDepfile << "...";
	std::ostringstream out;
	for (const auto& generated : { outputHpp, outputCpp }) {
		out << generated.string() << ":";
		for (const auto& dep : dependencyFiles) {
			out << " \\\n\t" << dep;
		}
		out << "\n";
	}
	TRY(saveTextFile(outputDepfile, out.str()));
	return {};
}

Result<Void, Error> run(const Arguments& args) {

	SessionInfo sessionInfo;
	TRY_ASSIGN(sessionInfo, createSlangSession(args.includeDirectories));

	ModuleInfo moduleInfo;
	TRY_ASSIGN(moduleInfo, loadSlangModule(
		sessionInfo.session,
		args.name,
		args.inputSlang,
		args.entryPoints
	));

	std::string wgslSource;
	TRY_ASSIGN(wgslSource, compileToWgsl(
		moduleInfo.program,
		args.inputSlang
	));

	if (!args.outputWgsl.empty()) {
		LOG(INFO) << "Writing generated WGSL source into '" << args.outputWgsl << "'...";
		TRY(saveTextFile(args.outputWgsl, wgslSource));
	}

	if (!args.outputHpp.empty()) {
		if (args.outputCpp.empty()) {
			return Error{ "Option --output-cpp must be non-empty when --output-hpp is non-empty."};
		}
		if (args.inputTemplate.empty()) {
			return Error{ "Option --input-template must be non-empty when --output-hpp is non-empty." };
		}
		
		TRY(generateCppBinding(
			moduleInfo.program,
			args.name,
			args.entryPoints,
			args.inputTemplate,
			wgslSource,
			args.outputHpp,
			args.outputCpp
		));
	}

	if (!args.outputDepfile.empty()) {
		TRY(generateDepfile(
			moduleInfo.dependencyFiles,
			args.outputDepfile,
			args.outputHpp,
			args.outputCpp
		));
	}

	return {};
}
