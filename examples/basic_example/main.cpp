// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

#include <slang-webgpu/common/result.h>

#include <webgpu/webgpu-raii.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace wgpu;

struct Kernel {
	std::string name;
	std::vector<raii::BindGroupLayout> bindGroupLayouts;
	raii::ComputePipeline pipeline;
};

struct KernelError {
	std::string message;
};

Device createDevice();
Result<Kernel, KernelError> createKernel(Device device, const std::string& name, const std::filesystem::path& path);

int main(int, char**) {
	// 1. Create GPU device
	raii::Device device = createDevice();
	raii::Queue queue = device->getQueue();

	// 2. Load kernel
	auto maybeKernel = createKernel(*device, "Hello World", R"(G:\SourceCode\SlangWebGPU\build\src\shaders\hello-world.wgsl)");
	if (isError(maybeKernel)) {
		std::cerr << std::get<KernelError>(maybeKernel).message << std::endl;
		return 1;
	}
	Kernel kernel = std::move(std::get<Kernel>(maybeKernel));

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
	std::vector<BindGroupEntry> entries(3, Default);
	entries[0].binding = 0;
	entries[0].buffer = *buffer0;
	entries[0].size = buffer0->getSize();

	entries[1].binding = 1;
	entries[1].buffer = *buffer1;
	entries[1].size = buffer1->getSize();

	entries[2].binding = 2;
	entries[2].buffer = *result;
	entries[2].size = result->getSize();

	BindGroupDescriptor bindGroupDesc = Default;
	bindGroupDesc.label = StringView("Bind group");
	bindGroupDesc.layout = *kernel.bindGroupLayouts[0];
	bindGroupDesc.entryCount = entries.size();
	bindGroupDesc.entries = entries.data();
	raii::BindGroup bindGroup = device->createBindGroup(bindGroupDesc);

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
		device->tick();
	}

	std::cout << "Result data:" << std::endl;
	for (int i = 0; i < 10; ++i) {
		std::cout << resultData[i] << std::endl;
	}

	return 0;
}

Device createDevice() {
	raii::Instance instance = createInstance();

	RequestAdapterOptions options = Default;
	raii::Adapter adapter = instance->requestAdapter(options);

	DeviceDescriptor descriptor = Default;
	descriptor.uncapturedErrorCallbackInfo2.callback = [](
		[[maybe_unused]] WGPUDevice const* device,
		WGPUErrorType type,
		WGPUStringView message,
		[[maybe_unused]] void* userdata1,
		[[maybe_unused]] void* userdata2
	) {
		std::cerr << "[WebGPU] Uncaptured error: ";
		if (message.data) std::cerr << StringView(message) << " (type: " << type << ")";
		else std::cerr << "(type: " << type << ")";
		std::cerr << std::endl;
	};
	descriptor.deviceLostCallbackInfo2.callback = [](
		[[maybe_unused]] WGPUDevice const* device,
		WGPUDeviceLostReason reason,
		WGPUStringView message,
		[[maybe_unused]] void* userdata1,
		[[maybe_unused]] void* userdata2
	) {
		if (reason == DeviceLostReason::InstanceDropped) return;
		std::cerr << "[WebGPU] Device lost: ";
		if (message.data) std::cerr << StringView(message) << " (reason: " << reason << ")";
		else std::cerr << "(reason: " << reason << ")";
		std::cerr << std::endl;
	};
	Device device = adapter->requestDevice(descriptor);

	AdapterInfo info;
	device.getAdapterInfo(&info);
	std::cout << "Using device: " << StringView(info.device) << " (vendor: " << StringView(info.vendor) << ", architecture: " << StringView(info.architecture) << ")" << std::endl;
	return device;
}

Result<Kernel, KernelError> createKernel(Device device, const std::string& name, const std::filesystem::path& path) {
	Kernel kernel;
	kernel.name = name;

	// 1. Load WGSL source code
	std::ifstream file;
	file.open(path);
	if (!file.is_open()) {
		return KernelError{ "Error while creating kernel '" + name + "': could not open file '" + path.string() + "'" };
	}
	std::stringstream ss;
	ss << file.rdbuf();
	std::string wgslSource = ss.str();

	// 2. Create shader module
	ShaderSourceWGSL wgslDesc = Default;
	wgslDesc.code = StringView(wgslSource);
	ShaderModuleDescriptor shaderDesc = Default;
	shaderDesc.nextInChain = &wgslDesc.chain;
	shaderDesc.label = StringView(kernel.name);
	raii::ShaderModule shaderModule = device.createShaderModule(shaderDesc);
	if (!shaderModule) {
		return KernelError{ "Error while creating kernel '" + name + "': could not compile shader from '" + path.string() + "'" };
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
