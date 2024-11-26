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

// Mirror of what is in the Slang shader
struct MyUniforms {
	float offset;
	float scale;
	uint32_t _pad[2];
};
static_assert(sizeof(MyUniforms) % 16 == 0);
struct ExtraUniforms {
	uint32_t indexOffset;
	uint32_t _pad[3];
};
static_assert(sizeof(ExtraUniforms) % 16 == 0);

/**
 * TODO: This struct and its substructs should ultimately be auto-generated as
 * well, and available through BufferScalarMathKernel::Uniforms
 */
struct BufferScalarMathUniforms {
	MyUniforms uniforms;
	ExtraUniforms extraUniforms;
};
static_assert(sizeof(BufferScalarMathUniforms) % 16 == 0);

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
	bufferDesc.size = sizeof(BufferScalarMathUniforms);
	bufferDesc.label = StringView("uniforms");
	bufferDesc.usage = BufferUsage::Uniform | BufferUsage::CopyDst;
	raii::Buffer uniforms = device->createBuffer(bufferDesc);

	bufferDesc.size = 16 * sizeof(float);
	bufferDesc.label = StringView("buffer");
	bufferDesc.usage = BufferUsage::Storage | BufferUsage::CopyDst | BufferUsage::CopySrc;
	raii::Buffer buffer = device->createBuffer(bufferDesc);

	bufferDesc.label = StringView("map");
	bufferDesc.usage = BufferUsage::MapRead | BufferUsage::CopyDst;
	raii::Buffer mapBuffer = device->createBuffer(bufferDesc);

	// 4. Fill in input buffers
	// Nothing specific to Slang here
	BufferScalarMathUniforms uniformData;
	uniformData.uniforms.offset = 3.14f;
	uniformData.uniforms.scale = 0.5f;
	uniformData.extraUniforms.indexOffset = 0;
	queue->writeBuffer(*uniforms, 0, &uniformData, sizeof(BufferScalarMathUniforms));
	std::vector<float> data0(16);
	for (int i = 0; i < 16; ++i) {
		data0[i] = 2.36f - 0.87f * i;
	}
	queue->writeBuffer(*buffer, 0, data0.data(), bufferDesc.size);

	// 5. Build bind group
	// Each generated kernel provides a 'createBindGroup' whose argument number
	// and names directly reflects the resources declared in the Slang shader.
	raii::BindGroup bindGroup = kernel.createBindGroup(*uniforms, *buffer);

	// 6. Dispatch kernel multiple times with various uniforms
	uniformData.uniforms.offset = 3.14f;
	queue->writeBuffer(*uniforms, 0, &uniformData, sizeof(BufferScalarMathUniforms));
	kernel.dispatchAdd(ThreadCount{ 16 }, *bindGroup);

	uniformData.uniforms.scale = 0.5f;
	uniformData.uniforms.offset = 0.04f;
	queue->writeBuffer(*uniforms, 0, &uniformData, sizeof(BufferScalarMathUniforms));
	kernel.dispatchMultiplyAndAdd(ThreadCount{ 16 }, *bindGroup);

	// 7. Copy result to map buffer
	raii::CommandEncoder encoder = device->createCommandEncoder();
	encoder->copyBufferToBuffer(*buffer, 0, *mapBuffer, 0, buffer->getSize());
	raii::CommandBuffer commands = encoder->finish();
	queue->submit(*commands);

	// 8. Read back result
	// Nothing specific to Slang here
	bool done = false;
	std::vector<float> resultData(16);
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

	// 9. Check result
	// Nothing specific to Slang here
	LOG(INFO) << "Result data:";
	for (int i = 0; i < 16; ++i) {
		LOG(INFO) << "(" << data0[i] << " + 3.14) * 0.5 + 0.04 = " << resultData[i];
		TRY_ASSERT(isClose((data0[i] + 3.14f) * 0.5f + 0.04f, resultData[i]), "Shader did not run correctly!");
	}

	return {};
}
