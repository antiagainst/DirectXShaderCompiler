// Run: %dxc -T vs_6_0 -E main

// CHECK: [[scb:%\d+]] = OpSpecConstantTrue %bool
// CHECK: [[sci:%\d+]] = OpSpecConstant %int 12
// CHECK: [[scf:%\d+]] = OpSpecConstant %float 4.2
[[vk::constant_id(0)]]  const bool  specConstBool  = true;
[[vk::constant_id(10)]] const int   specConstInt   = 12;
[[vk::constant_id(20)]] const float specConstFloat = 4.2;

float4 data1[specConstInt];
static float4 data2[specConstInt + 5];

// TODO: support type casting
//float3 data3[specConstBool];
//static float3 data4[uint(specConstFloat) + 6];

void main() {
    float2 local[specConstInt + 2 + specConstBool * 3];
}
