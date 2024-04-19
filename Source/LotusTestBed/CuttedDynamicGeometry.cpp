// Fill out your copyright notice in the Description page of Project Settings.


#include "CuttedDynamicGeometry.h"

#include "Engine/StaticMeshActor.h"

#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"

#include <algorithm>

//#define DEBUG_OUT
//#define DEBUG_EXEC

// Cutted Geometry. Now it is affected by one Cutter and the cutted geometry is a Default one.
// It can be affected by many cutters in the next revision

ACuttedDynamicGeometry::ACuttedDynamicGeometry()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	UDynamicMesh* dynamic_mesh = GetDynamicMeshComponent()->GetDynamicMesh();

	// 2x2x2 meters
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		dynamic_mesh,
		FGeometryScriptPrimitiveOptions(),
		FTransform(),
		200,
		200,
		200);

	SetActorTransform(FTransform::Identity);
	//bLockLocation = true;

	auto MeshAsset = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Game/OpeningDesign/curve.curve'"));
	//auto MaterialAsset = ConstructorHelpers::FObjectFinder<UMaterialInterface>(TEXT("Matarial'/Game/Materials/LightingDesign/WhiteDiffuse.WhiteDiffuse'"));

	m_s_shape_mesh = MeshAsset.Object;
}

// Default size of window is always 1x1x1 meters
void ACuttedDynamicGeometry::ApplyCutterTransform(ECutterType type, FTransform transform)
{
	UDynamicMesh* dynamic_mesh = GetDynamicMeshComponent()->GetDynamicMesh();

#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("Cutted: %s"), *transform.ToString());
#endif

	// Create a cutter box
	UDynamicMesh* cutter = AllocateComputeMesh();
	// 1x1x1 meters
	if (type == ECutterType::BOX)
	{
		cutter = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
			cutter,
			FGeometryScriptPrimitiveOptions(),
			FTransform(),
			100,
			100,
			100 * m_cutter_depth,
			0, 0, 0,
			EGeometryScriptPrimitiveOriginMode::Center);
	}
	else if (type == ECutterType::SPHERICAL)
	{
		cutter = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(
			cutter,
			FGeometryScriptPrimitiveOptions(),
			FTransform(),
			50, 100 * m_cutter_depth,
			12, 0, true,
			EGeometryScriptPrimitiveOriginMode::Center);
	}
	else if (type == ECutterType::S_SHAPE)
	{
		if (m_s_shape_mesh == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("S Shape cutter was not found: %s"), *GetName());
			return;
		}

		TEnumAsByte<EGeometryScriptOutcomePins> CutterOutcome;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
			m_s_shape_mesh,
			cutter,
			FGeometryScriptCopyMeshFromAssetOptions(),
			FGeometryScriptMeshReadLOD(),
			CutterOutcome,
			nullptr
		);
		
		/*UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(
			dynamic_mesh,
			CuttedMesh->GetActorTransform()
		);
		*/
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Undefined Cutter: %s"), *GetName());
		return;
	}	

#ifdef DEBUG_EXEC
	double start = FPlatformTime::Seconds() * 1000;
#endif
	dynamic_mesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		dynamic_mesh,
		FTransform(),
		cutter,
		transform,
		EGeometryScriptBooleanOperation::Subtract,
		FGeometryScriptMeshBooleanOptions());

#ifdef DEBUG_EXEC
	double end = FPlatformTime::Seconds() * 1000;
	UE_LOG(LogTemp, Warning, TEXT("CSG time: %.2f"), end - start);
#endif

	// Compute Actor Bounds
	{
		UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(cutter, transform);
		FBox bbox = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(cutter);
		/*ADynamicMeshActor* temp = NewObject<ADynamicMeshActor>();
		temp->SetActorTransform(transform);
		temp->GetDynamicMeshComponent()->SetDynamicMesh(cutter);
		FVector origin, extend;
		temp->GetActorBounds(true, origin, extend);
		temp->MarkPendingKill(); // inform the GC to garbage collect it*/

#ifdef DEBUG_OUT
		UE_LOG(LogTemp, Error, TEXT("Cutter Trans: %s"), *transform.ToString());
		//UE_LOG(LogTemp, Error, TEXT("Cutter Bounds: %s %s"), *origin.ToString(), *extend.ToString());
		UE_LOG(LogTemp, Error, TEXT("Cutter Bounds: %s"), *bbox.ToString());
#endif

		m_applied_openings_bbox.Add(bbox);
	}

	ReleaseComputeMesh(cutter);
}

void ACuttedDynamicGeometry::Reset()
{
	if (CuttedMesh == nullptr)
		return;

#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("Reset Cutted"));
#endif

	SetActorTransform(FTransform::Identity);

	UDynamicMesh* dynamic_mesh = GetDynamicMeshComponent()->GetDynamicMesh();

	// Clear the dynamic mesh
	dynamic_mesh = dynamic_mesh->Reset();

	FTransform CutterLocalToWorld;
	TEnumAsByte<EGeometryScriptOutcomePins> CutterOutcome;
	UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(
		Cast<UStaticMeshComponent>(CuttedMesh->GetComponentByClass(UStaticMeshComponent::StaticClass())),
		dynamic_mesh,
		FGeometryScriptCopyMeshFromComponentOptions(),
		false,
		CutterLocalToWorld,
		CutterOutcome,
		nullptr);

	UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(
		dynamic_mesh,
		CuttedMesh->GetActorTransform()
	);

	auto original_mesh_materials = Cast<UStaticMeshComponent>(CuttedMesh->GetComponentByClass(UStaticMeshComponent::StaticClass()))->GetMaterials();
	GetDynamicMeshComponent()->SetNumMaterials(original_mesh_materials.Num());
	for (int i = 0; i < original_mesh_materials.Num(); i++)
	{
		GetDynamicMeshComponent()->SetMaterial(i, original_mesh_materials[i]);
	}

	Cast<UStaticMeshComponent>(CuttedMesh->GetComponentByClass(UStaticMeshComponent::StaticClass()))->SetVisibility(false);

	m_applied_openings_bbox.Empty();
}

void ACuttedDynamicGeometry::ResetDebug()
{
	FVector pos = m_reset_pos;
	FQuat rot = m_reset_rot;
	FTransform cutter_trans(rot, pos, m_reset_scale);

	// Reset the Cutted dynamic Mesh
	Reset();

	ApplyCutterTransform(ECutterType::BOX, cutter_trans);
	return;

	UDynamicMesh* dynamic_mesh = GetDynamicMeshComponent()->GetDynamicMesh();

	// Create a cutter box
	UDynamicMesh* cutter = AllocateComputeMesh();
	// 1x1x1 meters
	cutter = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		cutter,
		FGeometryScriptPrimitiveOptions(),
		FTransform(),
		100,
		100,
		100,
		0, 0, 0,
		EGeometryScriptPrimitiveOriginMode::Center);

	dynamic_mesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		dynamic_mesh,
		FTransform(),
		cutter,
		cutter_trans,
		EGeometryScriptBooleanOperation::Subtract,
		FGeometryScriptMeshBooleanOptions());

	ReleaseComputeMesh(cutter);


	return;

	//UE_LOG(LogTemp, Error, TEXT("Cutter: XY %.1f %.1f"), );
	ActorToWorld().ToInverseMatrixWithScale().DebugPrint();
	FTransform(rot, pos).ToMatrixWithScale().DebugPrint();

	// Possible Correct??????
	FTransform transform = ActorToWorld().Inverse();
	transform.SetTranslation(ActorToWorld().InverseTransformPosition(pos));
	transform.ConcatenateRotation(rot);
	FVector newScale = transform.InverseTransformVectorNoScale(transform.GetScale3D());
	newScale *= newScale.GetSignVector();
	transform.SetScale3D(newScale);

	// Clear the dynamic mesh
	dynamic_mesh = dynamic_mesh->Reset();
	// Add a box
	dynamic_mesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
		dynamic_mesh,
		FGeometryScriptPrimitiveOptions(),
		transform,
		100,
		100,
		100); // Origin is at the center but on the base of the object
}