// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

// Header generated from shaders/simple_autodiff.slang (see config in CMakeLists.txt)
#include "generated/BufferScalarMathKernel.h"

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
 * Main entry point
 */
Result<Void, Error> run();

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
	// Nothing specific to Slang here
	raii::Device device = createDevice();
	raii::Queue queue = device->getQueue();

	// 2. Load kernel
	// This simply consists in instancing the generated HelloWorldKernel class.
	// NB: In a real scenario, this may be wrapped into a unique_ptr
	generated::BufferScalarMathKernel kernel(*device);
	TRY_ASSERT(kernel, "Kernel could not load!");

	// 3. Create buffers
	// Nothing specific to Slang here
	BufferDescriptor bufferDesc = Default;
	bufferDesc.size = 10 * sizeof(float);
	bufferDesc.label = StringView("buffer0");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopyDst;
	raii::Buffer buffer0 = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("result");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopySrc;
	raii::Buffer result = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("map");
	bufferDesc.usage = BufferUsage::MapRead | BufferUsage::CopyDst;
	raii::Buffer mapBuffer = device->createBuffer(bufferDesc);

	// 4. Fill in input buffers
	// Nothing specific to Slang here
	std::vector<float> data0(10);
	std::vector<float> data1(10);
	for (int i = 0; i < 10; ++i) {
		data0[i] = i * 1.06f;
		data1[i] = 2.36f - 0.87f * i;
	}
	queue->writeBuffer(*buffer0, 0, data0.data(), bufferDesc.size);

	// 5. Build bind group
	// Each generated kernel provides a 'createBindGroup' whose argument number
	// and names directly reflects the resources declared in the Slang shader.
	raii::BindGroup bindGroup = kernel.createBindGroup(*buffer0, *result);

	// 6. Dispatch kernel and copy result to map buffer
	// Each generated kernel provides a 'dispatch' method which can be called
	// for a specific number of workgroups or threads (the number of workgroup
	// is then automatically derived from the workgroup size).
	// NB: The 'dispatch' method may receive an existing encoder or compute pass
	// as first argument, otherwise if nothing is provided it creates its own
	// encoder and submits it. Here we create an encoder so that we directly issue
	// the buffer copy in the same command buffer.
	raii::CommandEncoder encoder = device->createCommandEncoder();
	kernel.dispatchAdd(*encoder, ThreadCount{ 10 }, *bindGroup);
	encoder->copyBufferToBuffer(*result, 0, *mapBuffer, 0, result->getSize());
	raii::CommandBuffer commands = encoder->finish();
	queue->submit(*commands);

	// 7. Read back result
	// Nothing specific to Slang here
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
		LOG(INFO) << data0[i] << " + " << 3.14f << " = " << resultData[i];
		TRY_ASSERT(isClose(data0[i] + 3.14f, resultData[i]), "Shader did not run correctly!");
	}

	return {};
}
