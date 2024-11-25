// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

#include <slang-webgpu/common/result.h>
#include <slang-webgpu/common/logger.h>
#include <slang-webgpu/common/io.h>

#include <slang-webgpu/examples/webgpu-utils.h> // provides createDevice()

// NB: raii::Foo is the equivalent of Foo except its release()/addRef() methods
// are automatically called
#include <webgpu/webgpu-raii.hpp>

#include <filesystem>
#include <cstring> // for memcpy

using namespace wgpu;

/**
 * A basic class that contains everything needed to dispatch a compute job.
 */
struct Kernel {
	std::string name;
	std::vector<raii::BindGroupLayout> bindGroupLayouts;
	raii::ComputePipeline pipeline;
};

/**
 * Main entry point
 */
Result<Void, Error> run();

/**
 * Load kernel from a WGSL file
 */
Result<Kernel, Error> createKernel(
	Device device,
	const std::string& name,
	const std::filesystem::path& path
);

/**
 * Create a bind group that fits the kernel's layout
 * The signature of this function matches the kernel's inputs.
 */
BindGroup createKernelBindGroup(
	Device device,
	const Kernel& kernel,
	Buffer buffer0,
	Buffer buffer1,
	Buffer result
);

int main(int, char**) {
	auto maybeError = run();
	if (isError(maybeError)) {
		LOG(ERROR) << std::get<Error>(maybeError).message;
		return 1;
	}
	return 0;
}

static bool isClose(float a, float b, float eps = 1e-6) {
	return std::abs(b - a) < eps;
}

Result<Void, Error> run() {
	// 1. Create GPU device
	raii::Device device = createDevice();
	raii::Queue queue = device->getQueue();

	// 2. Load kernel
	// When not using automated code generation, we manually write this 'createKernel' function
	Kernel kernel;
	TRY_ASSIGN(kernel, createKernel(*device, "Add buffers", SHADER_DIR "add-buffers.wgsl"));

	// 3. Create buffers
	BufferDescriptor bufferDesc = Default;
	bufferDesc.size = 10 * sizeof(float);
	bufferDesc.label = StringView("buffer0");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopyDst;
	raii::Buffer buffer0 = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("buffer1");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopyDst;
	raii::Buffer buffer1 = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("result");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopySrc;
	raii::Buffer result = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("map");
	bufferDesc.usage = BufferUsage::MapRead | BufferUsage::CopyDst;
	raii::Buffer mapBuffer = device->createBuffer(bufferDesc);

	// 4. Fill in input buffers
	std::vector<float> data0(10);
	std::vector<float> data1(10);
	for (int i = 0; i < 10; ++i) {
		data0[i] = i * 1.06f;
		data1[i] = 2.36f - 0.87f * i;
	}
	queue->writeBuffer(*buffer0, 0, data0.data(), bufferDesc.size);
	queue->writeBuffer(*buffer1, 0, data1.data(), bufferDesc.size);

	// 5. Build bind group
	// When not using automated code generation, we manually write this 'createKernelBindGroup' function
	raii::BindGroup bindGroup = createKernelBindGroup(*device, kernel, *buffer0, *buffer1, *result);

	// 6. Dispatch kernel
	ComputePassDescriptor computePassDesc = Default;
	computePassDesc.label = StringView(kernel.name);

	raii::CommandEncoder encoder = device->createCommandEncoder();
	raii::ComputePassEncoder computePass = encoder->beginComputePass(computePassDesc);
	computePass->setPipeline(*kernel.pipeline);
	computePass->setBindGroup(0, *bindGroup, 0, nullptr);
	computePass->dispatchWorkgroups(10, 1, 1);
	computePass->end();
	encoder->copyBufferToBuffer(*result, 0, *mapBuffer, 0, result->getSize());
	raii::CommandBuffer commands = encoder->finish();
	queue->submit(*commands);

	// 7. Read back result
	bool done = false;
	std::vector<float> resultData(10);
	auto h = mapBuffer->mapAsync(MapMode::Read, 0, mapBuffer->getSize(), [&](BufferMapAsyncStatus status) {
		done = true;
		if (status == BufferMapAsyncStatus::Success) {
			memcpy(resultData.data(), mapBuffer->getConstMappedRange(0, mapBuffer->getSize()), mapBuffer->getSize());
		}
		mapBuffer->unmap();
	});

	while (!done) {
		pollDeviceEvents(*device);
	}

	LOG(INFO) << "Result data:";
	for (int i = 0; i < 10; ++i) {
		LOG(INFO) << data0[i] << " + " << data1[i] << " = " << resultData[i];
		TRY_ASSERT(isClose(data0[i] + data1[i], resultData[i]), "Shader did not run correctly!");
	}

	return {};
}

Result<Kernel, Error> createKernel(
	Device device,
	const std::string& name,
	const std::filesystem::path& path
) {
	Kernel kernel;
	kernel.name = name;

	// 1. Load WGSL source code
	std::string wgslSource;
	TRY_ASSIGN(wgslSource, loadTextFile(path));

	// 2. Create shader module
	ShaderSourceWGSL wgslDesc = Default;
	wgslDesc.code = StringView(wgslSource);
	ShaderModuleDescriptor shaderDesc = Default;
	shaderDesc.nextInChain = &wgslDesc.chain;
	shaderDesc.label = StringView(kernel.name);
	raii::ShaderModule shaderModule = device.createShaderModule(shaderDesc);
	if (!shaderModule) {
		return Error{ "Error while creating kernel '" + name + "': could not compile shader from '" + path.string() + "'" };
	}

	// 3. Create pipeline layout
	std::vector<BindGroupLayoutEntry> layoutEntries(3, Default);
	layoutEntries[0].binding = 0;
	layoutEntries[0].visibility = ShaderStage::Compute;
	layoutEntries[0].buffer.type = BufferBindingType::ReadOnlyStorage;

	layoutEntries[1].binding = 1;
	layoutEntries[1].visibility = ShaderStage::Compute;
	layoutEntries[1].buffer.type = BufferBindingType::ReadOnlyStorage;

	layoutEntries[2].binding = 2;
	layoutEntries[2].visibility = ShaderStage::Compute;
	layoutEntries[2].buffer.type = BufferBindingType::Storage;

	std::vector<raii::BindGroupLayout> bindGroupLayouts(1);
	BindGroupLayoutDescriptor bindGroupLayoutDesc = Default;
	bindGroupLayoutDesc.entryCount = layoutEntries.size();
	bindGroupLayoutDesc.entries = layoutEntries.data();
	bindGroupLayouts[0] = device.createBindGroupLayout(bindGroupLayoutDesc);

	PipelineLayoutDescriptor layoutDesc = Default;
	layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)bindGroupLayouts.data();
	raii::PipelineLayout layout = device.createPipelineLayout(layoutDesc);

	// 4. Create compute pipeline (TODO)
	ComputePipelineDescriptor pipelineDesc = Default;
	pipelineDesc.label = StringView(kernel.name);
	pipelineDesc.compute.module = *shaderModule;
	pipelineDesc.compute.entryPoint = StringView("computeMain");
	pipelineDesc.layout = *layout;
	raii::ComputePipeline pipeline = device.createComputePipeline(pipelineDesc);

	kernel.pipeline = pipeline;
	kernel.bindGroupLayouts = bindGroupLayouts;
	return kernel;
}

BindGroup createKernelBindGroup(
	Device device,
	const Kernel& kernel,
	Buffer buffer0,
	Buffer buffer1,
	Buffer result
) {
	std::vector<BindGroupEntry> entries(3, Default);
	entries[0].binding = 0;
	entries[0].buffer = buffer0;
	entries[0].size = buffer0.getSize();

	entries[1].binding = 1;
	entries[1].buffer = buffer1;
	entries[1].size = buffer1.getSize();

	entries[2].binding = 2;
	entries[2].buffer = result;
	entries[2].size = result.getSize();

	BindGroupDescriptor bindGroupDesc = Default;
	bindGroupDesc.label = StringView("Bind group");
	bindGroupDesc.layout = *kernel.bindGroupLayouts[0];
	bindGroupDesc.entryCount = entries.size();
	bindGroupDesc.entries = entries.data();

	return device.createBindGroup(bindGroupDesc);
}
