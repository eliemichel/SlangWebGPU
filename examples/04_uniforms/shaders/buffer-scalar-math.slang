RWStructuredBuffer<float> buffer;
struct MyUniforms {
    float offset;
    float scale;
};
uniform MyUniforms uniforms;
struct ExtraUniforms {
    uint indexOffset;
};
uniform ExtraUniforms extraUniforms;

[shader("compute")]
[numthreads(8, 1, 1)]
void add(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (index < extraUniforms.indexOffset) return;
    buffer[index] = buffer[index - extraUniforms.indexOffset] + uniforms.offset;
}

[shader("compute")]
[numthreads(8, 1, 1)]
void sub(uint3 threadId: SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (index < extraUniforms.indexOffset) return;
    buffer[index] = buffer[index - extraUniforms.indexOffset] - uniforms.offset;
}

[shader("compute")]
[numthreads(8, 1, 1)]
void multiplyAndAdd(uint3 threadId: SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (index < extraUniforms.indexOffset) return;
    buffer[index] = buffer[index - extraUniforms.indexOffset] * uniforms.scale + uniforms.offset;
}
