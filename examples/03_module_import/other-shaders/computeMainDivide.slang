implementing buffer_math;

import utils;

[shader("compute")]
[numthreads(8,1,1)]
void computeMainDivide(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    if (!checkThreadId(index)) return;
    result[index] = buffer0[index] / buffer1[index];
}
