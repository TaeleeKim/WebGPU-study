#include "webgpu.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <time.h>

#define MAX_NUM_INSTANCES 16
using namespace glm;

WGPUDevice device;
WGPUQueue queue;
WGPUSwapChain swapchain;

WGPURenderPipeline pipeline;

WGPUBuffer vertBuf; // vertex buffer with triangle position and colours
WGPUBuffer indxBuf; // index buffer
WGPUBuffer uRotBuf; // uniform buffer (containing the rotation angle)
WGPUBuffer uMVPBuf;

WGPUBuffer unifBuf;

WGPUBindGroup bindGroup;

uint16_t WINDOW_WIDTH = 1200;
uint16_t WINDOW_HEIGHT = 800;

struct Cube {
	uint16_t instanceCount = 0;
	uint16_t indexCount = 0;
} cube;

/**
 * Current rotation angle (in degrees, updated per frame).
 */
float rotDeg = 0.0f;

static const uint32_t x_count = 4;
static const uint32_t y_count = 4;
static const uint32_t num_instances = x_count * y_count;
static const uint32_t matrix_float_count = 16; // 4x4 matrix
static const uint32_t matrix_size = 4 * matrix_float_count;
static const uint32_t uniform_buffer_size = num_instances * matrix_size;

mat4 modelMatrices[num_instances];
float mvpMatricesData[matrix_float_count * num_instances];

struct Time {
	double currentTime, deltaTime = 0.0f;
}timeStamp;

struct MVP {
	mat4 model;
	mat4 view;
	mat4 projection;
} view_mtr;

/**
 * WGSL equivalent of \c triangle_vert_spirv.
 */
static char const triangle_vert_wgsl[] = R"(
	struct VertexIn {
		@location(0) aPos : vec3<f32>;
		@location(1) aCol : vec3<f32>;
	};
	struct VertexOut {
		@location(0) vCol : vec3<f32>;
		@builtin(position) Position : vec4<f32>;
	};
	struct Rotation {
		@location(0) degs : f32;
	};
    struct MVP {
		model: mat4x4<f32>;
		view: mat4x4<f32>;
		projection: mat4x4<f32>;
	};	
	@group(0) @binding(0) var<uniform> uRot : Rotation;
    @group(0) @binding(1) var<uniform> uMVP : MVP;
	@stage(vertex)
	fn main(input : VertexIn) -> VertexOut {
		var rads : f32 = radians(uRot.degs);
		var cosA : f32 = cos(rads);
		var sinA : f32 = sin(rads);
		var rot : mat3x3<f32> = mat3x3<f32>(
			vec3<f32>( cosA, sinA, 0.0),
			vec3<f32>(-sinA, cosA, 0.0),
			vec3<f32>( 0.0,  0.0,  1.0));
		var output : VertexOut;

		// Rotate 1번째 방법 - Rotating된 Model을 Shader에 던져준다.
		//var pos = uMVP.projection * uMVP.view * uMVP.model * vec4<f32>(input.aPos, 1.0); 	
		//output.Position = pos;

		// Rotate 2번째 방법 - Shader에서 Ratate된 Model Matrix를 계산한다.
		var model = vec4<f32>(rot * vec3<f32>(input.aPos), 1.0);
        output.Position = uMVP.projection * uMVP.view * model;
		output.vCol = input.aCol;
		return output;
	}
)";


static char const instanced_vertex_shader_wgsl[] =R"(
struct VertexIn {
	@location(0) aPos : vec4<f32>,
	@location(1) aCol : vec3<f32>,
};
struct Uniforms {
    modelViewProjectionMatrix : array<mat4x4<f32>, 16>,
};
struct VertexOutput {
	@location(0) vCol : vec3<f32>,
	@builtin(position) Position : vec4<f32>,
};

@group(0) @binding(0) var<uniform> uniforms : Uniforms;

@stage(vertex)
fn main(
input : VertexIn,
  @builtin(instance_index) instanceIdx : u32,
) -> VertexOutput {
	var output : VertexOutput;
	output.Position = uniforms.modelViewProjectionMatrix[instanceIdx] * input.aPos;
	output.vCol = input.aCol;
	return output;
}

)";

/**
 * WGSL equivalent of \c triangle_frag_spirv.
 */
static char const triangle_frag_wgsl[] = R"(
	@stage(fragment)
	fn main(@location(0) vCol : vec3<f32>) -> @location(0) vec4<f32> {
		return vec4<f32>(vCol, 1.0);
	}
)";

/**
 * Helper to create a shader from SPIR-V IR.
 *
 * \param[in] code shader source (output using the \c -V \c -x options in \c glslangValidator)
 * \param[in] size size of \a code in bytes
 * \param[in] label optional shader name
 */
/*static*/ WGPUShaderModule createShader(const uint32_t* code, uint32_t size, const char* label = nullptr) {
	WGPUShaderModuleSPIRVDescriptor spirv = {};
	spirv.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
	spirv.codeSize = size / sizeof(uint32_t);
	spirv.code = code;
	WGPUShaderModuleDescriptor desc = {};
	desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&spirv);
	desc.label = label;
	return wgpuDeviceCreateShaderModule(device, &desc);
}

/**
 * Helper to create a shader from WGSL source.
 *
 * \param[in] code WGSL shader source
 * \param[in] label optional shader name
 */
static WGPUShaderModule createShader(const char* const code, const char* label = nullptr) {
	WGPUShaderModuleWGSLDescriptor wgsl = {};
	wgsl.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl.source = code;
	WGPUShaderModuleDescriptor desc = {};
	desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl);
	desc.label = label;
	return wgpuDeviceCreateShaderModule(device, &desc);
}

/**
 * Helper to create a buffer.
 *
 * \param[in] data pointer to the start of the raw data
 * \param[in] size number of bytes in \a data
 * \param[in] usage type of buffer
 */
static WGPUBuffer createBuffer(const void* data, size_t size, WGPUBufferUsage usage) {
	WGPUBufferDescriptor desc = {};
	desc.usage = WGPUBufferUsage_CopyDst | usage;
	desc.size  = size;
	WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
	wgpuQueueWriteBuffer(queue, buffer, 0, data, size);
	return buffer;
}

static void setProjectionAndView()
{
	view_mtr.projection = perspective(glm::radians(25.0f), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 10.0f);
	view_mtr.view = lookAt(vec3(50.0f, 50.0f, 50.f), vec3(0.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f));
}

/**
 * Bare minimum pipeline to draw a triangle using the above shaders.
 */
static void createPipelineAndBuffers() {
	// compile shaders
	// NOTE: these are now the WGSL shaders (tested with Dawn and Chrome Canary)
	WGPUShaderModule vertMod = createShader(instanced_vertex_shader_wgsl);
	WGPUShaderModule fragMod = createShader(triangle_frag_wgsl);

	WGPUBufferBindingLayout buf[1] = {};
	buf[0].type = WGPUBufferBindingType_Uniform;

	//buf[1].type = WGPUBufferBindingType_Uniform;

	// bind group layout (used by both the pipeline layout and uniform bind group, released at the end of this function)
	//WGPUBindGroupLayoutEntry bglEntry[2] = {};
	//bglEntry[0].binding = 0;
	//bglEntry[0].visibility = WGPUShaderStage_Vertex;
	//bglEntry[0].buffer = buf[0];

	//bglEntry[1].binding = 1;
	//bglEntry[1].visibility = WGPUShaderStage_Vertex;
	//bglEntry[1].buffer = buf[1];

	//WGPUBindGroupLayoutDescriptor bglDesc = {};
	//bglDesc.entryCount = 2;
	//bglDesc.entries = bglEntry;
	//WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

	WGPUBindGroupLayoutEntry bglEntry_inst[1] = {};
	bglEntry_inst[0].binding = 0;
	bglEntry_inst[0].visibility = WGPUShaderStage_Vertex;
	bglEntry_inst[0].buffer = buf[0];

	WGPUBindGroupLayoutDescriptor bglDesc_inst = {};
	bglDesc_inst.entryCount = 1;
	bglDesc_inst.entries = bglEntry_inst;
	WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc_inst);

	// pipeline layout (used by the render pipeline, released after its creation)
	WGPUPipelineLayoutDescriptor layoutDesc = {};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = &bindGroupLayout;
	WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &layoutDesc);

	// describe buffer layouts
	WGPUVertexAttribute vertAttrs[2] = {};
	vertAttrs[0].format = WGPUVertexFormat_Float32x4;
	vertAttrs[0].offset = 0;
	vertAttrs[0].shaderLocation = 0;
	vertAttrs[1].format = WGPUVertexFormat_Float32x3;
	vertAttrs[1].offset = 4 * sizeof(float);
	vertAttrs[1].shaderLocation = 1;
	WGPUVertexBufferLayout vertexBufferLayout = {};
	vertexBufferLayout.arrayStride = 7 * sizeof(float);
	vertexBufferLayout.attributeCount = 2;
	vertexBufferLayout.attributes = vertAttrs;

	// Fragment state
	WGPUBlendState blend = {};
	blend.color.operation = WGPUBlendOperation_Add;
	blend.color.srcFactor = WGPUBlendFactor_One;
	blend.color.dstFactor = WGPUBlendFactor_One;
	blend.alpha.operation = WGPUBlendOperation_Add;
	blend.alpha.srcFactor = WGPUBlendFactor_One;
	blend.alpha.dstFactor = WGPUBlendFactor_One;

	WGPUColorTargetState colorTarget = {};
	colorTarget.format = webgpu::getSwapChainFormat(device);
	//colorTarget.blend = &blend;
	colorTarget.blend = nullptr;
	colorTarget.writeMask = WGPUColorWriteMask_All;

	WGPUFragmentState fragment = {};
	fragment.module = fragMod;
	fragment.entryPoint = "main";
	fragment.targetCount = 1;
	fragment.targets = &colorTarget;

	WGPUStencilFaceState stencil_state_face_descriptor = {};
	stencil_state_face_descriptor.compare = WGPUCompareFunction_Always;
	stencil_state_face_descriptor.failOp = WGPUStencilOperation_Keep;
	stencil_state_face_descriptor.depthFailOp = WGPUStencilOperation_Keep;
	stencil_state_face_descriptor.passOp = WGPUStencilOperation_Keep;


	WGPUDepthStencilState depth_stencil_state = {};
	depth_stencil_state.format = WGPUTextureFormat_Depth24Plus;
	depth_stencil_state.stencilFront = stencil_state_face_descriptor;
	depth_stencil_state.stencilBack = stencil_state_face_descriptor;
	depth_stencil_state.depthWriteEnabled = true;
	depth_stencil_state.depthCompare = WGPUCompareFunction_Less;
	depth_stencil_state.stencilReadMask = 0xFFFFFFFF;
	depth_stencil_state.stencilWriteMask = 0xFFFFFFFF;
	depth_stencil_state.depthBias = 0;
	depth_stencil_state.depthBiasSlopeScale = 0.0f;
	depth_stencil_state.depthBiasClamp = 0.0f;

	WGPURenderPipelineDescriptor desc = {};
	desc.fragment = &fragment;

	// Other state
	desc.layout = pipelineLayout;

	desc.depthStencil = &depth_stencil_state;
	desc.vertex.module = vertMod;
	desc.vertex.entryPoint = "main";
	desc.vertex.bufferCount = 1;//0;
	desc.vertex.buffers = &vertexBufferLayout;

	desc.multisample.count = 1;
	desc.multisample.mask = 0xFFFFFFFF;
	desc.multisample.alphaToCoverageEnabled = false;

	desc.primitive.frontFace = WGPUFrontFace_CCW;
	desc.primitive.cullMode = WGPUCullMode_None;
	desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
	desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;

	pipeline = wgpuDeviceCreateRenderPipeline(device, &desc);

	// partial clean-up (just move to the end, no?)
	wgpuPipelineLayoutRelease(pipelineLayout);

	wgpuShaderModuleRelease(fragMod);
	wgpuShaderModuleRelease(vertMod);

	const float scaleFactor = 0.5f;

	// create the buffers (x, y, z, w,  r, g, b)
	float vertData[] = {
		// Front
		-0.8f, -0.8f, 0.8f, 0.0f, 1.0f, 1.0f, 0.0f, // BL
		 0.8f, -0.8f, 0.8f, 0.0f, 0.7f, 0.7f, 0.0f, // BR
		-0.8f,  0.8f, 0.8f, 0.0f, 0.7f, 0.7f, 0.0f, // TL
		 0.8f,  0.8f, 0.8f, 0.0f, 0.5f, 0.5f, 0.0f, // TR
		 // Rear
		-0.8f, -0.8f, -0.8f, 0.0f, 0.0f, 0.0f, 1.0f, // BL
		 0.8f, -0.8f, -0.8f, 0.0f, 0.0f, 0.0f, 0.7f, // BR
		-0.8f,  0.8f, -0.8f, 0.0f, 0.0f, 0.0f, 0.7f, // TL
		 0.8f,  0.8f, -0.8f, 0.0f, 0.0f, 0.0f, 0.5f, // TR
	};


	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			vertData[i*7 + j] *= scaleFactor;
		}
	}


	uint16_t const indxData[] = {
		0, 1, 2,
		2, 1, 3,
		
		4, 5, 6,
		6, 5, 7,

		1, 5, 3,
		3, 5, 7,

		0, 4, 2,
		2, 4, 6,

		2, 3, 6,
		6, 3, 7,

		0, 1, 4,
		4, 1, 5
	};

	const float step = 4.0;
	view_mtr.model = mat4(1.0f);

	// Initialize the matrix data for every instance.
	int m = 0;
	for (int x = 0; x < x_count; x++) {
		for (int y = 0; y < y_count; y++) {

			modelMatrices[m] = translate(view_mtr.model, vec3(step * (x - x_count / 2.0f + 0.5f), step * (y - y_count / 2.0f + 0.5), 0.0f));
			m++;
		}
	}
	
	mat4 tempMat4;
	m = 0;
	int iii = 0;
	double now = clock() / 1000.0f;
	for (int x = 0; x < x_count; x++) {
		for (int y = 0; y < y_count; y++) {

			tempMat4 = rotate(modelMatrices[iii], 1.0f, vec3(sin((x + 0.5f)*now), cos((y + 0.5f)*now), 0.0f));

			tempMat4 *= view_mtr.view;
			tempMat4 *= view_mtr.projection;

			const float *pSource = (const float*)value_ptr(tempMat4);
			for (int j = 0; j < 16; ++j)
				mvpMatricesData[m + j] = pSource[j];

			iii++;
			m += matrix_float_count;
		}
	}


	cube.indexCount = sizeof(indxData)/sizeof(uint16_t);
	cube.instanceCount = cube.indexCount / 3;

	vertBuf = createBuffer(vertData, sizeof(vertData), WGPUBufferUsage_Vertex);
	indxBuf = createBuffer(indxData, sizeof(indxData), WGPUBufferUsage_Index);

	// create the uniform bind group (note 'rotDeg' is copied here, not bound in any way)
	uRotBuf = createBuffer(&rotDeg, sizeof(rotDeg), WGPUBufferUsage_Uniform);

	unifBuf = createBuffer(&mvpMatricesData, uniform_buffer_size, WGPUBufferUsage_Uniform);

	setProjectionAndView();

	uMVPBuf = createBuffer(&view_mtr, sizeof(view_mtr)+256, WGPUBufferUsage_Uniform);

	//WGPUBindGroupEntry bgEntry[2] = {};
	//bgEntry[0].binding = 0;
	//bgEntry[0].buffer = uRotBuf;
	//bgEntry[0].offset = 0;
	//bgEntry[0].size = sizeof(rotDeg);

	//bgEntry[1].binding = 1;
	//bgEntry[1].buffer = uMVPBuf;
	//bgEntry[1].offset = 0;
	//bgEntry[1].size = sizeof(view_mtr);

	//WGPUBindGroupDescriptor bgDesc = {};
	//bgDesc.layout = bindGroupLayout;
	//bgDesc.entryCount = 2;
	//bgDesc.entries = bgEntry;

	//
	WGPUBindGroupEntry bgEntry_inst[1] = {};
	bgEntry_inst[0].binding = 0;
	bgEntry_inst[0].buffer = unifBuf;
	bgEntry_inst[0].offset = 0;
	bgEntry_inst[0].size = matrix_float_count * num_instances * sizeof(float);

	WGPUBindGroupDescriptor bgDesc_inst = {};
	bgDesc_inst.layout = bindGroupLayout;
	bgDesc_inst.entryCount = 1;
	bgDesc_inst.entries = bgEntry_inst;

	bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc_inst);

	// last bit of clean-up
	wgpuBindGroupLayoutRelease(bindGroupLayout);
}


/**
 * Draws using the above pipeline and buffers.
 */
static bool redraw() {
	WGPUTextureView backBufView = wgpuSwapChainGetCurrentTextureView(swapchain);			// create textureView

	WGPURenderPassColorAttachment colorDesc = {};
	colorDesc.view    = backBufView;
	colorDesc.loadOp  = WGPULoadOp_Clear;
	colorDesc.storeOp = WGPUStoreOp_Store;
#ifdef __EMSCRIPTEN__
	// Dawn has both clearValue/clearColor but only Color works; Emscripten only has Value
	colorDesc.clearValue.r = 0.3f;
	colorDesc.clearValue.g = 0.3f;
	colorDesc.clearValue.b = 0.3f;
	colorDesc.clearValue.a = 1.0f;
#else
	colorDesc.clearColor.r = 0.3f;
	colorDesc.clearColor.g = 0.3f;
	colorDesc.clearColor.b = 0.3f;
	colorDesc.clearColor.a = 1.0f;
#endif

	WGPUTextureFormat format = {};
	format = WGPUTextureFormat_Depth24Plus;
	
	WGPUExtent3D size = {};
	size.width = 800;
	size.height = 450;
	size.depthOrArrayLayers = 1;

	WGPUTextureDescriptor depth_texture_desc = {};
	depth_texture_desc.usage = WGPUTextureUsage_RenderAttachment;
	depth_texture_desc.format = format;
	depth_texture_desc.dimension = WGPUTextureDimension_2D;
	depth_texture_desc.mipLevelCount = 1;
	depth_texture_desc.sampleCount = 1;
	depth_texture_desc.size = size;

	WGPUTexture depth_stencil_texture = wgpuDeviceCreateTexture(device, &depth_texture_desc);

	WGPUTextureViewDescriptor depth_texture_view_dec = {};
	depth_texture_view_dec.format = depth_texture_desc.format;
	depth_texture_view_dec.dimension = WGPUTextureViewDimension_2D;
	depth_texture_view_dec.baseMipLevel = 0;
	depth_texture_view_dec.mipLevelCount = 1;
	depth_texture_view_dec.baseArrayLayer = 0;
	depth_texture_view_dec.arrayLayerCount = 1;
	depth_texture_view_dec.aspect = WGPUTextureAspect_All;

	WGPUTextureView depth_stencil_texture_view = wgpuTextureCreateView(depth_stencil_texture, &depth_texture_view_dec);

	WGPURenderPassDepthStencilAttachment depthDesc = {};
	depthDesc.view = depth_stencil_texture_view;
	depthDesc.depthLoadOp = WGPULoadOp_Clear;
	depthDesc.depthStoreOp = WGPUStoreOp_Store;
	depthDesc.depthClearValue = 1.0f;
	depthDesc.clearDepth = 1.0f;
	depthDesc.clearStencil = 0;
	//depthDesc.stencilLoadOp = WGPULoadOp_Clear;
	//depthDesc.stencilStoreOp = WGPUStoreOp_Store;

		
	// set up render pass
	WGPURenderPassDescriptor renderPass = {};
	renderPass.colorAttachmentCount = 1;
	renderPass.colorAttachments = &colorDesc;
	renderPass.depthStencilAttachment = &depthDesc;
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);			// create encoder
	WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPass);	// create pass

	// mvp update
	setProjectionAndView();

	// Rotate 1번째 방법
	//double now = clock()/1000.f;
	//const float sin_now = sin(now);
	//const float cos_now = cos(now);
	//view_mtr.model = rotate(view_mtr.model, 0.1f, vec3(sin_now, cos_now, 0.0f));
	
	// Rotate 2번째 방법
	//rotDeg += 0.2f;
	//wgpuQueueWriteBuffer(queue, uRotBuf, 0, &rotDeg, sizeof(rotDeg));
	wgpuQueueWriteBuffer(queue, uMVPBuf, 0, &view_mtr, sizeof(view_mtr));
	wgpuQueueWriteBuffer(queue, unifBuf, 0, &mvpMatricesData, matrix_float_count * num_instances * sizeof(float));

	// draw the triangle (comment these five lines to simply clear the screen)
	wgpuRenderPassEncoderSetPipeline(pass, pipeline);
	wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, 0);
	wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertBuf, 0, WGPU_WHOLE_SIZE);
	wgpuRenderPassEncoderSetIndexBuffer(pass, indxBuf, WGPUIndexFormat_Uint16, 0, WGPU_WHOLE_SIZE);
	wgpuRenderPassEncoderDrawIndexed(pass, cube.indexCount, cube.instanceCount, 0, 0, 0);

	wgpuRenderPassEncoderEnd(pass);
	wgpuRenderPassEncoderRelease(pass);														// release pass
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);				// create commands
	wgpuCommandEncoderRelease(encoder);														// release encoder

	wgpuQueueSubmit(queue, 1, &commands);
	wgpuCommandBufferRelease(commands);														// release commands
#ifndef __EMSCRIPTEN__
	/*
	 * TODO: wgpuSwapChainPresent is unsupported in Emscripten, so what do we do?
	 */
	wgpuSwapChainPresent(swapchain);
#endif
	wgpuTextureViewRelease(backBufView);													// release textureView

	return true;
}

extern "C" int __main__(int /*argc*/, char* /*argv*/[]) {
	if (window::Handle wHnd = window::create(WINDOW_WIDTH, WINDOW_HEIGHT)) {
		if ((device = webgpu::create(wHnd))) {
			queue = wgpuDeviceGetQueue(device);

			swapchain = webgpu::createSwapChain(device);
			createPipelineAndBuffers();

			window::show(wHnd);
			window::loop(wHnd, redraw);

		#ifndef __EMSCRIPTEN__
			wgpuBindGroupRelease(bindGroup);
			//wgpuBufferRelease(uRotBuf);
			wgpuBufferRelease(unifBuf);
			wgpuBufferRelease(indxBuf);
			wgpuBufferRelease(vertBuf);
			wgpuRenderPipelineRelease(pipeline);
			wgpuSwapChainRelease(swapchain);
			wgpuQueueRelease(queue);
			wgpuDeviceRelease(device);
		#endif
		}
	#ifndef __EMSCRIPTEN__
		window::destroy(wHnd);
	#endif
	}
	return 0;
}
