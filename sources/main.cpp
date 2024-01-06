#include "surf.h"

#include <cstdio>

#include "bvh.h"
#include "camera.h"
#include "material.h"
#include "mesh.h"
#include "render_context.h"
#include "renderer.h"
#include "scene.h"
#include "surf_math.h"
#include "pixel_buffer.h"
#include "timer.h"
#include "types.h"
#include "window_manager.h"

#ifdef _WIN32
#ifndef NDEBUG

// Windows debug mem leak detection
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#endif
#endif // _WIN32

#define RESOLUTION_SCALE	1.0f
#define	CAMERA_SPEED		2.0f
#define NUM_SMOOTH_FRAMES	20	// Number of frames to smooth FPS / frame timing over

#define FRAMEDATA_OUTPUT		1
#define WAVEFRONT_PATHTRACING	1

void handleCameraInput(GLFWwindow* window, Camera& camera, F32 deltaTime, bool& updated)
{
	updated = false;

	Float3 forward = camera.forward.normalize();
	Float3 right = WORLD_UP.cross(forward).normalize();
	Float3 up = forward.cross(right).normalize();

	Float3 deltaPosition(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		deltaPosition += 2.0f * CAMERA_SPEED * forward * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		deltaPosition -= 2.0f * CAMERA_SPEED * forward * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		deltaPosition += 2.0f * CAMERA_SPEED * right * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		deltaPosition -= 2.0f * CAMERA_SPEED * right * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
	{
		deltaPosition += 2.0f * CAMERA_SPEED * up * deltaTime;
		updated = true;
	}

	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
	{
		deltaPosition -= 2.0f * CAMERA_SPEED * up * deltaTime;
		updated = true;
	}

	camera.position += deltaPosition;
	Float3 target = camera.position + forward;

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
	{
		target += CAMERA_SPEED * up * deltaTime;
		updated = true;
	}
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
	{
		target -= CAMERA_SPEED * up * deltaTime;
		updated = true;
	}
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
	{
		target += CAMERA_SPEED * right * deltaTime;
		updated = true;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
	{
		target -= CAMERA_SPEED * right * deltaTime;
		updated = true;
	}

	if (!updated)
		return;

	camera.forward = (target - camera.position).normalize();
	right = WORLD_UP.cross(camera.forward).normalize();
	camera.up = camera.forward.cross(right).normalize();
	camera.generateViewPlane();
}

int main()
{
#ifdef _WIN32
#ifndef NDEBUG
	// Enable debug mem heap tracking
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
#endif

	WindowManager windowManager;
	GLFWwindow* window = windowManager.createWindow(PROGRAM_NAME, SCR_WIDTH, SCR_HEIGHT);

	RenderContext renderContext(window);
	FramebufferSize resolution = renderContext.getFramebufferSize();

#if WAVEFRONT_PATHTRACING == 0
	PixelBuffer resultBuffer(
		static_cast<U32>(resolution.width * RESOLUTION_SCALE),
		static_cast<U32>(resolution.height * RESOLUTION_SCALE)
	);
#endif

	Camera worldCam(
		Float3(0.0f, 0.0f, -7.0f),
		Float3(0.0f, 0.0f, 0.0f),
		static_cast<U32>(resolution.width * RESOLUTION_SCALE),
		static_cast<U32>(resolution.height * RESOLUTION_SCALE)
	);

	// -- BEGIN Scene setup

	Mesh susanneMesh("assets/susanne.obj");
	Mesh cubeMesh("assets/cube.obj");
	Mesh lensMesh("assets/lens.obj");
	Mesh planeMesh("assets/plane.obj");

	BvhBLAS susanneBVH(&susanneMesh);
	BvhBLAS cubeBVH(&cubeMesh);
	BvhBLAS lensBVH(&lensMesh);
	BvhBLAS planeBVH(&planeMesh);

	Material floorMaterial = Material{};
	floorMaterial.albedo = RgbColor(0.8f);
	floorMaterial.reflectivity = 0.01f;

	Material diffuseMaterial = Material{};
	diffuseMaterial.albedo = RgbColor(1.0f, 0.0f, 0.0f);

	Material dielectricMaterial = Material{};
	dielectricMaterial.albedo = RgbColor(0.7f, 0.7f, 0.2f);
	dielectricMaterial.absorption = RgbColor(0.3f, 0.04f, 0.3f);
	dielectricMaterial.refractivity = 1.0f;
	dielectricMaterial.indexOfRefraction = 1.42f;

	Material specularMaterial = Material{};
	specularMaterial.albedo = RgbColor(0.2f, 0.9f, 1.0f);
	specularMaterial.reflectivity = 0.8f;

	Material lightMaterial = Material{};
	lightMaterial.emissionColor = RgbColor(1.0f, 1.0f, 0.7f);
	lightMaterial.emissionStrength = 5.0f;

	Instance cubeL(
		&cubeBVH,
		&lightMaterial,
		glm::translate(
			Mat4(1.0f),
			static_cast<glm::vec3>(Float3(-10.0f, 5.0f, 0.0f))
		)
	);

	Instance cubeR(
		&cubeBVH,
		&lightMaterial,
		glm::translate(
			Mat4(1.0f),
			static_cast<glm::vec3>(Float3(10.0f, 5.0f, 0.0f))
		)
	);

	Instance floor(
		&planeBVH,
		&floorMaterial,
		glm::scale(
			glm::translate(
				Mat4(1.0f),
				static_cast<glm::vec3>(Float3(0.0f, -1.0f, 0.0f))
			),
			static_cast<glm::vec3>(Float3(10.0f, 10.0f, 10.0f))
		)
	);

	Instance susanne0(
		&susanneBVH,
		&diffuseMaterial,
		glm::translate(
			Mat4(1.0f),
			static_cast<glm::vec3>(Float3(0.0f, 0.0f, -1.0f))
		)
	);

	Instance susanne1(
		&susanneBVH,
		&specularMaterial,
		glm::translate(
			Mat4(1.0f),
			static_cast<glm::vec3>(Float3(3.0f, 0.0f, -1.0f))
		)
	);

	Instance lens0(
		&lensBVH,
		&dielectricMaterial,
		glm::translate(
			Mat4(1.0f),
			static_cast<glm::vec3>(Float3(-3.0f, 0.0f, -1.0f))
		)
	);

	SceneBackground background = SceneBackground{};
	background.type = BackgroundType::ColorGradient;
	background.gradient.colorA = RgbColor(1.0f, 1.0f, 1.0f);
	background.gradient.colorB = RgbColor(0.2f, 0.7f, 0.9f);

	Scene scene(background, { floor, cubeL, cubeR, susanne0, susanne1, lens0 });

	// -- END Scene setup

#if WAVEFRONT_PATHTRACING == 0
	RendererConfig rendererConfig = RendererConfig{
		5,	// Max bounces
		1	// Samples per frame
	};

	Renderer renderer(&renderContext, rendererConfig, resultBuffer, worldCam, scene);
#else
	GPUMesh susanneGPUMesh(&renderContext, susanneMesh);
	GPUMesh cubeGPUMesh(&renderContext, cubeMesh);
	GPUMesh lensGPUMesh(&renderContext, lensMesh);
	GPUMesh planeGPUMesh(&renderContext, planeMesh);

	// TODO:
	// - Load all hsot visible GPU buffers into single (per scene?) device local buffer
	// - Load BLAS data into GPU device local buffer
	// - Load instance data into GPU device local buffer (extra index offset into mesh & bvh buffer for start of per instance data)
	// - Load TLAS data into GPU device local buffer
	//
	// Above managed by GPUScene on loading of TLAS

	RendererConfig rendererConfig = RendererConfig{
		5,	// Max bounces
		1	// Samples per frame
	};

	FramebufferSize renderResolution = FramebufferSize{
		static_cast<U32>(resolution.width * RESOLUTION_SCALE),
		static_cast<U32>(resolution.height * RESOLUTION_SCALE)
	};

	WaveFrontRenderer renderer(&renderContext, rendererConfig, renderResolution, worldCam, scene);
#endif

	// Create frame timer
	Timer frameTimer;

	printf("Initialized Surf\n");

	F32 deltaTime = 0.0f;
	while (!glfwWindowShouldClose(window))
	{
		static F32 SMOOTH_FRAC = 1.0f / NUM_SMOOTH_FRAMES;
		static F32 AVERAGE_FRAMETIME = 10.0f;
		static F32 ALPHA = 1.0f;

		// Update scene state
		scene.update(deltaTime);

		// Render frame
		RendererConfig& config = renderer.config();	// Used only for debug info now -> can be updated using UI
		renderer.render(deltaTime);

		// Handle input
		bool cameraUpdated = false;
		handleCameraInput(window, worldCam, deltaTime, cameraUpdated);
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		if (cameraUpdated)
			renderer.clearAccumulator();

		// Tick frame timer and update average trackers
		frameTimer.tick();
		deltaTime = frameTimer.deltaTime();

#if FRAMEDATA_OUTPUT == 1
		AVERAGE_FRAMETIME = (1.0f - ALPHA) * AVERAGE_FRAMETIME + ALPHA * (deltaTime * 1'000.0f);
		if (ALPHA > SMOOTH_FRAC) ALPHA *= 0.5;

		F32 inv_avg_frametime = 1.0f / AVERAGE_FRAMETIME;
		F32 rps = (resolution.width * RESOLUTION_SCALE * resolution.height * RESOLUTION_SCALE * config.samplesPerFrame) * inv_avg_frametime;
		const FrameInstrumentationData& frameInfo = renderer.frameInfo();
		
		printf(
			"%08.2fms (%05.1f fps) - %08.2fMrays/s - %05u samples (%u spp) - %010.2f Lumen\n",
			AVERAGE_FRAMETIME,
			inv_avg_frametime * 1'000.0f,
			rps / 1'000.0f,
			frameInfo.totalSamples,
			config.samplesPerFrame,
			frameInfo.energy
		);
#endif

		glfwPollEvents();
	}

	printf("Goodbye!\n");
	return 0;
}
