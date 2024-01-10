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
	RenderPass m_guiRenderPass			= RenderPass(
		m_renderContext->device,
		std::vector{
			ImageAttachment{
				m_renderContext->swapchain.image_format, VK_SAMPLE_COUNT_1_BIT,
				ImageOps{ VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE },
				ImageOps{ VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE },
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			},
		},
		std::vector{
			AttachmentReference{ AttachmentType::Color, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
		}
	);
	std::vector<Framebuffer> m_framebuffers;
};
