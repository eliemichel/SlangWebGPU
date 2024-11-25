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
