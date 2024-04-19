// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"
#include "PlanarSampler.generated.h"

USTRUCT(BlueprintType)
struct FIlluminanceGoal {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, DisplayName = "Min", meta = (Units = "lux"))
	float min_value = 2.0f;

	UPROPERTY(EditAnywhere, DisplayName = "Max", meta = (Units = "lux"))
	float max_value = 2.0f;
};

USTRUCT(BlueprintType)
struct FIlluminanceCost {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, DisplayName = "Value", meta = (Units = "lux"))
	float value = 0.0f;

	UPROPERTY(EditAnywhere, DisplayName = "Loss")
	float lost = 1.0f;
};

/**
 * 
 */
UCLASS()
class LOTUSTESTBED_API APlanarSampler : public ASceneCapture2D
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Lighting Sampler", DisplayName = "Enable Rendering")
		bool m_enable_rendering = false;

	UPROPERTY(EditAnywhere, Category = "Lighting Sampler", DisplayName = "Show Preview")
	bool m_show_preview = false;
	bool m_current_show_preview = false; // to allow the toggle effect	

	unsigned int m_spp = 6;
	bool m_should_reset = true;
	bool m_rendering_done = false;

	UPROPERTY(EditAnywhere, Category = "Lighting Sampler", DisplayName = "Illumination Goal Range")//meta=(ShowOnlyInnerProperties)
		FIlluminanceGoal m_illumination_goal;

	UPROPERTY(VisibleAnywhere, Category = "Lighting Sampler", DisplayName = "Illumination Cost")//meta=(ShowOnlyInnerProperties)
		FIlluminanceCost m_illuminance_cost;

private:

	TObjectPtr<class UTextureRenderTarget2D> m_rt = nullptr;
	void CreateRT();

	/** Static Mesh component. */
	//UPROPERTY(Category = "DecalActor", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UStaticMeshComponent> StaticMeshComponent;
	TObjectPtr<class UMaterialInterface> GatherMaterial;
	TObjectPtr<class UMaterialInterface> PreviewMaterial;

	unsigned int m_rendering_counter = 0;

	float m_light_efficacy = 1.0;

public:
	APlanarSampler();

	virtual void Tick(float DeltaSeconds) override;
	virtual void TickActor
	(
		float DeltaTime,
		enum ELevelTick TickType,
		FActorTickFunction& ThisTickFunction
	) override;

	float m_per_call_rendering_time_ms;

	/** Allows Tick To happen in the editor viewport*/
	virtual bool ShouldTickIfViewportsOnly() const override;

	/** Returns UStaticMeshComponent subobject **/
	class UStaticMeshComponent* GetStaticComponent() const { return StaticMeshComponent; }

	struct RetColorStats {
		FVector3f illuminance = FVector3f(0, 0, 0); // Lux
		FVector3f min_luminance = FVector3f(FLT_MAX, FLT_MAX, FLT_MAX); // nits
		FVector3f max_luminance = FVector3f(-FLT_MAX, -FLT_MAX, -FLT_MAX); // nits
		bool valid = false;
	};

	RetColorStats GetColor();


	void SetShouldReset(bool should) { m_should_reset = should; };
	void SetRenderingDone(bool done) { m_rendering_done = done; };
	bool GetRenderingDone() { return m_rendering_done; };


	void SetLightEfficacy(float light_efficacy) { m_light_efficacy = light_efficacy; }
};
