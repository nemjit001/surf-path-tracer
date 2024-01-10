#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "render_context.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/descriptor_pool.h"

enum class UIStyle
{
	DarkMode,
	LightMode
};

class UIManager
{
public:
	UIManager(RenderContext* renderContext, UIStyle style = UIStyle::DarkMode);

	~UIManager();

	void drawUI(F32 deltaTime, bool& updated);

	void recordGUIPass(VkCommandBuffer cmdBuffer);

private:
	RenderContext* m_renderContext		= nullptr;
	DescriptorPool m_guiDescriptorPool	= DescriptorPool(m_renderContext->device);
	RenderPass m_guiRenderPass			= RenderPass(m_renderContext->device, m_renderContext->swapchain.image_format);
	ImGuiContext* m_guiContext			= nullptr;
};
