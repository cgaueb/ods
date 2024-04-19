// Fill out your copyright notice in the Description page of Project Settings.
#include "ViewSampler.h"

#include "ViewSampler.h"
#include "OpeningEngine.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"

static FString true_value = "true";
static FString false_value = "false";

inline auto strb(bool b) { return b ? *true_value : *false_value; }

#define FRUSTUM_BLOCK_SIZE 16
#define COMPUTE_AVG_PER_BLOCK
//#define DEBUG_OUT
//#define DEBUG_EXEC

AViewSampler::AViewSampler()
{
	PrimaryActorTick.SetTickFunctionEnable(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	bAllowTickBeforeBeginPlay = true;
	SetActorTickEnabled(true);
	SetTickableWhenPaused(true);
	
	// Settings for path tracing
	GetCaptureComponent2D()->bUseRayTracingIfEnabled = 1;
	GetCaptureComponent2D()->PostProcessSettings.PathTracingMaxBounces = 1;
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
	CreateRT();
}

void AViewSampler::CreateRT()
{
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: Creating Render Target of resolution %d"), FRUSTUM_BLOCK_SIZE);
	
	//auto SceneCaptureRT = NewObject<UTextureRenderTarget2D>(this, TEXT("AsyncCaptureScene_RT"));
	auto SceneCaptureRT = NewObject<UTextureRenderTarget2D>();
	SceneCaptureRT->RenderTargetFormat = RTF_RGBA16f;
	SceneCaptureRT->InitCustomFormat(FRUSTUM_BLOCK_SIZE, FRUSTUM_BLOCK_SIZE, PF_FloatRGBA, true);
	SceneCaptureRT->UpdateResourceImmediate(true);

	GetCaptureComponent2D()->TextureTarget = SceneCaptureRT;
	m_rt = SceneCaptureRT;
	UE_LOG(LogTemp, Display, TEXT("ViewSampler: The RT pointer is: %p"), m_rt);

	/*auto textureTarget = GetCaptureComponent2D()->TextureTarget;
	auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_NoFlags, CTF_DeferCompression);
	m_texture2D = imageRendered;*/
}

void AViewSampler::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: UpdateSceneCaptureContents with %d"), m_rendering_counter);

	GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("This message will appear on the screen!"));
}

void AViewSampler::Tick(float deltaSeconds)
{	
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The Tick value is: %f"), deltaSeconds);

	GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("This message will appear on the screen!"));
}


void AViewSampler::TickActor
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
		UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The Tick pointer value of RT is null"));
		CreateRT();
		return;
	}

	TArray<AOpeningEngine*> OpeningEngines;
	FindAllActors(GetWorld(), OpeningEngines);
	if (OpeningEngines.Num() == 0) return;
	AOpeningEngine* OpeningEngine = OpeningEngines[0];
	if (AOpeningEngine::LOTUS_STAGE_VIEW_SAMPLERS != OpeningEngine->GetStage()) return;
	
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
		UE_LOG(LogTemp, Warning, TEXT("ViewSampler %s rendering time: %.2f - spp: %d"), *GetName(), m_per_call_rendering_time_ms, pt_index);
	}
#endif

	if (m_should_reset) m_should_reset = false;

	m_rendering_counter++; // it is unsigned so it will wrap around

	// Deprecated
	if (m_enable_rendering && m_rendering_counter % 100 == 0)
	{
		m_rendering_counter = 0;

		auto textureTarget = GetCaptureComponent2D()->TextureTarget;
		auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_NoFlags, CTF_DeferCompression);

		FTexture2DMipMap* topMipMap = &imageRendered->GetPlatformData()->Mips[0];
		FByteBulkData* RawImage = &topMipMap->BulkData;

		/*unsigned char* dataPointer = (unsigned char*)RawImage->Lock(LOCK_READ_ONLY);

		float red(0), green(0), blue(0), alpha(0);
		for (int i = 0; i < SAMPLER_RESOLUTION * SAMPLER_RESOLUTION; ++i)
		{
			auto r = dataPointer[4 * i + 0];
			auto g = dataPointer[4 * i + 1];
			auto b = dataPointer[4 * i + 2];
			auto a = dataPointer[4 * i + 3];

			if (imageRendered->GetPixelFormat(0) == PF_B8G8R8A8)
			{
				std::swap(r, b);
			}
			else if(imageRendered->GetPixelFormat(0) == PF_R8G8B8A8)
			{

			}
			else if (imageRendered->GetPixelFormat(0) == PF_A8R8G8B8)
			{
				std::swap(b, a);
				std::swap(g, b);
				std::swap(r, g);
			}			

			red += r / 255.0f;
			green += g / 255.0f;
			blue += b / 255.0f;
			alpha += a / 255.0f;
		}*/

		FFloat16* dataPointer = (FFloat16*)RawImage->Lock(LOCK_READ_ONLY);

		FVector3f average(0, 0, 0);
		FVector3f maxColor(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		FVector3f minColor(FLT_MAX, FLT_MAX, FLT_MAX);
		for (int i = 0; i < FRUSTUM_BLOCK_SIZE * FRUSTUM_BLOCK_SIZE; ++i)
		{
			auto r = dataPointer[4 * i + 0];
			auto g = dataPointer[4 * i + 1];
			auto b = dataPointer[4 * i + 2];

			FVector3f value(r, g, b);
			average += value;
			maxColor = FVector3f::Max(value, maxColor);
			minColor = FVector3f::Max(value, minColor);
		}
		average /= FVector3f(FRUSTUM_BLOCK_SIZE * FRUSTUM_BLOCK_SIZE);

		UE_LOG(LogTemp, Warning, TEXT("ViewSampler: %.3f %.3f %.3f"), average.X, average.Y, average.Z);

		RawImage->Unlock();
	}
}

AViewSampler::RetColorStats AViewSampler::GetColor()
{
	RetColorStats stats;
	stats.valid = false;
	if (GetCaptureComponent2D()->TextureTarget == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The Tick pointer value of RT is null"));
		CreateRT();
		return stats;
	}
	
	auto textureTarget = GetCaptureComponent2D()->TextureTarget;
	auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_Transient, CTF_DeferCompression);
	//m_texture2D = imageRendered;

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

#ifndef COMPUTE_AVG_PER_BLOCK
	for (int i = 0; i < SAMPLER_RESOLUTION * SAMPLER_RESOLUTION; ++i)
	{
		auto r = dataPointer[4 * i + 0];
		auto g = dataPointer[4 * i + 1];
		auto b = dataPointer[4 * i + 2];

		FVector3f value(r, g, b);

		stats.average += value;
		stats.maxValue = FVector3f::Max(value, stats.maxValue);
		stats.minValue = FVector3f::Max(value, stats.minValue);
	}
	stats.average /= FVector3f(SAMPLER_RESOLUTION * SAMPLER_RESOLUTION);
#elif 0 // Used when we do the averaging
	constexpr int num_blocks_x = SAMPLER_RESOLUTION / FRUSTUM_BLOCK_SIZE;
	constexpr int num_blocks_y = SAMPLER_RESOLUTION / FRUSTUM_BLOCK_SIZE;
	for (int blockX = 0; blockX < num_blocks_x; blockX++)
	{
		for (int blockY = 0; blockY < num_blocks_y; blockY++)
		{
			FVector3f average(0, 0, 0);
			for (int y = 0; y < FRUSTUM_BLOCK_SIZE; ++y)
			{
				int offset_y = (blockY * FRUSTUM_BLOCK_SIZE + y) * SAMPLER_RESOLUTION;
				for (int x = 0; x < FRUSTUM_BLOCK_SIZE; ++x)
				{
					int i = offset_y + blockX * FRUSTUM_BLOCK_SIZE + x;

					auto r = dataPointer[4 * i + 0];
					auto g = dataPointer[4 * i + 1];
					auto b = dataPointer[4 * i + 2];

					FVector3f value(r, g, b);
					average += value;
					stats.average += value;
				}
			}
			stats.maxValue = FVector3f::Max(average, stats.maxValue);
			stats.minValue = FVector3f::Max(average, stats.minValue);
		}
	}

	stats.average /= FVector3f(SAMPLER_RESOLUTION * SAMPLER_RESOLUTION);
#else
	FVector3f average(0, 0, 0);
	FVector3f maxColor(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	FVector3f minColor(FLT_MAX, FLT_MAX, FLT_MAX);
	for (int i = 0; i < FRUSTUM_BLOCK_SIZE * FRUSTUM_BLOCK_SIZE; ++i)
	{
		auto r = dataPointer[4 * i + 0];
		auto g = dataPointer[4 * i + 1];
		auto b = dataPointer[4 * i + 2];

		FVector3f value(r, g, b);
		//value *= m_light_efficacy;
		stats.average += value;
		stats.maxValue = FVector3f::Max(value, stats.maxValue);
		stats.minValue = FVector3f::Max(value, stats.minValue);
	}
	stats.average /= FVector3f(FRUSTUM_BLOCK_SIZE * FRUSTUM_BLOCK_SIZE);

#endif

	stats.valid = true;

#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: %.3f %.3f %.3f %d"), stats.average.X, stats.average.Y, stats.average.Z, m_rendering_counter);
#endif

	RawImage->Unlock();
	
	return stats;
}

// This ultimately is what controls whether or not it can even tick at all in the editor view port. 
//But, it is EVERY view port so it still needs to be blocked from preview windows and junk.
bool AViewSampler::ShouldTickIfViewportsOnly() const
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