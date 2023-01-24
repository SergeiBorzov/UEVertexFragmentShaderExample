#include "TestActor.h"

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "UObject/UObjectGlobals.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphEvent.h"
#include "RenderTargetPool.h"
#include "PixelShaderUtils.h"

#include "Engine/TextureRenderTarget2D.h"


class FSimpleVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSimpleVS);
	SHADER_USE_PARAMETER_STRUCT(FSimpleVS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSimpleVS, "/CustomShaders/TestShader.usf", "MainVS", SF_Vertex);

class FSimplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSimplePS);
	SHADER_USE_PARAMETER_STRUCT(FSimplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector4f, StartColor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSimplePS, "/CustomShaders/TestShader.usf", "MainPS", SF_Pixel);

/* Sergei: This will be useful for custom vertex declaration

struct MyVertex {
	FVector4 Position;
};

class MyVertexDeclaration : public FRenderResource {
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~MyVertexDeclaration() {}

	virtual void InitRHI() {
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(MyVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(MyVertex, Position), VET_Float4, 0, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() {
		VertexDeclarationRHI.SafeRelease();
	}
};
*/

class FMyVertexBuffer : public FVertexBuffer
{
public:
	virtual void InitRHI() override
	{
		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo(TEXT("FClearVertexBuffer"));
		VertexBufferRHI = RHICreateVertexBuffer(sizeof(FVector4f) * 4, BUF_Static, CreateInfo);
		void* VoidPtr = RHILockBuffer(VertexBufferRHI, 0, sizeof(FVector4f) * 4, RLM_WriteOnly);
		// Generate the vertices used
		FVector4f* Vertices = reinterpret_cast<FVector4f*>(VoidPtr);
		Vertices[0] = FVector4f(-1.0f, -1.0f, 0.0f, 1.0f);
		Vertices[1] = FVector4f(0.0f, 1.0f, 0.0f, 1.0f);
		Vertices[2] = FVector4f(1.0f, -1.0f, 0.0f, 1.0f);
		RHIUnlockBuffer(VertexBufferRHI);
	}
};
TGlobalResource<FMyVertexBuffer> GMyVertexBuffer;

ATestActor::ATestActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATestActor::BeginPlay()
{
	Super::BeginPlay();
	const FString RenderTargetName = "/Game/TestRenderTarget.TestRenderTarget";
	RenderTarget2D = LoadObject<UTextureRenderTarget2D>(nullptr, *RenderTargetName);
	check(RenderTarget2D);
}



void ATestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ENQUEUE_RENDER_COMMAND(ExecTestShader)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			this->ExecuteTestShader_RenderThread(RHICmdList);
		}
	);
}

void ATestActor::ExecuteTestShader_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());
	FRDGBuilder GraphBuilder(RHICmdList);

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSimplePS> PixelShader(ShaderMap);
	TShaderMapRef<FSimpleVS> VertexShader(ShaderMap);

	// Registering render target as external texture for RDG
	FTexture2DRHIRef RenderTargetRHI = RenderTarget2D->GetResource()->GetTexture2DRHI();
	FRDGTextureRef RenderTargetTexture = RegisterExternalTexture(GraphBuilder, RenderTargetRHI, TEXT("RT_CameraView_GraphBuilder"));

	// Filling fragment shader parameters
	FSimplePS::FParameters* PixelPassParameters = GraphBuilder.AllocParameters<FSimplePS::FParameters>();
	PixelPassParameters->StartColor = FLinearColor(0.0f, 0.9f, 0.5f, 1.0f);
	PixelPassParameters->RenderTargets[0] = FRenderTargetBinding(RenderTargetTexture, ERenderTargetLoadAction::EClear);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Raster pass"),
		PixelPassParameters,
		ERDGPassFlags::Raster,
		[PixelPassParameters, VertexShader, PixelShader](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(0, 0, 0, 512, 512, 0);
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI(); // Default blending state
			// Sergei: Currently Culling is None (CM_None), Switch this to CW or CCW, 
			// depending on how you will define triangles to enable backface culling
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			// Sergei: Currently depth testing is disabled, enable it and use Less option
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			// Sergei: This has to be switched to yours Vertex Shader Declaration,
			// Unreal stuff that is similar to glVertexAttribPointer
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			// Sergei: Apply pipeline state
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PixelPassParameters);
			// Sergei: Don't use global vertex buffer
			RHICmdList.SetStreamSource(0, GMyVertexBuffer.VertexBufferRHI, 0);
			// Sergei: Here use RHICmdList.DrawIndexedPrimitive instead
			// For that you will need to create index buffer
			RHICmdList.DrawPrimitive(0, 1, 1); 
		});
	GraphBuilder.Execute();
}
