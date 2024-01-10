#pragma once

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "render_context.h"
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

	void draw(F32 deltaTime);

private:
	void setupWindowData(ImGui_ImplVulkanH_Window& wd);

private:
	RenderContext* m_renderContext		= nullptr;
	DescriptorPool m_guiDescriptorPool	= DescriptorPool(m_renderContext->device);

	// GUI setup
	ImGuiContext* m_guiContext;
	ImGui_ImplVulkanH_Window m_windowData;
};
