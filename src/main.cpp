// NB: This WEBGPU_CPP_IMPLEMENTATION must be defined in **exactly one** source
// file, and before including webgpu C++ header (see https://github.com/eliemichel/WebGPU-Cpp)
#define WEBGPU_CPP_IMPLEMENTATION

#include <webgpu/webgpu-raii.hpp>

using namespace wgpu;

Device createDevice();

int main(int, char**) {
	raii::Device device = createDevice();

	

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
