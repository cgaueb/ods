// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CuttedDynamicGeometry.h"

#include "OpeningDomain.generated.h"


UENUM(BlueprintType)
enum class EOpeningType : uint8
{
	SIMPLE_OPENING UMETA(DisplayName = "No spacing"),
	SPACING_X_OPENING UMETA(DisplayName = "Spacing on X Axis"),
	SPACING_XY_OPENING UMETA(DisplayName = "Spacing on XY Plane")
};

UENUM(BlueprintType)
enum class EScaleOptimization : uint8
{
	NO_OPTIMIZATION UMETA(DisplayName = "No Optimization"),
	SCALE_X_AXIS UMETA(DisplayName = "X Axis Scaling"),
	UNIFORM_SCALE UMETA(DisplayName = "Uniform Scaling"),
	VARIABLE_SCALE UMETA(DisplayName = "Variable Scaling")
};

/*
	Opening Domain will be agnostic to the cutter. It will just provide a parameterization between State to WorldTransform.
*/

UCLASS()
class LOTUSTESTBED_API AOpeningDomain : public AActor
{
	GENERATED_BODY()

	TSubclassOf<AActor> CutterDynamicClass = nullptr;
	
public:	
	AOpeningDomain();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Allows Tick To happen in the editor viewport*/
	virtual bool ShouldTickIfViewportsOnly() const override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	TArray<FBox> GetOpeningsBBOX();

public:	// UI
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName = "Neighbor Domains", Category = "Domain")
		TArray<AOpeningDomain*> m_domains; // if it has sub domains

	UPROPERTY(EditAnywhere, BlueprintReadOnly, DisplayName = "Instance Domains", Category = "Domain")
		TArray<AOpeningDomain*> m_instanced_domains; // if it has instances

	UPROPERTY(VisibleAnywhere, DisplayName = "Domain Preview Mesh", Category = "Domain")
		TObjectPtr<class UStaticMeshComponent> DomainMeshComponent; // Domain mesh for preview

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Cutted Mesh")
		TObjectPtr<class ACuttedDynamicGeometry> CuttedMesh; // The mesh that will be cut

	UPROPERTY(VisibleAnywhere, Category = "Domain", DisplayName = "Max Cutters", meta = (ClampMin = 1, ClampMax = 1000, ToolTip = "Maximum allowed openings in this domain"))
	int m_max_cutters = 1;

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Number of Cutters", meta = (ClampMin = 1, ClampMax = 1000, ToolTip = "Maximum allowed openings in this domain"))
	int m_number_of_cutters = 1;

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Cutter ScaleX or Locked")
	FVector2f m_cutter_scaleX = FVector2f(0.5f, 2.0f);

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Cutter ScaleY")
	FVector2f m_cutter_scaleY = FVector2f(0.5f, 2.0f);

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Opening Type")
		EOpeningType m_opening_type = EOpeningType::SIMPLE_OPENING;

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Optimize Scale")
	EScaleOptimization m_scale_optimization = EScaleOptimization::NO_OPTIMIZATION;

	UPROPERTY(EditAnywhere, Category = "Domain", DisplayName = "Cutter Type")
	ECutterType m_cutter_type = ECutterType::BOX;
	

public:

	int GetNumberOfVariables();

	// Get the transform of the cutter based on the parameters of the State.
	// Expects values in [0 1]
	FTransform GetTransformFromParameterization(float x, float y);

	// Get the transform of the cutter based on the parameters of the State.
	// Expects values in [0 1]
	FTransform GetTransformFromParameterization(float x, float y, float z, float w);

	// Get the transform of the cutter based on the parameters of the State.
	// Expects values in [0 1]
	bool ApplyTransformFromParameterization(float x, float x2, float x3 = -1.f, float x4 = -1.f, float x5 = -1.f, float x6 = -1.f);

	// Reset the Cutted Geometries
	bool ResetCutted();

	void BuildDebugTexture(const TArray<double>& Data, int RowSize);
private:

	class UMaterialInstanceDynamic* DebugMaterialInstance;

	int m_parameters_map[6] = { 0, 1, -1, -1, -1, -1 };
};
