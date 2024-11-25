StructuredBuffer<float> buffer0;
RWStructuredBuffer<float> result;
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
[numthreads(8,1,1)]
void add(uint3 threadId : SV_DispatchThreadID, uniform float offset)
{
    uint index = threadId.x;
    if (index < extraUniforms.indexOffset) return;
    result[index] = buffer0[index - extraUniforms.indexOffset] + offset + uniforms.offset;
}

[shader("compute")]
[numthreads(8, 1, 1)]
void sub(uint3 threadId: SV_DispatchThreadID, uniform float offset)
{
    uint index = threadId.x;
    if (index < extraUniforms.indexOffset) return;
    result[index] = buffer0[index - extraUniforms.indexOffset] - offset + uniforms.offset;
}

[shader("compute")]
[numthreads(8, 1, 1)]
void multiplyAndAdd(uint3 threadId: SV_DispatchThreadID, uniform float scale, uniform float offset)
{
    uint index = threadId.x;
    if (index < extraUniforms.indexOffset) return;
    result[index] = buffer0[index - extraUniforms.indexOffset] * scale + offset + uniforms.offset;
}
