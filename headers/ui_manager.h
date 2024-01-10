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

class UIManager
{
public:
	UIManager(RenderContext* renderContext, UIStyle style = UIStyle::DarkMode);

	~UIManager();

	void drawUI(F32 deltaTime, bool& updated);

	void recordGUIPass(VkCommandBuffer cmdBuffer, U32 frameIndex);

private:
	RenderContext* m_renderContext		= nullptr;
	ImGuiContext* m_guiContext			= nullptr;
	DescriptorPool m_guiDescriptorPool	= DescriptorPool(m_renderContext->device);

	// GUI render pass & render targets
	RenderPass m_guiRenderPass			= RenderPass(
		m_renderContext->device,
		VK_FORMAT_R8G8B8A8_SRGB
	);

	std::vector<Framebuffer> m_framebuffers;
};
