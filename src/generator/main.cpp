#include "slang-result-utils.h" // for TRY_SLANG

#include <slang-webgpu/common/result.h>
#include <slang-webgpu/common/logger.h>
#include <slang-webgpu/common/io.h>

#include <slang.h>
#include <slang-com-ptr.h>

#include <CLI11.hpp>

#include <filesystem>

using namespace slang;

/**
 * Command line arguments
 */
struct Arguments {
	std::string name;
	std::filesystem::path inputSlang;
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
	app.add_option("-w,--output-wgsl", args.outputWgsl, "Path to the output WGSL shader source");
	app.add_option("-d,--output-hpp", args.outputHpp, "Path to the output C++ header file that define kernels for each entry point");
	app.add_option("-c,--output-cpp", args.outputCpp, "Path to the output C++ source file that implements the header file");
	app.add_option("-e,--entrypoint", args.entryPoints, "Entry points to generate kernel for")
		->required()
		->delimiter(',');
	app.add_option("-I,--include-directories", args.includeDirectories, "Directories where to look for includes in slang shader")
		->delimiter(',');

	CLI11_PARSE(app, argc, argv);

	auto maybeError = run(args);
	if (isError(maybeError)) {
		LOG(ERROR) << std::get<Error>(maybeError).message;
		return 1;
	}

	return 0;
}

Result<Void, Error> run(const Arguments& args) {

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

	std::vector<const char*> includeDirectoriesData(args.includeDirectories.size());
	std::transform(
		args.includeDirectories.cbegin(),
		args.includeDirectories.cend(),
		includeDirectoriesData.begin(),
		[](const std::string& path) { return path.c_str(); }
	);
	sessionDesc.searchPaths = includeDirectoriesData.data();
	sessionDesc.searchPathCount = includeDirectoriesData.size();

	/* ... fill in `sessionDesc` ... */
	Slang::ComPtr<ISession> session;
	TRY_SLANG(globalSession->createSession(sessionDesc, session.writeRef()));

	LOG(INFO) << "Loading file '" << args.inputSlang << "'...";
	std::string source;
	TRY_ASSIGN(source, loadTextFile(args.inputSlang));

	LOG(INFO) << "Loading Slang module...";
	Slang::ComPtr<IBlob> diagnostics;
	IModule* module = session->loadModuleFromSourceString(
		args.name.c_str(),
		args.inputSlang.string().c_str(),
		source.c_str(),
		diagnostics.writeRef()
	);
	if (diagnostics) {
		std::string message = (const char*)diagnostics->getBufferPointer();
		return Error{ "Could not load slang module from file '" + args.inputSlang.string() + "': " + message};
	}

	for (const std::string& entryPointName : args.entryPoints) {
		LOG(INFO) << "Processing entry point '" << entryPointName << "'...";
		Slang::ComPtr<IEntryPoint> entryPoint;
		TRY_SLANG(module->findEntryPointByName(entryPointName.c_str(), entryPoint.writeRef()));

		IComponentType* components[] = { module, entryPoint };
		Slang::ComPtr<IComponentType> program;
		TRY_SLANG(session->createCompositeComponentType(components, 2, program.writeRef()));

		LOG(INFO) << "Reflection for entry point '" << entryPointName << "'...";
		slang::ProgramLayout* layout = program->getLayout();
		LOG(INFO) << "layout: " << layout;

		LOG(INFO) << "Compilation for entry point '" << entryPointName << "'...";
		Slang::ComPtr<IComponentType> linkedProgram;
		Slang::ComPtr<ISlangBlob> linkDiagnostics;
		program->link(linkedProgram.writeRef(), linkDiagnostics.writeRef());
		if (linkDiagnostics) {
			std::string message = (const char*)linkDiagnostics->getBufferPointer();
			return Error{ "Could not link slang module from file '" + args.inputSlang.string() + "' for entry point '" + entryPointName + "': " + message };
		}

		int entryPointIndex = 0; // only one entry point
		int targetIndex = 0; // only one target
		Slang::ComPtr<IBlob> kernelBlob;
		TRY_SLANG(linkedProgram->getEntryPointCode(
			entryPointIndex,
			targetIndex,
			kernelBlob.writeRef(),
			diagnostics.writeRef()
		));

		std::string wgslSource = (const char*)kernelBlob->getBufferPointer();
		LOG(DEBUG) << "WGSL Source:\n" << wgslSource;
	}

	return {};
}
