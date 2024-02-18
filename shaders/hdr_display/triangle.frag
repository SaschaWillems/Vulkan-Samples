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


// @todo: replace, directly convert at triangle setup?
vec3 xyYToRec709(vec2 xy, float Y)
{
    // https://github.com/ampas/aces-dev/blob/v1.0.3/transforms/ctl/README-MATRIX.md
    const mat3x3 XYZtoRGB = mat3x3(
        3.2409699419, -1.5373831776, -0.4986107603,
        -0.9692436363, 1.8759675015, 0.0415550574,
        0.0556300797, -0.2039769589, 1.0569715142
    );
    vec3 XYZ = Y * vec3(xy.x / xy.y, 1.0, (1.0 - xy.x - xy.y) / xy.y);
    vec3 RGB = XYZ * XYZtoRGB;
    float maxChannel = max(RGB.r, max(RGB.g, RGB.b));
    return RGB / max(maxChannel, 1.0);
}

void main()
{
	outFragColor = vec4(xyYToRec709(inColor.xy, 1.0), 1.0);
}
