module buffer_math;

import utils;

StructuredBuffer<float> buffer0;
StructuredBuffer<float> buffer1;
RWStructuredBuffer<float> result;

[shader("compute")]
[numthreads(8,1,1)]
void computeMainAdd(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (!checkThreadId(index)) return;
    result[index] = buffer0[index] + buffer1[index];
}

[shader("compute")]
[numthreads(1,1,1)]
void computeMainSub(uint3 threadId: SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (!checkThreadId(index)) return;
    result[index] = buffer0[index] - buffer1[index];
}

[shader("compute")]
[numthreads(8,1,1)]
void computeMainMultiply(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (!checkThreadId(index)) return;
    result[index] = buffer0[index] * buffer1[index];
}

__include "computeMainDivide";

