/* Copyright (c) 2023, Holochip Corporation
 * Copyright (c) 2024, Sascha Willems
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mesh_shader_shared.hlsl"

struct UBO
{
    float cull_center_x;
    float cull_center_y;
    float cull_radius;
    float meshlet_density;
};
[[vk::binding(0, 0)]]
ConstantBuffer<UBO> ubo : register(b0); 

groupshared SharedData sharedData;

float square(float x)
{
    return x * x;
}

float square(float2 p )
{
    return p.x * p.x + p.y * p.y;
}

// HLSL does not have an equivalent for gl_NumWorkGroups, we need to use a builtin for that instead
[[vk::ext_builtin_input(/* NumWorkgroups */ 24)]]
static const uint3 NumWorkGroups;

// Use numTaskInvocationsX * numTaskInvocationsY task shader invocations per task shader workgroup,
// resulting in N * N * numTaskInvocationsX * numTaskInvocationsY invocations
[numthreads(numTaskInvocationsX, numTaskInvocationsY, 1)] 
void main(uint3 DispatchThreadID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
    // calculate size and offset of sub-rect per task shader invocation
    float2 size = 2.0f / ( NumWorkGroups.xy * NumWorkGroups.xy );
    float2 offset = DispatchThreadID.xy * size;

    // determine the four corners of the sub-rect and check if it's completely out of the culling circle
    // (ignoring the case that a sub-rect could completely include the culling circle...)

    float2 position = float2(ubo.cull_center_x, ubo.cull_center_y);
    float2 p0 = position + offset;
    float2 p1 = p0 + float2(size.x, 0.0f);
    float2 p2 = p0 + size;
    float2 p3 = p0 + float2(0.0f, size.y);
    float squaredRadius = square(ubo.cull_radius);
    bool isValid = ( square(p0) < squaredRadius )
                    && ( square(p1) < squaredRadius )
                    && ( square(p2) < squaredRadius)
                    && ( square(p3) < squaredRadius);
    // contribute a single bit by this task shader invocation
    uint4 validVotes = WaveActiveBallot(isValid);

    if ( isValid )
    {
        // get the next free index into the offsets array
        uint index = WavePrefixCountBits(isValid); // subgroupBallotExclusiveBitCount
        sharedData.offsets[index] = offset;
    }
    if ( GroupIndex == 0 )
    {
        // for just one task shader invocation we can emit mesh shaders
        sharedData.position = position;
        sharedData.size = size;
        // get the actual number of task shader invocations that are determined to display something
        uint validCount = WaveActiveCountBits(isValid); // subgroupBallotBitCount
        DispatchMesh(validCount, 1, 1, sharedData);
    }
}
