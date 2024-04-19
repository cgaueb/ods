// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"
#include "ViewSampler.generated.h"

USTRUCT(BlueprintType)
struct FLuminanceGoal {
	GENERATED_BODY()

		UPROPERTY(EditAnywhere, DisplayName = "Min") // , meta = (Units = "cd")
		float min_value = 2.0f;

	UPROPERTY(EditAnywhere, DisplayName = "Max") // cd/m^2
		float max_value = 2.0f;
};

USTRUCT(BlueprintType)
struct FLuminanceCost {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, DisplayName = "Value") // , meta = (Units = "cd")
	float value = 0.0f;

	UPROPERTY(VisibleAnywhere, DisplayName = "Loss") // cd/m^2
	float loss = 1.0f;
};


/**
 * 
 */
UCLASS()
class LOTUSTESTBED_API AViewSampler : public ASceneCapture2D
{
	GENERATED_BODY()

	TObjectPtr<class UTextureRenderTarget2D> m_rt = nullptr;
	TObjectPtr<class UTexture2D> m_texture2D = nullptr;
	void CreateRT();

	unsigned int m_rendering_counter = 0;
	unsigned int m_spp = 32;
	bool m_should_reset = false;
	bool m_rendering_done = false;

	float m_light_efficacy = 1.0;

public:

	AViewSampler();

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

	UPROPERTY(EditAnywhere, Category = "Lighting Sampler", DisplayName="Enable Rendering")
	bool m_enable_rendering = false;

	float m_per_call_rendering_time_ms;

	struct RetColorStats {
		FVector3f average = FVector3f(0,0,0);
		FVector3f minValue = FVector3f(FLT_MAX, FLT_MAX, FLT_MAX);
		FVector3f maxValue = FVector3f(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		bool valid = false;
	};

	// Get lighting
	RetColorStats GetColor();
	
	void SetShouldReset(bool should) { m_should_reset = should; };
	void SetRenderingDone(bool done) { m_rendering_done = done; };
	bool GetRenderingDone() { return m_rendering_done; };

	UPROPERTY(EditAnywhere, Category = "Lighting Sampler", DisplayName = "Illumination Goal Range")//meta=(ShowOnlyInnerProperties)
	FLuminanceGoal m_illumination_goal;

	UPROPERTY(VisibleAnywhere, Category = "Lighting Sampler", DisplayName = "Luminance Cost")
	FLuminanceCost m_luminance_cost;

	void SetLightEfficacy(float light_efficacy) { m_light_efficacy = light_efficacy; }
};
