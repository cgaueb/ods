// Fill out your copyright notice in the Description page of Project Settings.


#include "AbstractSampler.h"
#include "AbstractSamplerComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "HAL/FileManager.h"

static FString true_value = "true";
static FString false_value = "false";

inline auto strb(bool b) { return b ? *true_value : *false_value; }

AAbstractSampler::AAbstractSampler()
{
	AbstractSamplerComponent = CreateDefaultSubobject<UAbstractSamplerComponent>(TEXT("NewAbstractSamplerComponent"));
	AbstractSamplerComponent->SetupAttachment(RootComponent);

	PrimaryActorTick.SetTickFunctionEnable(true);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	bAllowTickBeforeBeginPlay = true;

	SetActorTickEnabled(true);
	SetTickableWhenPaused(true);
	
	// Settings for the post process volume
	//GetCaptureComponent2D()->PostProcessSettings.PathTracingMaxBounces = 1;
	//GetCaptureComponent2D()->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: Rendering is: %s with %d counter"), strb(m_enable_rendering), m_rendering_counter);

	bool ok = IsActorTickEnabled();
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: IsActorTickEnabled is: %s"), strb(ok));
	bool ok2 = CanEverTick();
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: CanEverTick is: %d"), ok2);

	//GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("This message will appear on the screen!"));

	CreateRT();
}

void AAbstractSampler::CreateRT()
{
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: Creating Render Target of size %d"), 512);
	//Unsupported format when creating Texture2D from TextureRenderTarget2D.Supported formats are B8G8R8A8, FloatRGBA and G8.

	//auto SceneCaptureRT = NewObject<UTextureRenderTarget2D>(this, TEXT("AsyncCaptureScene_RT"));
	auto SceneCaptureRT = NewObject<UTextureRenderTarget2D>();
	SceneCaptureRT->RenderTargetFormat = RTF_RGBA8;
	SceneCaptureRT->InitAutoFormat(4, 4);
	SceneCaptureRT->UpdateResourceImmediate(true);

	//GetCaptureComponent2D()->TextureTarget = SceneCaptureRT;
	GetAbstractSamplerComponent()->TextureTarget = SceneCaptureRT;
	
	m_rt = SceneCaptureRT;
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The RT pointer is: %p"), m_rt);

	/*auto textureTarget = GetCaptureComponent2D()->TextureTarget;
	auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_NoFlags, CTF_DeferCompression);
	m_texture2D = imageRendered;*/
}

void AAbstractSampler::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	//UE_LOG(LogTemp, Warning, TEXT("The Actor's name is %s"), *YourActor->GetName());	
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: UpdateSceneCaptureContents with %d"), m_rendering_counter);

	GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("This message will appear on the screen!"));
}

void AAbstractSampler::Tick(float deltaSeconds)
{
	//UE_LOG(LogTemp, Warning, TEXT("The Actor's name is %s"), *YourActor->GetName());	
	UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The Tick value is: %f"), deltaSeconds);

	GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("This message will appear on the screen!"));
}


void AAbstractSampler::TickActor
(
	float DeltaTime,
	enum ELevelTick TickType,
	FActorTickFunction& ThisTickFunction
)
{
	//Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	//UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The TickActor value is: %f"), DeltaTime);
	
	if (GetAbstractSamplerComponent()->TextureTarget == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ViewSampler: The Tick pointer value of RT is null"));
		CreateRT();
		return;
	}
#if 0
	m_rendering_counter++;
	if (m_enable_rendering && m_rendering_counter % 100 == 0)
	{
		m_rendering_counter = 0;

		auto textureTarget = GetAbstractSamplerComponent()->TextureTarget;
		auto imageRendered = textureTarget->ConstructTexture2D(this, "CameraImage", EObjectFlags::RF_NoFlags, CTF_DeferCompression);
		//m_texture2D = imageRendered;

		FTexture2DMipMap* topMipMap = &imageRendered->PlatformData->Mips[0];
		FByteBulkData* RawImage = &topMipMap->BulkData;

		unsigned char* dataPointer = (unsigned char*)RawImage->Lock(LOCK_READ_ONLY);

		float red(0), green(0), blue(0), alpha(0);
		for (int i = 0; i < 100*512; ++i)
		{
			auto r = dataPointer[4 * i + 0];
			auto g = dataPointer[4 * i + 1];
			auto b = dataPointer[4 * i + 2];
			auto a = dataPointer[4 * i + 3];

			red += r / 255.0f;
			green += g / 255.0f;
			blue += b / 255.0f;
			alpha += a / 255.0f;
		}

		red /= 512 * 512;
		green /= 512 * 512;
		blue /= 512 * 512;
		alpha /= 512 * 512;		

		UE_LOG(LogTemp, Warning, TEXT("ViewSampler: %.3f %.3f %.3f %.3f"), red, green, blue, alpha);

		RawImage->Unlock();
	}
#endif
}

// This ultimately is what controls whether or not it can even tick at all in the editor view port. 
//But, it is EVERY view port so it still needs to be blocked from preview windows and junk.
bool AAbstractSampler::ShouldTickIfViewportsOnly() const
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


/*

UWorld* World = ViewCamera->GetWorld();
	SceneCapture = World->SpawnActor<ASceneCapture2D>(SceneCaptureClass, CaptureLocation, CaptureRotation);
	if (SceneCapture)
	{
		USceneCaptureComponent2D* CaptureComponent = SceneCapture->GetCaptureComponent2D();

		if (CaptureComponent->TextureTarget == nullptr)
		{
			SceneCaptureRT = NewObject<UTextureRenderTarget2D>(this, TEXT("AsyncCaptureScene_RT"), RF_Transient);
			SceneCaptureRT->RenderTargetFormat = RTF_RGBA8_SRGB;
			SceneCaptureRT->InitAutoFormat(ResX, ResY);
			SceneCaptureRT->UpdateResourceImmediate(true);

			CaptureComponent->TextureTarget = SceneCaptureRT;
		}
		else
		{
			SceneCaptureRT = CaptureComponent->TextureTarget;
		}

		FMinimalViewInfo CaptureView;
		ViewCamera->GetCameraView(0, CaptureView);
		CaptureComponent->SetCameraView(CaptureView);
	}
*/