// Fill out your copyright notice in the Description page of Project Settings.
#include "PlanarSampler.h"

#include "PlanarSampler.h"
#include "OpeningEngine.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "MathUtil.h"

#include <chrono>

#define SAMPLER_RESOLUTION 64
//#define DEBUG_OUT
//#define DEBUG_EXEC

APlanarSampler::APlanarSampler()
{
	// Enable Tick in editor
	PrimaryActorTick.SetTickFunctionEnable(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bAllowTickBeforeBeginPlay = true;
	SetActorTickEnabled(true);
	SetTickableWhenPaused(true);

	/*FVector X(1, 0, 0);
	FVector Y(0, 1, 0);
	FVector Z(0, 0, 1);	
	FTransform b(-Z, -Y, -X, FVector(10, 0, 0));*/

	//StaticMeshComponent = NewObject<UStaticMeshComponent>();
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("PlanarStaticMesh", true);
	StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	StaticMeshComponent->AddLocalTransform(FTransform(FRotator3d(90, 0, 0), FVector(10, 0, 0), FVector(1, 1, 1)));
	//StaticMeshComponent->AddLocalTransform(FTransform(FRotator3d(90, -90, 90), FVector(10, 0, 0), FVector(1, 1, 1)));	
	//StaticMeshComponent->AddLocalTransform(b);
	
	auto MeshAsset = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
	auto MaterialAsset = ConstructorHelpers::FObjectFinder<UMaterialInterface>(TEXT("Matarial'/Game/Materials/LightingDesign/WhiteDiffuse.WhiteDiffuse'"));
	auto PreviewMaterialAsset = ConstructorHelpers::FObjectFinder<UMaterialInterface>(TEXT("Matarial'/Game/Materials/LightingDesign/PlanarSampler_Preview.PlanarSampler_Preview'"));

	GatherMaterial = MaterialAsset.Object;
	PreviewMaterial = PreviewMaterialAsset.Object;

	if (MeshAsset.Object != nullptr && MaterialAsset.Object != nullptr)
	{
		StaticMeshComponent->SetStaticMesh(MeshAsset.Object);
		StaticMeshComponent->SetMaterial(0, MaterialAsset.Object);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PlanarSampler: Could not find shape"));
	}

	GetCaptureComponent2D()->ProjectionType = ECameraProjectionMode::Orthographic;
	GetCaptureComponent2D()->OrthoWidth = 100; // set equal to the plane size

	CreateRT();

	// Settings for path tracing
	GetCaptureComponent2D()->bUseRayTracingIfEnabled = 1;
	GetCaptureComponent2D()->PostProcessSettings.PathTracingMaxBounces = 3;
	GetCaptureComponent2D()->PostProcessSettings.PathTracingEnableDenoiser = 1;
	GetCaptureComponent2D()->PostProcessSettings.PathTracingSamplesPerPixel = m_spp;
	GetCaptureComponent2D()->PostProcessSettings.PathTracingEnableEmissive = 0;

	GetCaptureComponent2D()->PostProcessSettings.AutoExposureMaxBrightness = 1.0;
	GetCaptureComponent2D()->PostProcessSettings.AutoExposureMinBrightness = 1.0;
	GetCaptureComponent2D()->PostProcessSettings.bOverride_PathTracingMaxBounces = 1;
	GetCaptureComponent2D()->PostProcessSettings.bOverride_PathTracingSamplesPerPixel = 1;
	GetCaptureComponent2D()->PostProcessSettings.bOverride_PathTracingEnableDenoiser = 1;
	GetCaptureComponent2D()->PostProcessSettings.bOverride_AutoExposureMaxBrightness = 1;
	GetCaptureComponent2D()->PostProcessSettings.bOverride_AutoExposureMinBrightness = 1;
	GetCaptureComponent2D()->PostProcessSettings.bOverride_PathTracingEnableEmissive = 1;

	GetCaptureComponent2D()->bUseRayTracingIfEnabled = 1;
	GetCaptureComponent2D()->bAlwaysPersistRenderingState = 1;

	GetCaptureComponent2D()->ShowFlags.SetPathTracing(1);
	GetCaptureComponent2D()->ShowFlags.SetLensFlares(0);
	GetCaptureComponent2D()->ShowFlags.SetLocalExposure(0);
	GetCaptureComponent2D()->ShowFlags.SetPostProcessing(0);
	GetCaptureComponent2D()->ShowFlags.SetLocalExposure(0);

	// capture properties
	GetCaptureComponent2D()->bCaptureEveryFrame = 0;
	GetCaptureComponent2D()->bCaptureOnMovement = 0;

	m_per_call_rendering_time_ms = 0.f;
}

void APlanarSampler::Tick(float deltaSeconds)
{
#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("The Actor's name is %s"), *GetName());	
	UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: The Tick value is: %f"), deltaSeconds);
	GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("This message will appear on the screen!"));
#endif
}


void APlanarSampler::TickActor
(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorTickFunction& ThisTickFunction
)
{
	//Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	//UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The TickActor value is: %f"), DeltaTime);

	if (GetCaptureComponent2D()->TextureTarget == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: The Tick pointer value of RT is null"));
		CreateRT();
		return;
	}

	TArray<AOpeningEngine*> OpeningEngines;
	FindAllActors(GetWorld(), OpeningEngines);
	if (OpeningEngines.Num() == 0) return;
	AOpeningEngine* OpeningEngine = OpeningEngines[0];
	if (AOpeningEngine::LOTUS_STAGE_PLANAR_SAMPLERS != OpeningEngine->GetStage()) return;

	GetCaptureComponent2D()->bCameraCutThisFrame = m_should_reset;

#ifdef DEBUG_EXEC
	double start = FPlatformTime::Seconds() * 1000;
#endif

	GetCaptureComponent2D()->CaptureScene();

#ifdef DEBUG_EXEC
	FlushRenderingCommands();
	double end = FPlatformTime::Seconds() * 1000;
	m_per_call_rendering_time_ms = end - start;
#endif

	const auto View = GetCaptureComponent2D()->GetViewState(0);
	const auto pt_index = View->GetPathTracingSampleIndex();
	m_rendering_done = (pt_index == m_spp) && !m_should_reset;

#ifdef DEBUG_EXEC
	if (m_rendering_done)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlanarSampler %s rendering time: %.2f - spp: %d"), *GetName(), m_per_call_rendering_time_ms, pt_index);
	}
#endif

	if (m_should_reset) m_should_reset = false;

	if (m_show_preview != m_current_show_preview)
	{
		m_current_show_preview = m_show_preview;

		if (GatherMaterial != nullptr && PreviewMaterial != nullptr)
		{
			if (m_show_preview)
			{
				auto textureTarget = GetCaptureComponent2D()->TextureTarget;
				auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_NoFlags, CTF_DeferCompression);
				PreviewMaterial->GetMaterial()->SetTextureParameterValueEditorOnly("Preview", imageRendered);
				StaticMeshComponent->SetMaterial(0, PreviewMaterial);
			}
			else
			{
				StaticMeshComponent->SetMaterial(0, GatherMaterial);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PlanarSampler: Could not find Materials"));
		}
	}

	m_rendering_counter++; // it is unsigned so it will wrap around

	// Used for preview only
	if (m_enable_rendering && m_rendering_counter % 100 == 0)
	{	
		auto textureTarget = GetCaptureComponent2D()->TextureTarget;
		auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_NoFlags, CTF_DeferCompression);
		
		FTexture2DMipMap* topMipMap = &imageRendered->GetPlatformData()->Mips[0];
		int num_mips = imageRendered->GetNumMips();
		FByteBulkData* RawImage = &topMipMap->BulkData;

		FFloat16* dataPointer = (FFloat16*)RawImage->Lock(LOCK_READ_ONLY);
		FDateTime StartTime = FDateTime::UtcNow();
		float red(0), green(0), blue(0), alpha(0);
		for (int i = 0; i < SAMPLER_RESOLUTION * SAMPLER_RESOLUTION; ++i)
		{
			auto r = dataPointer[4 * i + 0];
			auto g = dataPointer[4 * i + 1];
			auto b = dataPointer[4 * i + 2];
			auto a = dataPointer[4 * i + 3];

			red += r * m_light_efficacy;
			green += g * m_light_efficacy;
			blue += b * m_light_efficacy;
			alpha += a;			
		}

		red /= SAMPLER_RESOLUTION * SAMPLER_RESOLUTION;
		green /= SAMPLER_RESOLUTION * SAMPLER_RESOLUTION;
		blue /= SAMPLER_RESOLUTION * SAMPLER_RESOLUTION;
		alpha /= SAMPLER_RESOLUTION * SAMPLER_RESOLUTION;

		float TimeElapsedInMs = (FDateTime::UtcNow() - StartTime).GetTotalMilliseconds();

#ifdef DEBUG_OUT
		UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: %.3f %.3f %.3f %.3f %d at %.2f ms"), red, green, blue, alpha, m_rendering_counter, TimeElapsedInMs);
#endif
		RawImage->Unlock();
	}
}

// This ultimately is what controls whether or not it can even tick at all in the editor view port. 
//But, it is EVERY view port so it still needs to be blocked from preview windows and junk.
bool APlanarSampler::ShouldTickIfViewportsOnly() const
{
	if (GetWorld() != nullptr && GetWorld()->WorldType == EWorldType::Editor)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void APlanarSampler::CreateRT()
{
	UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: Creating Render Target of size %d"), SAMPLER_RESOLUTION);
	//Unsupported format when creating Texture2D from TextureRenderTarget2D.Supported formats are B8G8R8A8, FloatRGBA and G8.

	//auto SceneCaptureRT = NewObject<UTextureRenderTarget2D>(this, TEXT("AsyncCaptureScene_RT"));
	auto SceneCaptureRT = NewObject<UTextureRenderTarget2D>();
	SceneCaptureRT->RenderTargetFormat = RTF_RGBA16f;
	SceneCaptureRT->InitCustomFormat(SAMPLER_RESOLUTION, SAMPLER_RESOLUTION, PF_FloatRGBA, true);
	SceneCaptureRT->UpdateResourceImmediate(true);

	GetCaptureComponent2D()->TextureTarget = SceneCaptureRT;
	m_rt = SceneCaptureRT;
}

APlanarSampler::RetColorStats APlanarSampler::GetColor()
{
	RetColorStats stats;
	stats.valid = false;
	if (GetCaptureComponent2D()->TextureTarget == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: The Tick pointer value of RT is null"));
		CreateRT();
		return stats;
	}

	auto textureTarget = GetCaptureComponent2D()->TextureTarget;
	auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_Transient, CTF_DeferCompression);

	FTexture2DMipMap* topMipMap = &imageRendered->GetPlatformData()->Mips[0];
	int num_mips = imageRendered->GetNumMips();
	FByteBulkData* RawImage = &topMipMap->BulkData;
	/*if (!topMipMap->DerivedData.HasData())
	{
		UE_LOG(LogTemp, Error, TEXT("ViewSampler: No Data %d %d vs %d %d"), imageRendered->GetSizeX(), imageRendered->GetSizeY(), textureTarget->SizeX, textureTarget->SizeY);
		UE_LOG(LogTemp, Error, TEXT("ViewSampler: %d"), num_mips);
		return stats;
	}*/

	FFloat16* dataPointer = (FFloat16*)RawImage->Lock(LOCK_READ_ONLY);

	for (int i = 0; i < SAMPLER_RESOLUTION * SAMPLER_RESOLUTION; ++i)
	{
		auto r = dataPointer[4 * i + 0];
		auto g = dataPointer[4 * i + 1];
		auto b = dataPointer[4 * i + 2];

		FVector3f value(r, g, b);
		value *= m_light_efficacy;

		stats.illuminance += value;
		stats.min_luminance = FVector3f::Min(stats.min_luminance, value);
		stats.max_luminance = FVector3f::Max(stats.max_luminance, value);
	}

	stats.illuminance /= FVector3f(SAMPLER_RESOLUTION * SAMPLER_RESOLUTION);
	stats.illuminance *= TMathUtilConstants<float>::Pi;
	float red = stats.illuminance.X;
	float green = stats.illuminance.Y;
	float blue = stats.illuminance.Z;
	
	stats.valid = true;

#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: Illum (lux) %.3f %.3f %.3f %d"), red, green, blue, m_rendering_counter);
	UE_LOG(LogTemp, Warning, TEXT("PlanarSampler: Min (nits) %.3f %.3f %.3f"), stats.min_luminance.X, stats.min_luminance.Y, stats.min_luminance.Z);
#endif

	RawImage->Unlock();

	return stats;
}

