#include "ui_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "render_context.h"
#include "vk_layer/descriptor_pool.h"
#include "vk_layer/vk_check.h"

UIManager::UIManager(RenderContext* renderContext, UIStyle style)
	:
	m_renderContext(renderContext),
	m_guiContext(nullptr),
	m_windowData{}
{
	assert(m_renderContext != nullptr);
	IMGUI_CHECKVERSION();
	m_guiContext = ImGui::CreateContext();

	// Setup control listeners
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup UI Style
	switch (style)
	{
	case UIStyle::LightMode:
		ImGui::StyleColorsLight();
		break;
	case UIStyle::DarkMode:
		ImGui::StyleColorsDark();
		break;
	}

	// Init GLFW vulkan IMGUI backend
	ImGui_ImplGlfw_InitForVulkan(m_renderContext->window, true);
	setupWindowData(m_windowData);

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = m_renderContext->instance;
	initInfo.PhysicalDevice = m_renderContext->gpu;
	initInfo.Device = m_renderContext->device;
	initInfo.QueueFamily = m_renderContext->queues.graphicsQueue.familyIndex;
	initInfo.Queue = m_renderContext->queues.graphicsQueue.handle;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_guiDescriptorPool.handle();
	initInfo.Subpass = 0;
	initInfo.MinImageCount = m_renderContext->swapchain.requested_min_image_count;
	initInfo.ImageCount = m_renderContext->swapchain.image_count;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.Allocator = nullptr;
	ImGui_ImplVulkan_Init(&initInfo, m_windowData.RenderPass);
}

UIManager::~UIManager()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext(m_guiContext);
}

void UIManager::draw(F32 deltaTime)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Render();
}

void UIManager::setupWindowData(ImGui_ImplVulkanH_Window& wd)
{
	FramebufferSize framebufferSize = m_renderContext->getFramebufferSize();

	wd.Surface = m_renderContext->renderSurface;
	wd.SurfaceFormat = VkSurfaceFormatKHR{ m_renderContext->swapchain.image_format, m_renderContext->swapchain.color_space };
	wd.PresentMode = m_renderContext->swapchain.present_mode;

	ImGui_ImplVulkanH_CreateOrResizeWindow(
		m_renderContext->instance,
		m_renderContext->gpu,
		m_renderContext->device,
		&wd,
		m_renderContext->queues.graphicsQueue.familyIndex,
		nullptr,
		framebufferSize.width, framebufferSize.height,
		m_renderContext->swapchain.requested_min_image_count
	);
}
