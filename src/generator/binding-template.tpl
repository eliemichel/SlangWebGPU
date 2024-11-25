[[header]]
#pragma once

#include <slang-webgpu/common/kernel-utils.h>

// NB: raii::Foo is the equivalent of Foo except its release()/addRef() methods
// are automatically called
#include <webgpu/webgpu-raii.hpp>

#include <array>

namespace generated {

/**
 * A basic class that contains everything needed to dispatch a compute job.
 */
class {{kernelName}}Kernel {
public:
	{{kernelName}}Kernel(wgpu::Device device);

	/**
	 * Create a bind group to be used with the dispatch methods of this kernel.
	 * Arguments directly reflect the input resources declared in the original
	 * slang shader.
	 */
	wgpu::BindGroup createBindGroup(
		{{bindGroupMembers}}
	) const;

	{{foreach entryPoints}}
	/**
	 * Dispatch the kernel's entry point '{{entryPoint}}' on a given number of
	 * threads or workgroups. The bind group MUST have been created by this
	 * Kernel's createBindGroup method.
	 *
	 * This overload creates its own command encoder, compute pass, and submit
	 * all resulting commands to the device's queue.
	 */
	void dispatch{{EntryPoint}}(
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	/**
	 * Variant of dispatch{{EntryPoint}}() that uses an already existing command encoder.
	 * NB: This does not finish and submit the encoder.
	 */
	void dispatch{{EntryPoint}}(
		wgpu::CommandEncoder encoder,
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	/**
	 * Variant of dispatch{{EntryPoint}}() that uses an already existing compute pass.
	 * NB: This does not end the pass.
	 */
	void dispatch{{EntryPoint}}(
		wgpu::ComputePassEncoder computePass,
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);
	{{end}}

	{{if entryPointCount == 1}}
	/**
	 * Dispatch the kernel on a given number of threads or workgroups.
	 * The bind group MUST have been created by this Kernel's createBindGroup
	 * method.
	 *
	 * This overload creates its own command encoder, compute pass, and submit
	 * all resulting commands to the device's queue.
	 *
	 * NB: This function is only available if there is a single entry point in
	 * the kernel.
	 */
	void dispatch(
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	/**
	 * Variant of dispatch() that uses an already existing command encoder.
	 * NB: This does not finish and submit the encoder.
	 */
	void dispatch(
		wgpu::CommandEncoder encoder,
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);

	/**
	 * Variant of dispatch() that uses an already existing compute pass.
	 * NB: This does not end the pass.
	 */
	void dispatch(
		wgpu::ComputePassEncoder computePass,
		DispatchSize dispatchSize,
		wgpu::BindGroup bindGroup
	);
	{{end}}

	/**
	 * In case of trouble loading shader, the kernel might be invalid.
	 */
	operator bool() const { return m_valid; }

private:
	static constexpr const char* s_name = "{{kernelLabel}}";
	static constexpr std::array<ThreadCount,{{entryPointCount}}> s_workgroupSize = {
	{{foreach entryPoints}}
		ThreadCount{{workgroupSize}},
	{{end}}
	};
	static const char* s_wgslSource;

	wgpu::Device m_device;
	bool m_valid = false;
	std::array<wgpu::raii::BindGroupLayout,1> m_bindGroupLayouts;
	std::array<wgpu::raii::ComputePipeline,{{entryPointCount}}> m_pipelines;
};

} // namespace generated


[[implementation]]
#include "{{kernelName}}Kernel.h"

#include <slang-webgpu/common/variant-utils.h>

#include <variant>
#include <string>

using namespace wgpu;

namespace generated {

////////////////////////////////////////////
// Initialization

{{kernelName}}Kernel::{{kernelName}}Kernel(Device device)
	: m_device(device)
{
	// 1. Create shader module
	ShaderSourceWGSL wgslDesc = Default;
	wgslDesc.code = StringView(s_wgslSource);
	ShaderModuleDescriptor shaderDesc = Default;
	shaderDesc.nextInChain = &wgslDesc.chain;
	shaderDesc.label = StringView(s_name);
	raii::ShaderModule shaderModule = m_device.createShaderModule(shaderDesc);
	m_valid = shaderModule;

	// 2. Create pipeline layout (automatically generated)
	std::vector<BindGroupLayoutEntry> layoutEntries(3, Default);
	{{bindGroupLayoutEntries}}

	// TODO: handle more than 1 bind group
	std::vector<raii::BindGroupLayout> bindGroupLayouts(1);
	BindGroupLayoutDescriptor bindGroupLayoutDesc = Default;
	bindGroupLayoutDesc.entryCount = layoutEntries.size();
	bindGroupLayoutDesc.entries = layoutEntries.data();
	m_bindGroupLayouts[0] = m_device.createBindGroupLayout(bindGroupLayoutDesc);

	PipelineLayoutDescriptor layoutDesc = Default;
	layoutDesc.bindGroupLayoutCount = m_bindGroupLayouts.size();
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)m_bindGroupLayouts.data();
	raii::PipelineLayout layout = m_device.createPipelineLayout(layoutDesc);

	// 3. Create compute pipelines
	{{foreach entryPoints}}
	// 3.{{entryPointIndex}}. Entry point '{{entryPoint}}'
	{
		std::string label = std::string(s_name) + "::{{entryPoint}}";
		ComputePipelineDescriptor pipelineDesc = Default;
		pipelineDesc.label = StringView(label);
		pipelineDesc.compute.module = *shaderModule;
		pipelineDesc.compute.entryPoint = StringView("{{entryPoint}}");
		pipelineDesc.layout = *layout;
		m_pipelines[{{entryPointIndex}}] = m_device.createComputePipeline(pipelineDesc);
	}
	{{end}}
}

////////////////////////////////////////////
// Bind Group

BindGroup {{kernelName}}Kernel::createBindGroup(
	{{bindGroupMembersImpl}}
) const {
	std::vector<BindGroupEntry> entries(3, Default);
	{{bindGroupEntries}}

	BindGroupDescriptor bindGroupDesc = Default;
	bindGroupDesc.label = StringView("Bind group");
	bindGroupDesc.layout = *m_bindGroupLayouts[0];
	bindGroupDesc.entryCount = entries.size();
	bindGroupDesc.entries = entries.data();

	return m_device.createBindGroup(bindGroupDesc);
}

{{foreach entryPoints}}
////////////////////////////////////////////
// Entry point '{{entryPoint}}'

void {{kernelName}}Kernel::dispatch{{EntryPoint}}(
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	CommandEncoderDescriptor encoderDesc = Default;
	encoderDesc.label = StringView(s_name);

	raii::CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);
	dispatch{{EntryPoint}}(*encoder, dispatchSize, bindGroup);
	raii::CommandBuffer commands = encoder->finish();
	raii::Queue queue = m_device.getQueue();
	queue->submit(*commands);
}

void {{kernelName}}Kernel::dispatch{{EntryPoint}}(
	CommandEncoder encoder,
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	ComputePassDescriptor computePassDesc = Default;
	computePassDesc.label = StringView(s_name);

	raii::ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
	dispatch{{EntryPoint}}(*computePass, dispatchSize, bindGroup);
	computePass->end();
}

void {{kernelName}}Kernel::dispatch{{EntryPoint}}(
	ComputePassEncoder computePass,
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	WorkgroupCount workgroupCount = std::visit(overloaded{
		[](WorkgroupCount count) { return count; },
		[](ThreadCount threadCount) { return WorkgroupCount{
			divideAndCeil(threadCount.x, s_workgroupSize[{{entryPointIndex}}].x),
			divideAndCeil(threadCount.y, s_workgroupSize[{{entryPointIndex}}].y),
			divideAndCeil(threadCount.z, s_workgroupSize[{{entryPointIndex}}].z)
		}; }
	}, dispatchSize);

	computePass.setPipeline(*m_pipelines[{{entryPointIndex}}]);
	computePass.setBindGroup(0, bindGroup, 0, nullptr);
	computePass.dispatchWorkgroups(workgroupCount.x, workgroupCount.y, workgroupCount.z);
}
{{end}}

{{if entryPointCount == 1}}
////////////////////////////////////////////
// Commodity aliases enabled when there is a single entry point in the kernel

void {{kernelName}}Kernel::dispatch(
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	dispatch{{EntryPoint}}(dispatchSize, bindGroup);
}

void {{kernelName}}Kernel::dispatch(
	CommandEncoder encoder,
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	dispatch{{EntryPoint}}(encoder, dispatchSize, bindGroup);
}

void {{kernelName}}Kernel::dispatch(
	ComputePassEncoder computePass,
	DispatchSize dispatchSize,
	BindGroup bindGroup
) {
	dispatch{{EntryPoint}}(computePass, dispatchSize, bindGroup);
}
{{end}}

const char* {{kernelName}}Kernel::s_wgslSource = R"({{wgslSource}})";

} // namespace codegen
