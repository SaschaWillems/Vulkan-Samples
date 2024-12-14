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

struct VertexOutput
{
	float4 position: SV_Position;
	float3 color: COLOR0;    
};

groupshared SharedData sharedData;

[numthreads(numMeshInvocationsX, numMeshInvocationsY, 1)]
// we'll emit 4 vertices on 2 primitives per mesh shader invocation
[outputtopology("triangle")]
void main(out indices uint3 triangles[2 * numMeshInvocationsX * numMeshInvocationsY], out vertices VertexOutput vertices[4 * numMeshInvocationsX * numMeshInvocationsY], uint3 DispatchThreadID : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex, uint3 GroupID : SV_GroupID, uint3 GroupThreadID : SV_GroupThreadID)
{
    SetMeshOutputCounts(3, 1);
    if ( GroupIndex == 0 )
    {
        // set the number of vertices and primitives to put out just once for the complete workgroup
        SetMeshOutputCounts(4 * numMeshInvocationsX * numMeshInvocationsY, 2 * numMeshInvocationsX * numMeshInvocationsY); 
    }
    // There is no equivalnent for gl_WorkGroupSize in HLSL, so we define work group size manually instead
    const int2 workGroupSize = int2(numMeshInvocationsX, numMeshInvocationsY);

    // determine 4 vertices by combining some global position and offset, as well as some local offset and size
    float2 globalOffset = sharedData.offsets[GroupID.x];
    float2 localSize = sharedData.size / workGroupSize.xy;
    float2 localOffset = GroupThreadID.xy * localSize;
    float2 v0 = sharedData.position + globalOffset + localOffset;
    float2 v1 = v0 + float2( localSize.x, 0.0f );
    float2 v2 = v0 + localSize;
    float2 v3 = v0 + float2( 0.0f, localSize.y );

    uint vertexBaseIndex = 4 * GroupIndex;
    vertices[vertexBaseIndex + 0].position = float4( v0, 0.0f, 1.0f );
    vertices[vertexBaseIndex + 1].position = float4( v1, 0.0f, 1.0f );
    vertices[vertexBaseIndex + 2].position = float4( v2, 0.0f, 1.0f );
    vertices[vertexBaseIndex + 3].position = float4( v3, 0.0f, 1.0f );

    uint primitiveBaseIndex = 2 * GroupIndex;
    triangles[primitiveBaseIndex + 0] = uint3( vertexBaseIndex + 0, vertexBaseIndex + 1, vertexBaseIndex + 2 );
    triangles[primitiveBaseIndex + 1] = uint3( vertexBaseIndex + 2, vertexBaseIndex + 3, vertexBaseIndex + 0 );

    vertices[vertexBaseIndex + 0].color = float3( 1.0f, 0.0f, 0.0f );
    vertices[vertexBaseIndex + 1].color = float3( 0.0f, 1.0f, 0.0f );
    vertices[vertexBaseIndex + 2].color = float3( 0.0f, 0.0f, 1.0f );
    vertices[vertexBaseIndex + 3].color = float3( 1.0f, 1.0f, 0.0f );
}
