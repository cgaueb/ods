// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "CuttedDynamicGeometry.generated.h"

UENUM(BlueprintType)
enum class ECutterType : uint8
{
	BOX UMETA(DisplayName = "Box"),
	SPHERICAL UMETA(DisplayName = "Spherical"),
	S_SHAPE UMETA(DisplayName = "S Shape")
};

/**
 * 
 */
UCLASS()
class LOTUSTESTBED_API ACuttedDynamicGeometry : public ADynamicMeshActor
{
	GENERATED_BODY()

public:

	ACuttedDynamicGeometry();

	TArray<FBox> GetOpeningsBBOX()
	{
		return m_applied_openings_bbox;
	}

public: // UI

	// Set the transformation of the default cutter
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cutter")
	void ApplyCutterTransform(ECutterType type, FTransform transform);

	// Reset the cutted geometry
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cutted")
		void Reset();

	// Cutter
	//UPROPERTY(EditAnywhere, Category = "Cutter", DisplayName = "Static Meshes to use for cutting")
	//TArray<TObjectPtr<class UStaticMeshComponent>> CutterMeshes;

	// Cutted Geometry
	UPROPERTY(EditAnywhere, Category = "Cutted", DisplayName = "Static Mesh to Cut")
		TObjectPtr<class AStaticMeshActor> CuttedMesh = nullptr;	

	// Cutter Depth
	UPROPERTY(EditAnywhere, Category = "Cutted", DisplayName = "Cutter Depth", meta = (Units = "m"))
	float m_cutter_depth = 1.0;

	/*				DEBUG				*/

	// Debug Reset the cutted geometry
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cutter")
		void ResetDebug();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutter")
		FVector m_reset_pos;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutter");
	FQuat m_reset_rot = FQuat(0, 0, 0, 1);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cutter");
	FVector m_reset_scale = FVector(1, 1, 1);

	/*				END DEBUG				*/

private:
	TArray<FBox> m_applied_openings_bbox;

	TObjectPtr<class UStaticMesh> m_s_shape_mesh = nullptr;
};
