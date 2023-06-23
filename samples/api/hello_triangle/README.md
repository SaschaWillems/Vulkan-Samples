<!--
- Copyright (c) 2019-2023, The Khronos Group
- Copyright (c) 2023, Sascha Willems
-
- SPDX-License-Identifier: Apache-2.0
-
- Licensed under the Apache License, Version 2.0 the "License";
- you may not use this file except in compliance with the License.
- You may obtain a copy of the License at
-
-     http://www.apache.org/licenses/LICENSE-2.0
-
- Unless required by applicable law or agreed to in writing, software
- distributed under the License is distributed on an "AS IS" BASIS,
- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
- See the License for the specific language governing permissions and
- limitations under the License.
-
-->

# Hello (Vulkan) Triangle

## Overview

This samples demonstrates how to render a triangle in **unextended Vulkan 1.0**. It's mostly self-contained and only makes minimal use of the framework. It serves as a starting point for developers wanting to get into Vulkan and who need to learn how a basic Vulkan application is structured, which resources are involved and how to properly use the api.

### Vulkan is explicit

Vulkan is a low-level api that hands over a lot of control and responsibility that has traditionally been part of the implementation to the developer. This makes it very explicit and verbose, contrary to other apis like OpenGL, where the implementation hides a lot of details from the developer. As this sample is an introduction to using Vulkan it adheres to that and is purposely explicit and verbose. For other samples in this repository most of this is encapsulated into a shared framework. 

### Vulkan is evolving

Since it's release in 2016, Vulkan has been constantly evolving. New extensions are released on a regular basis. Some just change minor things while others add new concepts (e.g. hardware accelerated ray tracing) or even fix problems with the api design (e.g. replacing render passes) New core versions are released from time to time that make extensions mandatory, a list of those can be found [here](https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/vulkan_release_summary.adoc). "Core version" means that if you go with this version as the baseline for your application you don't need to check for the extension that are part of this (and lower) core versions. As noted above this sample is using unextended Vulkan version 1.0.

## Concepts

@todo: add notes on alternatives with ext, newer versions, e.g. render passes or pipelines

A low level api like Vulkan brings a lot of new concepts that developer didn't have to deal with in other apis, or at least not to the extend that is required in Vulkan. Those concepts are important to know as they have direct influence at how a Vulkan application needs to be structured in order to fully leverage the potential of the api.

@todo: Start with initialization

### Synchronization

Usually, realtime rendering involves both the CPU and the GPU. To get the most out of them, workloads need to be synchronized in an optimal way between these two. In Vulkan, synchronization is very explicit and is  controlled by the application. While this opens up a lot of optimization options, it can actually lead to worse performance (than on older apis) when not used correct. It's therefore important to use synchronization in such a way that the CPU and GPU run in parallel as much as possible. For this we'll be using the different synchronization primitives in this sample:

#### Fences

A fence (`VkFence`) is a synchronization primitive for a queue to host (GPU to CPU) dependency. Using a fence the host application (CPU) can check if work submitted to a queue (GPU) has finished execution.

In this sample we use one fence per frame (see `PerFrame::queue_submit_fence`) to make sure that the command buffer (of that frame) has finished executing (on the GPU) before we start working on it again (on the CPU).

This is achieved by passing this fence to `vkQueueSubmit`, which signals it (from the GPU) once execution has finished, and then (later on) by checking that state using `vkWaitForFences`.

```cpp
// Signalling
void HelloTriangle::render_triangle(Context &context, uint32_t swapchain_index)
{
    ...
	VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	info.commandBufferCount   = 1;
	info.pCommandBuffers      = &cmd;
	info.waitSemaphoreCount   = 1;
	info.pWaitSemaphores      = &context.per_frame[swapchain_index].swapchain_acquire_semaphore;
	info.pWaitDstStageMask    = &wait_stage;
	info.signalSemaphoreCount = 1;
	info.pSignalSemaphores    = &context.per_frame[swapchain_index].swapchain_release_semaphore;
	VK_CHECK(vkQueueSubmit(context.queue, 1, &info, context.per_frame[swapchain_index].queue_submit_fence));
    ...
}

// Waiting
VkResult HelloTriangle::acquire_next_image(Context &context, uint32_t *image)
{
    ...
	if (context.per_frame[*image].queue_submit_fence != VK_NULL_HANDLE)
	{
		vkWaitForFences(context.device, 1, &context.per_frame[*image].queue_submit_fence, true, UINT64_MAX);
		vkResetFences(context.device, 1, &context.per_frame[*image].queue_submit_fence);
	}
    ...    
}
```

#### Semaphores

A semaphore (`VkSemaphore`) is a synchronization primitive 

### Swapchain

### Render state
-- @todo: not sure if this is a good caption

Higher level apis like OpenGL allow you to change state at any given time (aka "state machine"). While this makes things easy from a developer's point-of-view it makes things hard for implementations as they have to deal with the fact that state can change at any time. Vulkan changes this approach and has the developer declare as much state as possible up-front. This gives the implementation certain guarantees leaving a lot of new room for optimizations.

-- @todo: add note that this is somehow changed with dynamic state extensions

#### Render passes

#### Pipelines

### Command buffers

### Shaders

@todo: note on SPIR-V

### Validation

@todo: add note that this helps developers

## Application structure

@todo

### Setup

@todo: add note that instance, device, etc. are all just abstractions

All Vulkan resources are created in the `HelloTriangle::prepare` function. The ordering is important, as Vulkan objects may require other objects that they depend on at creation time. The basic logic of this is the same all Vulkan applications.

In general there are two basic dependencies: Vulkan objects are either attached to an instance or to a device.

The Vulkan instance is always the starting point.

For this sample the creation flow looks like this

* Create the Vulkan instance
* Create the Vulkan device (based on the instance)
* Create the device based Vulkan resources
    * Create the swap chain
    * Create the render pass
    * Create the pipeline state object
    * Create the frame buffers

### Rendering

### Teardown

## Conclusion

Following this sample you should know have a basic knowledge of how Vulkan applications works. The other samples in this repository build upon the foundations in this sample. 

## Things to look for afterwards

// @todo: bad title

- sync2
- timeline semaphores
- descriptor buffers
- shader objects
