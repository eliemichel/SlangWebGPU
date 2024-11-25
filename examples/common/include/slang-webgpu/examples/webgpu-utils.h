#pragma once

#include <webgpu/webgpu.hpp>

/**
 * Create a WebGPU device.
 *
 * NB: On emscripten, this requires ASYNCIFY so that the API is simpler.
 * If you do not want to use ASYNCIFY, you may replace it with other mechanism
 * to get a device.
 */
wgpu::Device createDevice();

/**
 * Let the device trigger pending callbacks if they are ready.
 * NB: On emscripten, this requires ASYNCIFY. If you do not wish to use
 * ASYNCIFY, use callbacks instead of explicitly polling the device
 */
void pollDeviceEvents(wgpu::Device device);
