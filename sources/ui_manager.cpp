#include "ui_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include "render_context.h"
#include "vk_layer/render_pass.h"
#include "vk_layer/descriptor_pool.h"
#include "vk_layer/vk_check.h"

UIManager::UIManager(RenderContext* renderContext, UIStyle style)
	:
	m_renderContext(renderContext),
	m_guiContext(nullptr)
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

	// Init IMGUI Vulkan support
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
	ImGui_ImplVulkan_Init(&initInfo, m_guiRenderPass.handle());
}

UIManager::~UIManager()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext(m_guiContext);
}

void UIManager::drawUI(F32 deltaTime, bool& updated)
{
	updated = false;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// TODO: setup virtual window

	ImGui::Render();
}

void UIManager::recordGUIPass(VkCommandBuffer cmdBuffer)
{
	VkCommandBufferBeginInfo cmdBegin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	cmdBegin.flags = 0;
	cmdBegin.pInheritanceInfo = nullptr;
	VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBegin));

	// TODO: create images & framebuffers for UI pass
	// TODO: update main render pass to take into account rendered UI layer (read as sampled image from shader?)
	VkRenderPassBeginInfo uiPassBegin = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	uiPassBegin.clearValueCount = 0;
	uiPassBegin.pClearValues = nullptr;
	uiPassBegin.framebuffer = VK_NULL_HANDLE;
	uiPassBegin.renderArea = VkRect2D{};
	uiPassBegin.renderPass = m_guiRenderPass.handle();
	vkCmdBeginRenderPass(cmdBuffer, &uiPassBegin, VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
	vkCmdEndRenderPass(cmdBuffer);

	VK_CHECK(vkEndCommandBuffer(cmdBuffer));
}
