# Copyright (c) 2022-2024, Sascha Willems
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 the "License";
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_LIST_DIR} PATH)
get_filename_component(CATEGORY_NAME ${PARENT_DIR} NAME)

add_sample(
    ID ${FOLDER_NAME}
    CATEGORY ${CATEGORY_NAME}
    AUTHOR "Sascha Willems"
    NAME "Conditional rendering"
    DESCRIPTION "Demonstrates usage of VK_EXT_conditional_rendering for conditionally toggling the visibility of sub-meshes of a complex glTF model."
    SHADER_FILES_GLSL
        "conditional_rendering/glsl/model.vert"
        "conditional_rendering/glsl/model.frag"
    SHADER_FILES_HLSL
        "conditional_rendering/hlsl/model.vert.hlsl"
        "conditional_rendering/hlsl/model.frag.hlsl")
