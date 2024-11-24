// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

#include <slang-webgpu/common/result.h>
#include <slang-webgpu/common/logger.h>
#include <slang-webgpu/common/io.h>

// NB: raii::Foo is the equivalent of Foo except its release()/addRef() methods
// are automatically called
#include <webgpu/webgpu-raii.hpp>

#include <filesystem>

using namespace wgpu;

struct WorkgroupCount {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};
struct ThreadCount {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};
using DispatchSize = std::variant<WorkgroupCount, ThreadCount>;

/**
 * A basic class that contains everything needed to dispatch a compute job.
 */
class AddBuffersKernel {
public:
	AddBuffersKernel(Device device);

	BindGroup createBindGroup(
		Buffer buffer0,
		Buffer buffer1,
		Buffer result
	) const;

	void dispatch(
		DispatchSize dispatchSize,
		BindGroup bindGroup
	);

	void dispatch(
		CommandEncoder encoder,
		DispatchSize dispatchSize,
		BindGroup bindGroup
	);

	void dispatch(
		ComputePassEncoder computePass,
		DispatchSize dispatchSize,
		BindGroup bindGroup
	);

	operator bool() const { return m_valid; }

private:
	Result<Void, Error> initialize();

private:
	static constexpr const char* s_name = "Add Buffers";
	static constexpr const char* s_sourcePath = R"(G:\SourceCode\SlangWebGPU\build\examples\no_codegen\shaders\add-buffers.wgsl)";
	static constexpr ThreadCount s_workgroupSize = { 1, 1, 1 };

	Device m_device;
	bool m_valid = false;
	std::vector<raii::BindGroupLayout> m_bindGroupLayouts;
	raii::ComputePipeline m_pipeline;
};

// from https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

AddBuffersKernel::AddBuffersKernel(Device device)
	: m_device(device)
{
	auto maybeError = initialize();
	if (isError(maybeError)) {
		LOG(ERROR) << "Failed to initialize kernel '" << s_name << "': " + std::get<1>(maybeError).message;
		m_valid = false;
	}
	else {
		m_valid = true;
	}
}

Result<Void, Error> AddBuffersKernel::initialize() {
	// 1. Load WGSL source code
	std::string wgslSource;
	TRY_ASSIGN(wgslSource, loadTextFile(s_sourcePath));

	// 2. Create shader module
	ShaderSourceWGSL wgslDesc = Default;
	wgslDesc.code = StringView(wgslSource);
	ShaderModuleDescriptor shaderDesc = Default;
	shaderDesc.nextInChain = &wgslDesc.chain;
	shaderDesc.label = StringView(s_name);
	raii::ShaderModule shaderModule = m_device.createShaderModule(shaderDesc);
	if (!shaderModule) {
		return Error{ "Could not compile shader from '" + std::string(s_sourcePath) + "'" };
	}

	// 3. Create pipeline layout (automatically generated)
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

	// TODO: handle more than 1 bind group
	std::vector<raii::BindGroupLayout> bindGroupLayouts(1);
	BindGroupLayoutDescriptor bindGroupLayoutDesc = Default;
	bindGroupLayoutDesc.entryCount = layoutEntries.size();
	bindGroupLayoutDesc.entries = layoutEntries.data();
	bindGroupLayouts[0] = m_device.createBindGroupLayout(bindGroupLayoutDesc);

	PipelineLayoutDescriptor layoutDesc = Default;
	layoutDesc.bindGroupLayoutCount = bindGroupLayouts.size();
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)bindGroupLayouts.data();
	raii::PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);

	// 4. Create compute pipeline
	ComputePipelineDescriptor pipelineDesc = Default;
	pipelineDesc.label = StringView(s_name);
	pipelineDesc.compute.module = *shaderModule;
	pipelineDesc.compute.entryPoint = StringView("computeMain");
	pipelineDesc.layout = *layout;
	raii::ComputePipeline pipeline = m_device.createComputePipeline(pipelineDesc);

	m_pipeline = pipeline;
	m_bindGroupLayouts = bindGroupLayouts;
	return {};
}

BindGroup AddBuffersKernel::createBindGroup(
	Buffer buffer0,
	Buffer buffer1,
	Buffer result
) const {
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
	bindGroupDesc.layout = *m_bindGroupLayouts[0];
	bindGroupDesc.entryCount = entries.size();
	bindGroupDesc.entries = entries.data();

	return m_device.createBindGroup(bindGroupDesc);
}

static uint32_t divideAndCeil(uint32_t x, uint32_t y) {
	return (x + y - 1) / y;
}

void AddBuffersKernel::dispatch(
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	CommandEncoderDescriptor encoderDesc = Default;
	encoderDesc.label = StringView(s_name);

	raii::CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);
	dispatch(*encoder, dispatchSize, bindGroup);
	raii::CommandBuffer commands = encoder->finish();
	raii::Queue queue = m_device.getQueue();
	queue->submit(*commands);
}

void AddBuffersKernel::dispatch(
	CommandEncoder encoder,
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	ComputePassDescriptor computePassDesc = Default;
	computePassDesc.label = StringView(s_name);

	raii::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
	dispatch(*computePass, dispatchSize, bindGroup);
	computePass->end();
}

void AddBuffersKernel::dispatch(ComputePassEncoder computePass, DispatchSize dispatchSize, BindGroup bindGroup) {
	WorkgroupCount workgroupCount = std::visit(overloaded{
		[](WorkgroupCount count) { return count; },
		[](ThreadCount threadCount) { return WorkgroupCount{
			divideAndCeil(threadCount.x, s_workgroupSize.x),
			divideAndCeil(threadCount.y, s_workgroupSize.y),
			divideAndCeil(threadCount.z, s_workgroupSize.z)
		}; }
	}, dispatchSize);

	computePass.setPipeline(*m_pipeline);
	computePass.setBindGroup(0, bindGroup, 0, nullptr);
	computePass.dispatchWorkgroups(workgroupCount.x, workgroupCount.y, workgroupCount.z);
}

/**
 * Main entry point
 */
Result<Void, Error> run();

/**
 * Create a WebGPU device.
 */
Device createDevice();

int main(int, char**) {
	auto maybeError = run();
	if (isError(maybeError)) {
		LOG(ERROR) << std::get<Error>(maybeError).message;
		return 1;
	}
	return 0;
}

Result<Void, Error> run() {
	// 1. Create GPU device
	raii::Device device = createDevice();
	raii::Queue queue = device->getQueue();

	// 2. Load kernel
	AddBuffersKernel kernel(*device);

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
	raii::BindGroup bindGroup = kernel.createBindGroup(*buffer0, *buffer1, *result);

	// 6. Dispatch kernel and copy result to map buffer
	raii::CommandEncoder encoder = device->createCommandEncoder();
	kernel.dispatch(*encoder, WorkgroupCount{ 10 }, *bindGroup);
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

	LOG(INFO) << "Result data:";
	for (int i = 0; i < 10; ++i) {
		LOG(INFO) << data0[i] << " + " << data1[i] << " = " << resultData[i];
	}

	return {};
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
		if (message.data)
			LOG(ERROR) << "[WebGPU] Uncaptured error: " << StringView(message) << " (type: " << type << ")";
		else
			LOG(ERROR) << "[WebGPU] Uncaptured error: (reason: " << type << ")";
	};
	descriptor.deviceLostCallbackInfo2.callback = [](
		[[maybe_unused]] WGPUDevice const* device,
		WGPUDeviceLostReason reason,
		WGPUStringView message,
		[[maybe_unused]] void* userdata1,
		[[maybe_unused]] void* userdata2
	) {
		if (reason == DeviceLostReason::InstanceDropped) return;
		if (message.data)
			LOG(ERROR) << "[WebGPU] Device lost: " << StringView(message) << " (reason: " << reason << ")";
		else
			LOG(ERROR) << "[WebGPU] Device lost: (reason: " << reason << ")";
	};
	Device device = adapter->requestDevice(descriptor);

	AdapterInfo info;
	device.getAdapterInfo(&info);
	LOG(INFO)
		<< "Using device: " << StringView(info.device)
		<< " (vendor: " << StringView(info.vendor)
		<< ", architecture: " << StringView(info.architecture) << ")";
	return device;
}
