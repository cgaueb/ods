// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "Engine/BlendableInterface.h"
#include "Engine/SceneCapture.h"
#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/Actor.h"

#include "AbstractSamplerComponent.generated.h"

class ISceneViewExtension;
class FSceneInterface;

/**
 *	Used to capture a 'snapshot' of the scene from a single plane and feed it to a render target.
 */
UCLASS(hidecategories = (Collision, Object, Physics, SceneComponent), ClassGroup = Rendering, editinlinenew, meta = (BlueprintSpawnableComponent))
class LOTUSTESTBED_API UAbstractSamplerComponent : public USceneCaptureComponent2D
{
	GENERATED_UCLASS_BODY()

public:

	TSharedPtr<class FLightSamplingTechnique, ESPMode::ThreadSafe> LightSamplingExtension;

	bool bExtensionRegistered = false;

	FSceneView* View;
	//LotusPathTracingData PathTracingData;

	//virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool RequiresGameThreadEndOfFrameUpdates() const override
	{
		// this method could probably be removed allowing them to run on any thread, but it isn't worth the trouble
		return true;
	}

	void UpdateSceneCaptureContents(FSceneInterface* Scene) override;

	//UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Projection)
	//	bool bUseCustomProjectionMatrixNew;

	/** Output render target of the scene capture that can be read in materials. */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	//	TObjectPtr<class UTextureRenderTarget2D> TextureTarget;

	//void UpdateSceneCaptureContents(FSceneInterface* Scene) override;
	
	/** Render the scene to the texture the next time the main view is rendered. */
	//void CaptureSceneDeferred();

	// For backwards compatibility
	//void UpdateContent() { CaptureSceneDeferred(); }
};
