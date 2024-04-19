// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"
#include "AbstractSampler.generated.h"

/**
 * 
 */
UCLASS()
class LOTUSTESTBED_API AAbstractSampler : public ASceneCapture
{
	GENERATED_BODY()

	TObjectPtr<class UTextureRenderTarget2D> m_rt = nullptr;
	TObjectPtr<class UTexture2D> m_texture2D = nullptr;
	void CreateRT();

	int m_rendering_counter = 0;

public:

	AAbstractSampler();

	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene);

	virtual void Tick(float DeltaSeconds) override;
	virtual void TickActor
	(
		float DeltaTime,
		enum ELevelTick TickType,
		FActorTickFunction& ThisTickFunction
	) override;

	/** Allows Tick To happen in the editor viewport*/
	virtual bool ShouldTickIfViewportsOnly() const override;

	// shows only the struct components
	// UPROPERTY(EditAnywhere, Category="Cat With ShowOnlyInnerProperties", meta=(ShowOnlyInnerProperties))

	UPROPERTY(EditAnywhere, Category = "Lighting Sampler", DisplayName="Enable Rendering")
	bool m_enable_rendering = true;
	
	/** Scene capture component. */
	UPROPERTY(Category = DecalActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<class UAbstractSamplerComponent> AbstractSamplerComponent;

	class UAbstractSamplerComponent* GetAbstractSamplerComponent() const { return AbstractSamplerComponent; }
};
