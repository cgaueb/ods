// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbstractSamplerComponent.h"

#include "Components/SceneCaptureComponent.h"
#include "Misc/ScopeLock.h"
#include "UObject/RenderingObjectVersion.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Actor.h"
#include "RenderingThread.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Components/BillboardComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "Engine/StaticMesh.h"
#include "Engine/SceneCapture.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCaptureCube.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Components/DrawFrustumComponent.h"
#include "Engine/PlanarReflection.h"
#include "Components/PlanarReflectionComponent.h"
#include "PlanarReflectionSceneProxy.h"
#include "Components/BoxComponent.h"
#include "Logging/MessageLog.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneCaptureRendering.h"
#include "Containers/ArrayView.h"
#include "Misc/MemStack.h"
#include "EngineDefines.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "Engine/Scene.h"
#include "SceneInterface.h"
#include "LegacyScreenPercentageDriver.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "Shader.h"
#include "TextureResource.h"
#include "SceneUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "SceneRenderTargetParameters.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "RendererModule.h"
#include "Rendering/MotionVectorSimulation.h"
#include "SceneViewExtension.h"
#include "GenerateMips.h"

#define LOCTEXT_NAMESPACE "ViewSamplerComponent"

UAbstractSamplerComponent::UAbstractSamplerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;

	bUseRayTracingIfEnabled = 1;
	ShowFlags.SetPathTracing(1);

	// Tick in the editor so that bCaptureEveryFrame preview works
	bTickInEditor = true;

	// Legacy initialization.
	{
		// previous behavior was to capture 2d scene captures before cube scene captures.
		CaptureSortPriority = 1;

		// previous behavior was not exposing MotionBlur and Temporal AA in scene capture 2d.
		ShowFlags.TemporalAA = false;
		ShowFlags.MotionBlur = false;

#if WITH_EDITORONLY_DATA
		if (!IsRunningCommandlet())
		{
			static ConstructorHelpers::FObjectFinder<UStaticMesh> EditorMesh(TEXT("/Engine/EditorMeshes/MatineeCam_SM"));
			CaptureMesh = EditorMesh.Object;
		}
#endif
	}
}

void UAbstractSamplerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// The function that draw a shader into a given RenderGraph texture
template<typename TShaderParameters, typename TShaderClassVertex, typename TShaderClassPixel>
inline void DrawShaderPass(
	FRDGBuilder& GraphBuilder,
	const FString& PassName,
	TShaderParameters* PassParameters,
	TShaderMapRef<TShaderClassVertex> VertexShader,
	TShaderMapRef<TShaderClassPixel> PixelShader,
	FRHIBlendState* BlendState,
	const FIntRect& Viewport
)
{
	const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState);

	GraphBuilder.AddPass(
		FRDGEventName(TEXT("%s"), *PassName),
		PassParameters,
		ERDGPassFlags::Raster,
		[PixelShader, PassParameters, Viewport, PipelineState](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.SetViewport(
			Viewport.Min.X, Viewport.Min.Y, 0.0f,
			Viewport.Max.X, Viewport.Max.Y, 1.0f
		);

		SetScreenPassPipelineState(RHICmdList, PipelineState);

		SetShaderParameters(
			RHICmdList,
			PixelShader,
			PixelShader.GetPixelShader(),
			*PassParameters
		);

		DrawRectangle(
			RHICmdList,                             // FRHICommandList
			0.0f, 0.0f,                             // float X, float Y
			Viewport.Width(), Viewport.Height(),  // float SizeX, float SizeY
			Viewport.Min.X, Viewport.Min.Y,     // float U, float V
			Viewport.Width(),                       // float SizeU
			Viewport.Height(),                      // float SizeV
			Viewport.Size(),                        // FIntPoint TargetSize
			Viewport.Size(),                        // FIntPoint TextureSize
			PipelineState.VertexShader,             // const TShaderRefBase VertexShader
			EDrawRectangleFlags::EDRF_Default       // EDrawRectangleFlags Flags
		);
	});
}


class FLightSamplingTechnique : public FSceneViewExtensionBase
{
public:
	FLightSamplingTechnique(const FAutoRegister& AutoRegister, UAbstractSamplerComponent* ViewComponent);

	//~ Begin FSceneViewExtensionBase Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	//~ End FSceneViewExtensionBase Interface

private:

	bool bAreShadersCompiled = false;
	UAbstractSamplerComponent* ViewComponent = nullptr;

};

FLightSamplingTechnique::FLightSamplingTechnique(const FAutoRegister& AutoRegister, UAbstractSamplerComponent* ViewComponentPtr)
	: FSceneViewExtensionBase(AutoRegister)
{
	ViewComponent = ViewComponentPtr;
}

void FLightSamplingTechnique::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) {
	if (!InView.bIsSceneCapture || !InView.bSceneCaptureUsesRayTracing) return;
	if (!InViewFamily.RenderTarget->GetRenderTargetTexture()) return;
	if (!ViewComponent->TextureTarget) return;
	const auto view_name = InViewFamily.RenderTarget->GetRenderTargetTexture().GetReference()->GetName();
	const auto comp_name = ViewComponent->TextureTarget->GetName();
	if (view_name != *comp_name) return;

	UE_LOG(LogTemp, Warning, TEXT("Ours"));
	auto Ours = FSceneView(InView);
	ViewComponent->View = &InView;
	//InView.Info
	return;
}

void FLightSamplingTechnique::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) {
	if (!View.bIsSceneCapture || !View.bSceneCaptureUsesRayTracing) return;
	if (!ViewComponent->TextureTarget) return;
	const auto view_name = View.Family->RenderTarget->GetRenderTargetTexture().GetReference()->GetName();
	const auto comp_name = ViewComponent->TextureTarget->GetName();
	if (view_name != *comp_name) return;

	//const FViewInfo* ViewInfo = static_cast<const FViewInfo*>(View.Family->Views[0]);
	//const auto FeatureLevel = InView.Family->GetFeatureLevel();
	//const auto ViewInfo = FViewInfo(InView.SceneViewInitOptions);

	//RenderPathTracing(GraphBuilder, *ViewInfo);
}

void FLightSamplingTechnique::SetupViewFamily(FSceneViewFamily& InViewFamily) {
	//UE_LOG(LogTemp, Warning, TEXT("SetupViewFamily"));
}

void FLightSamplingTechnique::BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {
	//UE_LOG(LogTemp, Warning, TEXT("BeginRenderViewFamily"));
}

void FLightSamplingTechnique::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {
	//UE_LOG(LogTemp, Warning, TEXT("PreRenderViewFamily_RenderThread"));
}

void FLightSamplingTechnique::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {
	//UE_LOG(LogTemp, Warning, TEXT("PreRenderView_RenderThread"));
}

void FLightSamplingTechnique::PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {
	
}

void UAbstractSamplerComponent::UpdateSceneCaptureContents(FSceneInterface* Scene)
{

	USceneCaptureComponent2D::UpdateSceneCaptureContents(Scene);
	auto RenderTextureTarget = TextureTarget;
	if (!bExtensionRegistered) {
		LightSamplingExtension = FSceneViewExtensions::NewExtension<FLightSamplingTechnique>(this);
		bExtensionRegistered = true;
	}
	auto RenderScene = Scene->GetRenderScene();
	auto Skylight = RenderScene->SkyLight;

	if (!Skylight) return;
	if (!View) return;
}

#undef LOCTEXT_NAMESPACE