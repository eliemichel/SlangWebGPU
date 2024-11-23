@binding(2) @group(0) var<storage, read_write> result_0 : array<f32>;

@binding(0) @group(0) var<storage, read> buffer0_0 : array<f32>;

@binding(1) @group(0) var<storage, read> buffer1_0 : array<f32>;

@compute
@workgroup_size(1, 1, 1)
fn computeMain(@builtin(global_invocation_id) threadId_0 : vec3<u32>)
{
    var index_0 : u32 = threadId_0.x;
    result_0[index_0] = buffer0_0[index_0] + buffer1_0[index_0];
    return;
}

