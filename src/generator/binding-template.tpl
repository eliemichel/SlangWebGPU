[[header]]
#pragma once

#include <slang-webgpu/common/result.h>
#include <slang-webgpu/common/KernelBase.h{{foo}}>

// NB: raii::Foo is the equivalent of Foo except its release()/addRef() methods
// are automatically called
#include <webgpu/webgpu-raii.hpp>

namespace generated {

/**
 * A basic class that contains everything needed to dispatch a compute job.
 */
class AddBuffersKernel {
public:
	AddBuffersKernel(Device device);

	wgpu::BindGroup createBindGroup(
		wgpu::Buffer buffer0,
		wgpu::Buffer buffer1,
		wgpu::Buffer result
	) const;

	void dispatch(
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	void dispatch(
		wgpu::CommandEncoder encoder,
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	void dispatch(
		wgpu::ComputePassEncoder computePass,
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	operator bool() const { return m_valid; }

private:
	Result<Void, Error> initialize();

private:
	static constexpr const char* s_name = "Add Buffers";
	static constexpr const char* s_sourcePath = R"(G:\SourceCode\SlangWebGPU\build\examples\no_codegen\shaders\add-buffers.wgsl)";
	static constexpr ThreadCount s_workgroupSize = { 1, 1, 1 };

	wgpu::Device m_device;
	bool m_valid = false;
	std::vector<wgpu::raii::BindGroupLayout> m_bindGroupLayouts;
	wgpu::raii::ComputePipeline m_pipeline;
};

} // namespace generated


[[implementation]]
#include "AddBuffersKernel.h"

#include <slang-webgpu/common/logger.h>
#include <slang-webgpu/common/io.h>

#include <variant>

using namespace wgpu;

// from https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

namespace generated {

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

} // namespace codegen
