// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/ObjectLibrary.h"
#include "Components/SpotLightComponent.h"

#include "ViewSampler.h"
#include "PlanarSampler.h"
#include "BayesOptimizer.hpp"

#include <random>

#include "OpeningEngine.generated.h"

USTRUCT(BlueprintType)
struct FOptimizationOpeningState {
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere)
		TArray<float> parameters; // POSX (it also has the opening subdomain index), POSY, ScaleX, ScaleY, Spacing

	UPROPERTY(VisibleAnywhere)
		int domainIndex = 0; // the domain that the cutter uses
};

USTRUCT(BlueprintType)
struct FSamplerPair {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<AActor> Sampler = nullptr;

	UPROPERTY(VisibleAnywhere)
	FVector2D Value;
};

USTRUCT(BlueprintType)
struct FOpeningPair {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FOptimizationOpeningState> cutter = { };

	UPROPERTY(VisibleAnywhere)
	float cost = FLT_MAX;

	UPROPERTY(VisibleAnywhere)
	float loss = FLT_MAX;

	UPROPERTY(VisibleAnywhere)
	float penalty = FLT_MAX;
};

USTRUCT(BlueprintType)
struct FOptimizationState {
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	float best_loss = FLT_MAX;

	UPROPERTY(VisibleAnywhere)
	float best_cost = FLT_MAX;

	UPROPERTY(VisibleAnywhere)
	float best_penalty = FLT_MAX;

	UPROPERTY(VisibleAnywhere)
	float Total_time_in_seconds = 0;

	UPROPERTY(VisibleAnywhere)
	TArray<FSamplerPair> per_planar_sampler_cost = { };

	UPROPERTY(VisibleAnywhere)
	TArray<FSamplerPair> per_view_sampler_cost = { };

	UPROPERTY(VisibleAnywhere)
	TArray<FOpeningPair> top_k_openings = { };

	UPROPERTY(VisibleAnywhere)
	TArray<FOptimizationOpeningState> best_cutter = { };

	UPROPERTY()
	TArray<FOptimizationOpeningState> previous_cutter = { };
};

template<typename T>
void GetObjectsOfClass(TArray<T*>& OutArray)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetData;
	AssetRegistryModule.Get().GetAssetsByClass(T::StaticClass()->GetFName(), AssetData);
	for (int i = 0; i < AssetData.Num(); i++) {
		T* Object = Cast<T>(AssetData[i].GetAsset());
		OutArray.Add(Object);
	}
}

template<typename T>
void FindAllActors(UWorld* World, TArray<T*>& Out)
{
	for (TActorIterator<T> It(World); It; ++It)
	{
		Out.Add(*It);
	}
}

template<typename T>
void FindAllAssets(TArray<T*>& array, FString AssetPath) {
	auto textureLibrary = UObjectLibrary::CreateLibrary(T::StaticClass(), true, GIsEditor);
	textureLibrary->LoadAssetDataFromPath(AssetPath);

	TArray<FAssetData> Assets;
	textureLibrary->GetAssetDataList(Assets);
	for (auto& asset : Assets) {
		T* val = Cast<T>(asset.GetAsset());
		array.Add(val);
	}
}

struct ODAABB {
	FVector LB = { TNumericLimits<float>::Max(), TNumericLimits<float>::Max(), TNumericLimits<float>::Max() };
	FVector UB = { TNumericLimits<float>::Min(), TNumericLimits<float>::Min(), TNumericLimits<float>::Min() };

	ODAABB() = default;
	ODAABB(FVector LowerBound, FVector UpperBound) : LB(LowerBound), UB(UpperBound) {}

	static bool DoIntersect(const ODAABB& A, const ODAABB& B) {
		return(
			A.UB.X > B.LB.X &&
			A.LB.X < B.UB.X &&
			A.UB.Y > B.LB.Y &&
			A.LB.Y < B.UB.Y &&
			A.UB.Z > B.LB.Z &&
			A.LB.Z < B.UB.Z
		);
	}

	static ODAABB Intersect(const ODAABB& A, const ODAABB& B) {
		if (!DoIntersect(A, B)) return {};
		return {
			{ std::max(A.LB.X, B.LB.X), std::max(A.LB.Y, B.LB.Y), std::max(A.LB.Z, B.LB.Z) },
			{ std::min(A.UB.X, B.UB.X), std::min(A.UB.Y, B.UB.Y), std::min(A.UB.Z, B.UB.Z) }
		};
	}
};

UCLASS()
class LOTUSTESTBED_API AOpeningEngine : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOpeningEngine();

	// Stages
	enum SimulationStage {
		LOTUS_STAGE_INIT,
		LOTUS_STAGE_SET_MAX_ENV_MAP,
		LOTUS_STAGE_SET_AVG_ENV_MAP,
		LOTUS_STAGE_VIEW_SAMPLERS,
		LOTUS_STAGE_PLANAR_SAMPLERS,
		LOTUS_STAGE_CSG_OPS,
		LOTUS_STAGE_OPT_STEP,
		LOTUS_STAGE_MAX
	};

	int m_csg_counter = 0;
	int m_csg_reset = 200;

	int m_opt_counter = 0;
	int m_opt_reset = 200;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Allows Tick To happen in the editor viewport*/
	virtual bool ShouldTickIfViewportsOnly() const override;

private:
	
	void FindSamplers();
	double Loss(double YtrueMin, double YtrueMax, double Ytest);

	// Samplers
	TArray<class AViewSampler*> m_view_samplers;
	TArray<class APlanarSampler*> m_planar_samplers;

	BayesOptimizer Optimizer;

	// Evaluation
	SimulationStage m_stage = LOTUS_STAGE_INIT;

	class ASkyLight* m_skylight;

	class UTextureCube* m_env_map = nullptr;
	class UTextureCube* m_view_env_map = nullptr;
	class UTextureCube* m_planar_env_map = nullptr;
	std::mt19937 m_random_generator;
	std::uniform_real_distribution<float> m_UniformF_distr;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Evaluation", meta = (ClampMin = 1, ToolTip = "Light Efficacy from the Environement"))
	float m_light_efficacy = 30000;

	/*		Optimization UI		*/

	UPROPERTY(VisibleAnywhere, Category = "Optimization", DisplayName = "Is Optimizing")
	bool m_enable_optimization = false;

	UPROPERTY(EditAnywhere, Category = "Optimization", DisplayName = "Optimization Steps", meta=(ClampMin=1, ClampMax=1000, ToolTip = "Optimization Steps"))
	int m_max_optimization_steps = 100;

	UPROPERTY(EditAnywhere, Category = "Optimization", DisplayName = "Low value stiffness")
	float m_low_stiffness = 0.001;

	UPROPERTY(EditAnywhere, Category = "Optimization", DisplayName = "High value stiffness")
	float m_high_stiffness = 0.001;

	UPROPERTY(EditAnywhere, Category = "Optimization", DisplayName = "Penalty multiplier")
	float m_penalty_multiplier = 1;

	//UPROPERTY(EditAnywhere, Category = "Optimization", DisplayName = "Number of Cutters", meta = (ClampMin = 0, ClampMax = 100))
	//int m_cutters_number = 1;

	int m_current_optimization_count = 0;
		
	UPROPERTY(EditAnywhere, Category="Optimization", meta=(ShowOnlyInnerProperties))
	FOptimizationState m_opt_state;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Optimization")
	TArray<class AOpeningDomain*> m_opening_domains;

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Number of initial training samples", meta = (ClampMin = 1, ClampMax = 1000))
	int m_train_steps = BayesOptimizer::GetDefualtTrainingIters();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Number of exploration steps", meta = (ClampMin = 1, ClampMax = 1000))
	int m_explore_steps = BayesOptimizer::GetDefualtExploreIters();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Number of relearn steps", meta = (ClampMin = 0, ClampMax = 1000))
	int m_relearn_steps = BayesOptimizer::GetDefualtRelearnIters();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Numbers of conseq. stack-steps before jump", meta = (ClampMin = 0, ClampMax = 1000))
	int m_force_jump = BayesOptimizer::GetDefaultForceJumpStepIters();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Cache Top-k results", meta = (ClampMin = 1, ClampMax = 10))
	int Top_k = 10;

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Apply Top-k result")
	int Apply_k = 0;

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Learn all parameters")
	bool Learn_all = BayesOptimizer::GetDefaultLearnAll();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Function Prior")
	FVector2f m_prior = BayesOptimizer::GetDefaultPrior();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Kernel log prior")
	FVector2f m_kernel_prior = BayesOptimizer::GetDefaultKernelLogPrior();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "NIG params")
	FVector2f m_nig_params = BayesOptimizer::GetDefaultNIGParams();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Threshold indicating locally stuck", meta = (ClampMin = 0, ClampMax = 1000))
	float m_stack_threshold = BayesOptimizer::GetDefaultStackThreshold();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Variance of observation noise", meta = (ClampMin = 0, ClampMax = 1000))
	float m_noise_observation = BayesOptimizer::GetDefaultObservationNoise();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Uniform prob. for random jump", meta = (ClampMin = 0, ClampMax = 1))
	float m_epsilon_step = BayesOptimizer::GetDefaultEpsilonGreedyStep();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Explore exploit param")
	float m_explore_exploit = BayesOptimizer::GetDefaultExploreExploit();

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Surrogate method", meta = (GetOptions = "GetSurrogateOptions"))
	FName m_surrogate_method = BayesOptimizer::SurrogateMethods()[0];

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Kernel type", meta = (GetOptions = "GetKernelOptions"))
	FName m_kernel_method = BayesOptimizer::KernelMethods()[0];

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Posterior learning method", meta = (GetOptions = "GetLearningOptions"))
	FName m_learning_method = BayesOptimizer::LearningMethods()[0];

	UPROPERTY(EditAnywhere, Category = "Optimization|Bayesian", DisplayName = "Criteria method", meta = (GetOptions = "GetCriteriaOptions"))
	FName m_criteria_method = BayesOptimizer::CriteriaMethods()[1];

	UFUNCTION(CallInEditor, Category = "Optimization", DisplayName="Start Random Optimization")
	void StartBayesOptimization();

	UFUNCTION(CallInEditor, Category = "Optimization", DisplayName = "Apply k-Opening")
	void Apply_k_Opening();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Optimization", DisplayName="Start Random Optimization")
	void StartOptimization();

	void StartOptimizationSA();

	UFUNCTION(CallInEditor, Category = "Optimization")
	void StopOptimization();

	UFUNCTION(CallInEditor, Category = "Optimization")
	void PrintSamplerStats();

	TArray<double> RefLossValues;
	TArray<double> PredLossValues;

	bool m_enable_debug_cost;
	bool m_enable_debug_bayes;
	int m_global_num_of_dims;

	UPROPERTY(EditAnywhere, Category = "Debug", DisplayName = "Debug Samples across dim", meta = (ClampMin = 0, ClampMax = 1000))
	int m_num_samples = 20;

	int m_current_sample = 0;

	UFUNCTION(CallInEditor, Category = "Debug")
	void StartDebugCostFunction();

	UFUNCTION(CallInEditor, Category = "Debug")
	void StartDebugBayesCostFunction();

	void TickDebugCostFunction();
	void TickDebugBayesFunction();
	void DebugSetCutter();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Optimization")
	class UTextureCube* m_max_env_map;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Optimization")
	class UTextureCube* m_avg_env_map;

	UFUNCTION(CallInEditor)
	TArray<FName> GetLearningOptions() const
	{
		return BayesOptimizer::LearningMethods();
	}

	UFUNCTION(CallInEditor)
	TArray<FName> GetCriteriaOptions() const
	{
		return BayesOptimizer::CriteriaMethods();
	}

	UFUNCTION(CallInEditor)
	TArray<FName> GetKernelOptions() const
	{
		return BayesOptimizer::KernelMethods();
	}

	UFUNCTION(CallInEditor)
	TArray<FName> GetSurrogateOptions() const
	{
		return BayesOptimizer::SurrogateMethods();
	}

	bool SetEnvMap(class UTextureCube* env_map);
	void MainLoop(float DeltaTime);
	void ResetDomains();
	void SampleOpeningDomain(bool MutateAll);
	void SampleOpeningDomainAG(bool MutateAll);
	void FinalizeOpenings();
	float EvaluateOverlapLoss();
	float EvaluateLoss();
	bool PrepareOptimizationComponents();
	float GenFloat();
	FVector4f GenFloat4();
	int GenInt(int num); // Return a random number in [0, num]
	void BuildBayesOptDataPoint(TArray<double>& X, double* Y);
	void BuildCutterDataPoint(TArray<FOptimizationOpeningState>& Cutters, const TArray<double>& X);
	void ApplyCutterTransforms();
	void ApplyCutterTransforms(const TArray<FOptimizationOpeningState>& Cutters);
	void CacheCutterSolution(const TArray<FOptimizationOpeningState>& Cutters, double cost);
	void LogArray(const FString& prefix, const TArray<double>& Array);

	std::function<void(void)> m_init_opt_cb;
	std::function<void(void)> m_step_opt_cb;
	std::function<void(void)> m_csg_op_cb;

	SimulationStage GetStage() { return m_stage; }
};

