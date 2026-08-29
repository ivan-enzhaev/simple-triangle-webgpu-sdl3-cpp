#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <webgpu/webgpu.h>

#if defined(SDL_PLATFORM_WIN32)
#include <windows.h>
#elif defined(SDL_PLATFORM_ANDROID)
#include <android/native_window.h>
#endif

static SDL_Window *window = NULL;
static WGPUInstance instance = NULL;
static WGPUAdapter adapter = NULL;
static WGPUDevice device = NULL;
static WGPUQueue queue = NULL;
static WGPUSurface surface = NULL;
static WGPUSurfaceConfiguration config = { 0 };
static WGPURenderPipeline pipeline = NULL;

static bool is_configured = false;

static const int WIN_WIDTH = 280;
static const int WIN_HEIGHT = 280;

const char *shader_code =
    "@vertex\n"
    "fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {\n"
    "    var pos = array<vec2f, 3>(\n"
    "        vec2f(0.0, 0.5),\n"
    "        vec2f(-0.5, -0.5),\n"
    "        vec2f(0.5, -0.5)\n"
    "    );\n"
    "    return vec4f(pos[in_vertex_index], 0.0, 1.0);\n"
    "}\n\n"
    "@fragment\n"
    "fn fs_main() -> @location(0) vec4f {\n"
    "    return vec4f(1.0, 0.5, 0.0, 1.0); // Orange color\n"
    "}\n";

#ifdef __cplusplus
static inline WGPUStringView WGPU_STR(const char *s) {
    WGPUStringView view;
    view.data = s;
    view.length = (s != NULL) ? SDL_strlen(s) : WGPU_STRLEN;
    return view;
}
#else
#define WGPU_STR(s) \
    (WGPUStringView) { .data = s, .length = (s != NULL) ? SDL_strlen(s) : WGPU_STRLEN }
#endif

static WGPUSurface CreateWGPUSurface(WGPUInstance inst, SDL_Window *win)
{
#if defined(__EMSCRIPTEN__)
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasSource = {
        .chain = { .sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector },
        .selector = WGPU_STR("#canvas")
    };
    WGPUSurfaceDescriptor desc = { .nextInChain = (WGPUChainedStruct *)&canvasSource };
    return wgpuInstanceCreateSurface(inst, &desc);

#elif defined(SDL_PLATFORM_WIN32)
    SDL_PropertiesID props = SDL_GetWindowProperties(win);
    HWND hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
    HINSTANCE hinstance = (HINSTANCE)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, NULL);

    WGPUSurfaceSourceWindowsHWND hwndSource = {
        .chain = { .sType = WGPUSType_SurfaceSourceWindowsHWND },
        .hinstance = hinstance,
        .hwnd = hwnd
    };
    WGPUSurfaceDescriptor desc = { .nextInChain = (WGPUChainedStruct *)&hwndSource };
    return wgpuInstanceCreateSurface(inst, &desc);

#elif defined(SDL_PLATFORM_ANDROID)
    SDL_PropertiesID props = SDL_GetWindowProperties(win);
    ANativeWindow *aNativeWindow = (ANativeWindow *)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);

    WGPUSurfaceSourceAndroidNativeWindow androidSource = {
        .chain = { .sType = WGPUSType_SurfaceSourceAndroidNativeWindow },
        .window = aNativeWindow
    };
    WGPUSurfaceDescriptor desc = { .nextInChain = (WGPUChainedStruct *)&androidSource };
    return wgpuInstanceCreateSurface(inst, &desc);
#else
#error "Platform surface mapping not implemented"
#endif
}

void handle_device_request(
    WGPURequestDeviceStatus status,
    WGPUDevice res,
    WGPUStringView message,
    void *userdata1,
    void *userdata2)
{
    printf("DEVICE CALLBACK: status=%d\n", status);

    if (message.data && message.length > 0)
        printf("DEVICE MESSAGE: %.*s\n",
            (int)message.length,
            message.data);

    if (status == WGPURequestDeviceStatus_Success)
    {
        device = res;
        printf("DEVICE CREATED\n");
    }
    else
    {
        printf("DEVICE CREATION FAILED\n");
    }
}

void handle_adapter_request(
    WGPURequestAdapterStatus status,
    WGPUAdapter res,
    WGPUStringView message,
    void *userdata1,
    void *userdata2)
{
    printf("ADAPTER CALLBACK: status=%d\n", status);

    if (message.data && message.length > 0)
        printf("ADAPTER MESSAGE: %.*s\n",
            (int)message.length,
            message.data);

    if (status == WGPURequestAdapterStatus_Success)
    {
        adapter = res;
        printf("ADAPTER CREATED\n");

        WGPURequestDeviceCallbackInfo deviceCallbackInfo = {
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = handle_device_request
        };

        wgpuAdapterRequestDevice(adapter, NULL, deviceCallbackInfo);
    }
    else
    {
        printf("ADAPTER CREATION FAILED\n");
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        return SDL_APP_FAILURE;

    window = SDL_CreateWindow("WebGPU Triangle", WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_RESIZABLE);
    instance = wgpuCreateInstance(NULL);
    surface = CreateWGPUSurface(instance, window);

    WGPURequestAdapterOptions opt = { .compatibleSurface = surface };

    WGPURequestAdapterCallbackInfo adapterCallbackInfo = {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = (WGPURequestAdapterCallback)handle_adapter_request
    };

    wgpuInstanceRequestAdapter(instance, &opt, adapterCallbackInfo);

#ifndef __EMSCRIPTEN__
    while (adapter == NULL || device == NULL)
    {
        SDL_Delay(1);
    }
#endif

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    if (!device)
    {
        return SDL_APP_CONTINUE;
    }

    if (!is_configured)
    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        if (w <= 0 || h <= 0)
        {
            return SDL_APP_CONTINUE; // Surface isn't ready on Android yet
        }

        queue = wgpuDeviceGetQueue(device);

        // Dynamically query supported surface capabilities
        WGPUSurfaceCapabilities caps = { 0 };
        wgpuSurfaceGetCapabilities(surface, adapter, &caps);

        // WGPUTextureFormat surface_format = (caps.formatCount > 0) ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;

        WGPUTextureFormat surface_format = WGPUTextureFormat_Undefined;

        for (size_t i = 0; i < caps.formatCount; ++i)
        {
            if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm)
            {
                surface_format = caps.formats[i];
                break;
            }

            if (caps.formats[i] == WGPUTextureFormat_RGBA8Unorm)
            {
                surface_format = caps.formats[i];
            }
        }

        if (surface_format == WGPUTextureFormat_Undefined)
        {
            surface_format = caps.formats[0];
        }

        WGPUPresentMode present_mode = (caps.presentModeCount > 0) ? caps.presentModes[0] : WGPUPresentMode_Fifo;

        config.device = device;
        config.format = surface_format;
        config.usage = WGPUTextureUsage_RenderAttachment;
        config.width = (uint32_t)w;
        config.height = (uint32_t)h;
        config.presentMode = present_mode;
        wgpuSurfaceConfigure(surface, &config);

        wgpuSurfaceCapabilitiesFreeMembers(caps);

        WGPUShaderSourceWGSL wgsl_desc = { 0 };
        wgsl_desc.chain.sType = WGPUSType_ShaderSourceWGSL;
        wgsl_desc.code = WGPU_STR(shader_code);

        WGPUShaderModuleDescriptor shader_desc = { 0 };
        shader_desc.nextInChain = &wgsl_desc.chain;
        WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(device, &shader_desc);

        WGPURenderPipelineDescriptor pipeline_desc = { 0 };
        pipeline_desc.vertex.module = shader_module;
        pipeline_desc.vertex.entryPoint = WGPU_STR("vs_main");

        WGPUColorTargetState color_target = { .format = config.format, .writeMask = WGPUColorWriteMask_All };
        WGPUFragmentState fragment_state = {
            .module = shader_module,
            .entryPoint = WGPU_STR("fs_main"),
            .targetCount = 1,
            .targets = &color_target
        };
        pipeline_desc.fragment = &fragment_state;
        pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pipeline_desc.multisample.count = 1;
        pipeline_desc.multisample.mask = 0xFFFFFFFF;

        pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeline_desc);
        wgpuShaderModuleRelease(shader_module);

        is_configured = true;
        printf("WebGPU Device/Queue ready and Surface configured.\n");
    }

    WGPUSurfaceTexture surfaceTexture = { 0 };
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

#if defined(WGPUSurfaceGetCurrentTextureStatus_Success) && defined(WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
#define SURFACE_STATUS_SUCCESS(s) ((s) == WGPUSurfaceGetCurrentTextureStatus_Success || (s) == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
#elif defined(WGPUSurfaceGetCurrentTextureStatus_SuccessStatus)
#define SURFACE_STATUS_SUCCESS(s) ((s) == WGPUSurfaceGetCurrentTextureStatus_SuccessStatus)
#else
#define SURFACE_STATUS_SUCCESS(s) ((s) == 0 || (s) == 1)
#endif

    if (!SURFACE_STATUS_SUCCESS(surfaceTexture.status))
    {
        return SDL_APP_CONTINUE;
    }

    WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, NULL);
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);

    WGPURenderPassColorAttachment colorAttachment = {
        .nextInChain = NULL,
        .view = view,
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
        .resolveTarget = NULL,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = { 0.2, 0.2, 0.2, 1.0 }
    };

    WGPURenderPassDescriptor renderPassDesc = {
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment
    };

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, NULL);
    wgpuQueueSubmit(queue, 1, &commandBuffer);

#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(surface);
#endif

    if (commandBuffer)
    {
        wgpuCommandBufferRelease(commandBuffer);
    }
    if (pass)
    {
        wgpuRenderPassEncoderRelease(pass);
    }
    if (encoder)
    {
        wgpuCommandEncoderRelease(encoder);
    }
    if (view)
    {
        wgpuTextureViewRelease(view);
    }
    if (surfaceTexture.texture)
    {
        wgpuTextureRelease(surfaceTexture.texture);
    }

#undef SURFACE_STATUS_SUCCESS

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (pipeline)
        wgpuRenderPipelineRelease(pipeline);
    if (surface)
        wgpuSurfaceUnconfigure(surface);
    if (queue)
        wgpuQueueRelease(queue);
    if (device)
        wgpuDeviceRelease(device);
    if (adapter)
        wgpuAdapterRelease(adapter);
    if (surface)
        wgpuSurfaceRelease(surface);
    if (instance)
        wgpuInstanceRelease(instance);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}
