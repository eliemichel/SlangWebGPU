// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

// Header generated from add-buffers.slang (see config in CMakeLists.txt)
#include "generated/HelloWorldKernel.h"

#include <slang-webgpu/common/result.h>
#include <slang-webgpu/common/logger.h>
#include <slang-webgpu/common/io.h>

// NB: raii::Foo is the equivalent of Foo except its release()/addRef() methods
// are automatically called
#include <webgpu/webgpu-raii.hpp>

#include <filesystem>

using namespace wgpu;

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
	generated::HelloWorldKernel kernel(*device);
	TRY_ASSERT(kernel, "Kernel could not load!");

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
