#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "render_context.h"
#include "vk_layer/framebuffer.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/image.h"
#include "vk_layer/descriptor_pool.h"

enum class UIStyle
{
	DarkMode,
	LightMode
};

struct UIState
{
	F32 focalLength = 0.0f;
	F32 defocusAngle = 0.0f;
	bool animate = false;
	U32 spp = 1;
	bool updated = false;
};

class UIManager
{
public:
	UIManager(RenderContext* renderContext, UIStyle style = UIStyle::DarkMode);

	~UIManager();

	void drawUI(F32 deltaTime, UIState& state);

	void recordGUIPass(VkCommandBuffer cmdBuffer, U32 frameIndex);

private:
	RenderContext* m_renderContext		= nullptr;
	ImGuiContext* m_guiContext			= nullptr;
	DescriptorPool m_guiDescriptorPool	= DescriptorPool(m_renderContext->device);

	// GUI render pass & render targets
	RenderPass m_guiRenderPass			= RenderPass(m_renderContext->device, m_renderContext->swapchain.image_format);
	std::vector<Framebuffer> m_framebuffers;
};
