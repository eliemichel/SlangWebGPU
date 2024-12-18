#include <slang-webgpu/examples/webgpu-utils.h>

#include <slang-webgpu/common/logger.h>

// NB: raii::Foo is the equivalent of Foo except its release()/addRef() methods
// are automatically called
#include <webgpu/webgpu-raii.hpp>

#ifdef __EMSCRIPTEN__
#  include <emscripten/html5.h>
#endif

using namespace wgpu;

#ifdef __EMSCRIPTEN__

Device createDevice() {
	raii::Instance instance = createInstance();

	RequestAdapterOptions options = Default;
	raii::Adapter adapter = instance->requestAdapter(options);

	DeviceDescriptor descriptor = Default;
	descriptor.deviceLostCallback = [](
		WGPUDeviceLostReason reason,
		const char* message,
		[[maybe_unused]] void* userdata
	) {
		if (message)
			LOG(ERROR) << "[WebGPU] Device lost: " << StringView(message) << " (reason: " << reason << ")";
		else
			LOG(ERROR) << "[WebGPU] Device lost: (reason: " << reason << ")";
	};
	Device device = adapter->requestDevice(descriptor);

	wgpuDeviceSetUncapturedErrorCallback(
		device,
		[](
			WGPUErrorType type,
			const char* message,
			[[maybe_unused]] void* userdata
		) {
			if (message)
				LOG(ERROR) << "[WebGPU] Uncaptured error: " << StringView(message) << " (type: " << type << ")";
			else
				LOG(ERROR) << "[WebGPU] Uncaptured error: (reason: " << type << ")";
		},
		nullptr
	);

	AdapterInfo info;
	wgpuAdapterGetInfo(*adapter, &info);
	LOG(INFO)
		<< "Using device: " << StringView(info.device)
		<< " (vendor: " << StringView(info.vendor)
		<< ", architecture: " << StringView(info.architecture) << ")";
	wgpuAdapterInfoFreeMembers(info);
	return device;
}

#else // __EMSCRIPTEN__

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
	info.freeMembers();
	return device;
}

#endif // __EMSCRIPTEN__

void pollDeviceEvents([[maybe_unused]] Device device) {
#ifdef __EMSCRIPTEN__
	emscripten_sleep(50);
#else // __EMSCRIPTEN__
	device.tick();
#endif // __EMSCRIPTEN__
}
