#version 460
/* Copyright (c) 2023, Sascha Willems
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

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 outFragColor;


vec3 Rec709ToRec2020(vec3 color)
{
    const mat3x3 conversion = mat3x3
    (
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578
    );
    return color * conversion;
}

vec3 Rec2020ToRec709(vec3 color)
{
    const mat3x3 conversion = mat3x3
    (
        1.660496, -0.587656, -0.072840,
        -0.124547, 1.132895, -0.008348,
        -0.018154, -0.100597, 1.118751
    );
    return color * conversion;
}

vec3 LinearToST2084(vec3 color)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    vec3 cp = pow(abs(color), vec3(m1));
    return pow((c1 + c2 * cp) / (1 + c3 * cp), vec3(m2));
}


void main()
{
    // @todo
    const float standardNits = 80.0f;
    const float st2084max = 10000.0;
    const float hdrScalar = standardNits / st2084max;

	vec3 color = pow(abs(inColor.rgb), vec3(2.2));

    // The HDR scene is in Rec.709, but the display is Rec.2020
    color = Rec709ToRec2020(color);

    // Apply the ST.2084 curve to the scene.
    color = LinearToST2084(color * hdrScalar);

	outFragColor = vec4(color, 1.0);
}
