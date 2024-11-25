// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

// Header generated from shaders/simple_autodiff.slang (see config in CMakeLists.txt)
#include "generated/SimpleAutodiffKernel.h"

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

Result<Void, Error> run() {
	// 1. Create GPU device
	// Nothing specific to Slang here
	raii::Device device = createDevice();
	raii::Queue queue = device->getQueue();

	// 2. Load kernel
	// This simply consists in instancing the generated HelloWorldKernel class.
	// NB: In a real scenario, this may be wrapped into a unique_ptr
	generated::SimpleAutodiffKernel kernel(*device);
	TRY_ASSERT(kernel, "Kernel could not load!");

	// 3. Create buffers
	// Nothing specific to Slang here
	BufferDescriptor bufferDesc = Default;
	bufferDesc.size = 5 * sizeof(float);
	bufferDesc.label = StringView("output");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopySrc;
	raii::Buffer output = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("map");
	bufferDesc.usage = BufferUsage::MapRead | BufferUsage::CopyDst;
	raii::Buffer mapBuffer = device->createBuffer(bufferDesc);

	// 5. Build bind group
	// Each generated kernel provides a 'createBindGroup' whose argument number
	// and names directly reflects the resources declared in the Slang shader.
	raii::BindGroup bindGroup = kernel.createBindGroup(*output);

	// 6. Dispatch kernel and copy result to map buffer
	// Each generated kernel provides a 'dispatch' method which can be called
	// for a specific number of workgroups or threads (the number of workgroup
	// is then automatically derived from the workgroup size).
	// NB: The 'dispatch' method may receive an existing encoder or compute pass
	// as first argument, otherwise if nothing is provided it creates its own
	// encoder and submits it. Here we create an encoder so that we directly issue
	// the buffer copy in the same command buffer.
	raii::CommandEncoder encoder = device->createCommandEncoder();
	kernel.dispatch(*encoder, ThreadCount{ 10 }, *bindGroup);
	encoder->copyBufferToBuffer(*output, 0, *mapBuffer, 0, output->getSize());
	raii::CommandBuffer commands = encoder->finish();
	queue->submit(*commands);

	// 7. Read back result
	// Nothing specific to Slang here
	bool done = false;
	std::vector<float> outputData(10);
	auto h = mapBuffer->mapAsync(MapMode::Read, 0, mapBuffer->getSize(), [&](BufferMapAsyncStatus status) {
		done = true;
		if (status == BufferMapAsyncStatus::Success) {
			memcpy(outputData.data(), mapBuffer->getConstMappedRange(0, mapBuffer->getSize()), mapBuffer->getSize());
		}
		mapBuffer->unmap();
	});

	while (!done) {
		pollDeviceEvents(*device);
	}

	LOG(INFO) << "Result: " << outputData[0];
	LOG(INFO) << "dF wrt x: " << outputData[1];
	LOG(INFO) << "dF wrt x and y: " << outputData[2];
	LOG(INFO) << "dF wrt x computed using backward differentiation: " << outputData[3];
	LOG(INFO) << "dF wrt y computed using backward differentiation: " << outputData[4];

	TRY_ASSERT(outputData[0] == 13.0, "validation error");
	TRY_ASSERT(outputData[1] == 4.0, "validation error");
	TRY_ASSERT(outputData[2] == 10.0, "validation error");
	TRY_ASSERT(outputData[3] == 4.0, "validation error");
	TRY_ASSERT(outputData[4] == 6.0, "validation error");

	return {};
}
