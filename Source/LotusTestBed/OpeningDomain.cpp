// Fill out your copyright notice in the Description page of Project Settings.


#include "OpeningDomain.h"
#include "Math/Transform.h"

#include "CuttedDynamicGeometry.h"
#include "Engine/StaticMeshActor.h"
#include "ImageUtils.h"
#include "Math/UnrealMathUtility.h"

//#define DEBUG_OUT

/*enum class EParameterName : uint8 {
	POS_X = 0,
	POS_Y = 1,
	SCALE_X = 2,
	SCALE_Y = 3,
	SPACING_X = 4,
	SPACING_Y = 5
};*/
namespace EParameterName {
	constexpr int POS_X = 0;
	constexpr int POS_Y = 1;
	constexpr int SCALE_X = 2;
	constexpr int SCALE_Y = 3;
	constexpr int SPACING_X = 4;
	constexpr int SPACING_Y = 5;
}

// Sets default values
AOpeningDomain::AOpeningDomain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	[[deprecated]] ConstructorHelpers::FClassFinder<AActor> CutterDynamicClassClassFinder(TEXT("/Game/BP_MyCutterDynamicActor.BP_MyCutterDynamicActor_C"));
	CutterDynamicClass = CutterDynamicClassClassFinder.Class;
	
	DomainMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("PlanarStaticMesh", true);
	auto MeshAsset = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("StaticMesh'/Engine/BasicShapes/Plane.Plane'"));
	auto MaterialAsset = ConstructorHelpers::FObjectFinder<UMaterialInterface>(TEXT("Matarial'/Game/Materials/LightingDesign/OpeningDomain.OpeningDomain'"));
	
	if (MeshAsset.Object != nullptr && MaterialAsset.Object != nullptr)
	{
		DomainMeshComponent->SetStaticMesh(MeshAsset.Object);
		DomainMeshComponent->SetMaterial(0, MaterialAsset.Object);
		//DomainPart->SetMaterial(1, DebugMaterialInstance);

		DomainMeshComponent->bVisibleInRayTracing = false;
		DomainMeshComponent->bVisibleInRealTimeSkyCaptures = false;
		DomainMeshComponent->bVisibleInReflectionCaptures = false;
		DomainMeshComponent->bVisibleInSceneCaptureOnly = false;
		DomainMeshComponent->bHiddenInSceneCapture = true;
		//DomainPart->bHiddenInGame = true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OpeningDomain: Could not find shape"));
	}
}

// Called when the game starts or when spawned
void AOpeningDomain::BeginPlay()
{
	Super::BeginPlay();
}

bool AOpeningDomain::ShouldTickIfViewportsOnly() const 
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

void AOpeningDomain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//m_cutter_scale = FVector2f(GetActorScale3D().X, GetActorScale3D().Y);

#ifdef DEBUG_OUT
	if (m_opening_type != EOpeningType::SIMPLE_OPENING && m_scale_optimization != EScaleOptimization::NO_OPTIMIZATION)
	{
		UE_LOG(LogTemp, Error, TEXT("Domain: %s has Spacing and Scaling (NOT SUPPORTED)"), *GetName());
	}
#endif

#if 0
	UObject* Object = this; //assign your object instance which has the Flag member here!
	UClass* Class = Object->GetClass();
	static FName FlagPropertyName = TEXT("m_max_cutters");
	for (TFieldIterator<UProperty> PropertyIterator(Class); PropertyIterator; ++PropertyIterator)
	{
		UProperty* Property = *PropertyIterator;
		FName const PropertyName = Property->GetFName();
		if (PropertyName == FlagPropertyName)
		{
			FIntProperty* IntProperty = Cast<FIntProperty>(Property);
			if (IntProperty)
			{
				IntProperty->SetPropertyValue((void*)Object, 3);
				IntProperty->SetMetaData("UIMin", TEXT("2"));
				//IntProperty->SetInstanceMetaData("UIMin", TEXT("0.0f"));
			}
		}
	}

	UProperty* Property = Class->FindPropertyByName(FlagPropertyName);
	if (FIntProperty* IntProperty = Cast<FIntProperty>(Property))
	{
		//IntProperty->SetPropertyValue((void*)Object, 3);
	}
#endif
}

// Get the transform of the cutter based on the parameters of the State
FTransform AOpeningDomain::GetTransformFromParameterization(float x, float y)
{
	return GetTransformFromParameterization(x, y, -1, -1);
}

// Get the transform of the cutter based on the parameters of the State. Parameters (posXY, scaleXY)
FTransform AOpeningDomain::GetTransformFromParameterization(float x, float y, float z, float w)
{
	// at first check how many connected domains we have
	if (m_domains.Num() > 0)
	{
		// Domains are concatenated on X
		TArray<FVector2f> cdf;
		cdf.Add({ (float)GetActorScale().X, (float)GetActorScale().Y });
		for (auto domain : m_domains)
		{
			auto scale = domain->GetActorScale();
			cdf.Add(cdf.Last() + FVector2f{ (float)scale.X, (float)scale.Y });
		}
		int selected_domain = 0;
		for (; selected_domain < cdf.Num(); selected_domain++)
		{
			if (x * cdf.Last().X <= cdf[selected_domain].X)
				break;
		}
		// in case the loop did not find a domain
		selected_domain = std::min(selected_domain, cdf.Num() - 1);
		// scale x to appropriate value
		float minX = selected_domain == 0? 0.f : cdf[selected_domain - 1].X;
		float maxX = cdf[selected_domain].X;
		x = ((x * cdf.Last().X) - minX) / (maxX - minX);

		if (selected_domain > 0)
			return m_domains[selected_domain - 1]->GetTransformFromParameterization(x, y, z, w);
	}

#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("Domain: %s XY %.1f %.1f"), *GetName(), x, y);
#endif
	
	x = 2.0 * x - 1.0;
	y = 2.0 * y - 1.0;
	x *= 50.0; // Maybe becase the BOX cutter has a size of 100x100x100 ?????
	y *= 50.0;
	FVector origin, extend;
	GetActorBounds(true, origin, extend);
#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("Domain: Orig:(%s) %s"), *origin.ToString(), *extend.ToString());
#endif

	auto trans = ActorToWorld();
	//auto trans2 = GetActorTransform(); // same with the above
	auto rotation = trans.GetRotation();
	auto location = trans.GetLocation();
	auto scale = trans.GetScale3D();

	auto transform = FTransform(FQuat(FVector(0, 1, 0), 0), FVector(x, y, 0), FVector(1, 1, 1)) * trans;
	//auto rotation2 = transform.GetRotation();
	auto location2 = transform.GetLocation();

	float scaleX = (z >= 0.0) ? FMath::Lerp(m_cutter_scaleX.X, m_cutter_scaleX.Y, z) : 1.0;
	float scaleY = (w >= 0.0) ? FMath::Lerp(m_cutter_scaleY.X, m_cutter_scaleY.Y, z) : 1.0;

	return FTransform(rotation, location2, FVector(scaleX, scaleY, 1.0));
}


// Get the transform of the cutter based on the parameters of the State. Max Parameters (posXY, scaleXY, SpacingXY)
bool AOpeningDomain::ApplyTransformFromParameterization(float x1, float x2, float x3, float x4, float x5, float x6)
{
	// Only call because it sets the variable permutation
	GetNumberOfVariables();

	// Apply the operation on the instanced domains
	bool success = true;
	for (auto instance : m_instanced_domains)
	{
		success = instance->ApplyTransformFromParameterization(x1, x2, x3, x4, x5, x6) && success;
	}

	// Check for Neighboring Domains
	if (m_domains.Num() > 0)
	{
		// Domains are concatenated on X
		TArray<FVector2f> cdf;
		cdf.Add({ (float)GetActorScale().X, (float)GetActorScale().Y });
		for (auto domain : m_domains)
		{
			auto scale = domain->GetActorScale();
			cdf.Add(cdf.Last() + FVector2f{ (float)scale.X, (float)scale.Y });
		}
		int selected_domain = 0;
		for (; selected_domain < cdf.Num(); selected_domain++)
		{
			if (x1 * cdf.Last().X <= cdf[selected_domain].X)
				break;
		}
		// in case the loop did not find a domain
		selected_domain = std::min(selected_domain, cdf.Num() - 1);
		// scale x to appropriate value
		float minX = selected_domain == 0 ? 0.f : cdf[selected_domain - 1].X;
		float maxX = cdf[selected_domain].X;
		x1 = ((x1 * cdf.Last().X) - minX) / (maxX - minX);

		// if we select a neighboring domain
		if (selected_domain > 0)
			return m_domains[selected_domain - 1]->ApplyTransformFromParameterization(x1, x2, x3, x4, x5, x6);
	}

	// This domain does not cut, so we skip it 
	if (CuttedMesh == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Domain: %s CuttedMesh does not exists"), *GetName());
		return false;
	}

#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("Domain: %s XY %.1f %.1f"), *GetName(), x, y);
#endif

	//UE_LOG(LogTemp, Error, TEXT("Domain: %f %f %f %f %f"), x1, x2, x3, x4, x5);
	// re arrange the variables based on the usage
	float packed_parameters[6] = { x1, x2, x3, x4, x5, x6};
	x1 = m_parameters_map[EParameterName::POS_X] != -1 ?     packed_parameters[m_parameters_map[EParameterName::POS_X]] : -1.f;
	x2 = m_parameters_map[EParameterName::POS_Y] != -1 ?     packed_parameters[m_parameters_map[EParameterName::POS_Y]] : -1.f;
	x3 = m_parameters_map[EParameterName::SCALE_X] != -1 ?   packed_parameters[m_parameters_map[EParameterName::SCALE_X]] : -1.f;
	x4 = m_parameters_map[EParameterName::SCALE_Y] != -1 ?   packed_parameters[m_parameters_map[EParameterName::SCALE_Y]] : -1.f;
	x5 = m_parameters_map[EParameterName::SPACING_X] != -1 ? packed_parameters[m_parameters_map[EParameterName::SPACING_X]] : -1.f;
	x6 = m_parameters_map[EParameterName::SPACING_Y] != -1 ? packed_parameters[m_parameters_map[EParameterName::SPACING_Y]] : -1.f;
	
	//UE_LOG(LogTemp, Error, TEXT("Domain: %f %f %f %f %f"), x1, x2, x3, x4, x5);

	float scaled_x1 = 2.0 * x1 - 1.0;
	float scaled_x2 = 2.0 * x2 - 1.0;
	scaled_x1 *= 50.0; // Maybe becase the BOX cutter has a size of 100x100x100 ?????
	scaled_x2 *= 50.0;
	FVector origin, extend;
	GetActorBounds(true, origin, extend);
#ifdef DEBUG_OUT
	UE_LOG(LogTemp, Warning, TEXT("Domain: Orig:(%s) %s"), *origin.ToString(), *extend.ToString());
#endif

	auto trans = ActorToWorld();
	//auto trans2 = GetActorTransform(); // same with the above
	auto rotation = trans.GetRotation();
	auto location = trans.GetLocation();
	auto scale = trans.GetScale3D();

	auto transform = FTransform(FQuat(FVector(0, 1, 0), 0), FVector(scaled_x1, scaled_x2, 0), FVector(1, 1, 1)) * trans;
	//auto rotation2 = transform.GetRotation();
	auto location2 = transform.GetLocation();

	float scaleX = (x3 >= 0.0) ? FMath::Lerp(m_cutter_scaleX.X, m_cutter_scaleX.Y, x3) : 1.0;
	float scaleY = (x4 >= 0.0) ? FMath::Lerp(m_cutter_scaleY.X, m_cutter_scaleY.Y, x4) : 1.0;

	if (m_opening_type == EOpeningType::SIMPLE_OPENING)
	{
		// Put in reverse because of the Unreal Coordinate System
		FTransform cutter_transform(rotation, location2, FVector(scaleX, scaleY, 1.0));
		CuttedMesh->ApplyCutterTransform(m_cutter_type, cutter_transform);
	}
	else if(m_opening_type == EOpeningType::SPACING_X_OPENING)
	{
		x5 = 2.0 * x5 - 1.0; // spacing in [-1 1] to set the direction

		// Add a minimum offset to prevent overlaping
		//x5 += (std::signbit(x5) ? -1.0f : 1.0f) / GetActorScale3D().X;
		x5 += std::copysign(1.0f / GetActorScale3D().X, x5);

		for (float offset = x1; 0.0f < offset && offset < 1.0f; offset += x5)
		{
			scaled_x1 = 2.0 * offset - 1.0;
			scaled_x1 *= 50.0; // Maybe becase the BOX cutter has a size of 100x100x100 ?????
			auto atransform = FTransform(FQuat(FVector(0, 1, 0), 0), FVector(scaled_x1, scaled_x2, 0), FVector(1, 1, 1)) * trans;
			//auto rotation2 = transform.GetRotation();
			auto alocation2 = atransform.GetLocation();
			FTransform cutter_transform2(rotation, alocation2, FVector(scaleX, scaleY, 1.0));
			CuttedMesh->ApplyCutterTransform(m_cutter_type, cutter_transform2);
		}		
	}
	else if (m_opening_type == EOpeningType::SPACING_XY_OPENING)
	{
		x5 = 2.0 * x5 - 1.0; // spacing in [-1 1] to set the direction
		x6 = 2.0 * x6 - 1.0; // spacing in [-1 1] to set the direction

		// Add a minimum offset to prevent overlaping
		x5 += std::copysign(1.0f / GetActorScale3D().X, x5);
		x6 += std::copysign(1.0f / GetActorScale3D().Y, x6);

		for (float offsetX = x1; 0.0f < offsetX && offsetX < 1.0f; offsetX += x5)
		{
			for (float offsetY = x2; 0.0f < offsetY && offsetY < 1.0f; offsetY += x6)
			{
				scaled_x1 = 2.0 * offsetX - 1.0;
				scaled_x1 *= 50.0; // Maybe becase the BOX cutter has a size of 100x100x100 ?????
				scaled_x2 = 2.0 * offsetY - 1.0;
				scaled_x2 *= 50.0; // Maybe becase the BOX cutter has a size of 100x100x100 ?????
				auto atransform = FTransform(FQuat(FVector(0, 1, 0), 0), FVector(scaled_x1, scaled_x2, 0), FVector(1, 1, 1)) * trans;
				//auto rotation2 = transform.GetRotation();
				auto alocation2 = atransform.GetLocation();
				FTransform cutter_transform2(rotation, alocation2, FVector(scaleX, scaleY, 1.0));
				CuttedMesh->ApplyCutterTransform(m_cutter_type, cutter_transform2);
			}
		}
	}

	return true;
}

bool AOpeningDomain::ResetCutted()
{
	if (CuttedMesh == nullptr)
	{
		return false;
	}
	
	CuttedMesh->Reset();

	bool ok = true;
	for (auto inst : m_instanced_domains)
	{
		ok = inst->ResetCutted() && ok;
	}
	for (auto inst : m_domains)
	{
		ok = inst->ResetCutted() && ok;
	}
	return ok;	
}

int AOpeningDomain::GetNumberOfVariables()
{
	int number = 2; // Position XY

	switch (m_scale_optimization)
	{
	case EScaleOptimization::SCALE_X_AXIS:
		m_parameters_map[EParameterName::SCALE_X] = number;
		number += 1; // Scale only on the X Axis. Requires 1 Variables
		break;
	case EScaleOptimization::UNIFORM_SCALE:
		m_parameters_map[EParameterName::SCALE_X] = number;
		m_parameters_map[EParameterName::SCALE_Y] = number;
		number += 1; // Uniform scaling requires 1 Variable
		break;
	case EScaleOptimization::VARIABLE_SCALE:
		m_parameters_map[EParameterName::SCALE_X] = number;
		m_parameters_map[EParameterName::SCALE_Y] = number + 1;
		number += 2; // Variable scaling requires 2 Variables
		break;
	case EScaleOptimization::NO_OPTIMIZATION:
	default:
		// No scaling
		break;
	}

	if (m_opening_type == EOpeningType::SPACING_X_OPENING)
	{
		m_parameters_map[EParameterName::SPACING_X] = number;
		number += 1; // Add Spacing X
	}
	else if (m_opening_type == EOpeningType::SPACING_XY_OPENING)
	{
		m_parameters_map[EParameterName::SPACING_X] = number;
		m_parameters_map[EParameterName::SPACING_Y] = number + 1;
		number += 2; // Add Spacing XY
	}
	
	return number;
}

TArray<FBox> AOpeningDomain::GetOpeningsBBOX()
{
	return CuttedMesh ? CuttedMesh->GetOpeningsBBOX() : TArray<FBox>{};
}

void AOpeningDomain::BuildDebugTexture(const TArray<double>& Data, int RowSize)
{

	TArray<FColor> ColorData;
	ColorData.SetNum(Data.Num());

	double maxVal = Data[0];
	for (int i = 1; i < Data.Num(); ++i)
	{
		if(Data[i] > maxVal) maxVal = Data[i];
	}

	for (int i = 0; i < ColorData.Num(); ++i)
	{
		const uint8 val = (Data[i] / maxVal) * 255;
		ColorData[i] = FColor(val, val, val);
	}

	FCreateTexture2DParameters params;
	params.bDeferCompression = false;
	params.bSRGB = true;
	params.bUseAlpha = false;
	params.CompressionSettings = TextureCompressionSettings::TC_Default;
	params.SourceGuidHash = FGuid::NewGuid();

	UTexture2D* mDebugTexture = FImageUtils::CreateTexture2D(
		RowSize, RowSize, ColorData, this, FString(TEXT("CostTexture")), EObjectFlags::RF_Public, params);
	
	mDebugTexture->Filter = TextureFilter::TF_Nearest;

	DebugMaterialInstance = DomainMeshComponent->CreateDynamicMaterialInstance(1, DomainMeshComponent->GetMaterial(0), TEXT("HeatMapMaterial"));
	//DebugMaterialInstance = UMaterialInstanceDynamic::Create(MaterialAsset.Object, this, TEXT("DebugMaterial"));
	DebugMaterialInstance->SetTextureParameterValue(FName("Diffuse"), mDebugTexture);
	DomainMeshComponent->SetMaterial(1, DebugMaterialInstance);
}

