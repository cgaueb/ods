// Fill out your copyright notice in the Description page of Project Settings.


#include "OpeningEngine.h"
#include "PlanarSampler.h"
#include "ViewSampler.h"
#include "OpeningDomain.h"
#include "CuttedDynamicGeometry.h"
#include "CoreGlobals.h"

#include "EngineUtils.h"
#include "Modules/ModuleManager.h"
#include "Engine/SkyLight.h"
#include "Engine/TextureCube.h"
#include "Components/SkyLightComponent.h"

#include <assert.h>
#include <numeric>
#include <fstream>

//#define DEBUG_EXEC

enum class SAMPLING {
	REGULAR,
	RANDOM
};

enum class ALGORITHM {
	RANDOM,
	METROPOLIS,
	GAUSSIAN_PROCESS
};

constexpr SAMPLING SAMPLING_MODE = SAMPLING::RANDOM;
ALGORITHM SELECTED_ALGORITHM = ALGORITHM::RANDOM;

constexpr auto unmapPosX(float posX, const float domain_num)
{
	const float step = 1.0f / domain_num;
	float variable = step;
	for (float intpart = 0.0f; intpart < domain_num; intpart += 1.f, variable += step)
	{
		if (posX <= variable)
			return std::pair(intpart, posX * domain_num - intpart);
	}
	return std::pair(domain_num - 1.f, 1.f);
	
	/*float intpart = 1;
	float fractpart = modf(posX * domain_num, &intpart);
	return intpart == (int)domain_num? std::pair(domain_num - 1.f, 1.0f) : std::pair(intpart, fractpart); // Quick hack */
}

constexpr float mapPosX(float x, float domain_selected, const float domain_num)
{
	return (domain_selected + x) / domain_num;
}

// Sets default values
AOpeningEngine::AOpeningEngine()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	m_UniformF_distr = std::uniform_real_distribution<float>(0.f, 1.f);
	m_enable_optimization = false;
	//ConstructorHelpers::FClassFinder<AActor> CutterDynamicClassClassFinder(TEXT("/Game/BP_MyCutterDynamicActor.BP_MyCutterDynamicActor_C"));
	//CutterDynamicClass = CutterDynamicClassClassFinder.Class;
}

// Called when the game starts or when spawned
void AOpeningEngine::BeginPlay()
{
	Super::BeginPlay();
	m_enable_optimization = false;
	m_current_optimization_count = 0;
	UE_LOG(LogTemp, Warning, TEXT("OpeningDesign: Begin Play"));
}

bool AOpeningEngine::SetEnvMap(class UTextureCube* env_map) {
	TArray<UTextureCube*> EnvMaps;
	FindAllAssets(EnvMaps, "/Game/StarterContent/HDRI");

	USkyLightComponent* SkylightComponent = nullptr;
	if (m_skylight == nullptr) {
		TArray<ASkyLight*> SkyLights;
		FindAllActors(GetWorld(), SkyLights);
		if (SkyLights.Num() > 0) m_skylight = SkyLights[0];
		SkylightComponent = m_skylight->GetLightComponent();
		SkylightComponent->bRealTimeCapture = false;
		SkylightComponent->bLowerHemisphereIsBlack = false;
		SkylightComponent->CubemapResolution = 512;
		SkylightComponent->SourceType = SLS_SpecifiedCubemap;
		//SkylightComponent->SetLowerHemisphereColor({ 0,0,1,1 });
	}
	else {
		SkylightComponent = m_skylight->GetLightComponent();
	}

	if (SkylightComponent == nullptr) return false;
	if (SkylightComponent->Cubemap == env_map) return true;

	SkylightComponent->SetCubemap(env_map);

	return false;
}

void AOpeningEngine::MainLoop(float DeltaTime) {
	FString stage = FString("UNKNOW");
	switch(m_stage) {
		case LOTUS_STAGE_INIT: stage = "Init Stage"; break;
		case LOTUS_STAGE_OPT_STEP: stage = "Optimization Stage"; break;
		case LOTUS_STAGE_CSG_OPS: stage = "CSG Stage"; break;
		case LOTUS_STAGE_SET_MAX_ENV_MAP: stage = "Set MAX Env Map Stage"; break;
		case LOTUS_STAGE_SET_AVG_ENV_MAP: stage = "Set AVG Env Map Stage"; break;
		case LOTUS_STAGE_VIEW_SAMPLERS: stage = "View Sampler Stage"; break;
		case LOTUS_STAGE_PLANAR_SAMPLERS: stage = "Planar Sampler Stage"; break;
	}

	GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, FString::Printf(TEXT("Optimization Stage: %s"), *stage));

	if (LOTUS_STAGE_INIT == m_stage) {
		m_init_opt_cb();
		m_stage = LOTUS_STAGE_OPT_STEP;
	}
	else if (LOTUS_STAGE_OPT_STEP == m_stage) {
		if (m_current_optimization_count == m_max_optimization_steps + 1)
		{ // final cutters were rendered on previous ticks
			this->StopOptimization();
			m_opt_state.Total_time_in_seconds = FPlatformTime::Seconds() - m_opt_state.Total_time_in_seconds;
		}
		else
		{
			m_step_opt_cb();
			m_stage = LOTUS_STAGE_CSG_OPS;
		}
	}
	else if (LOTUS_STAGE_CSG_OPS == m_stage) {
		m_csg_op_cb();
		m_stage = LOTUS_STAGE_SET_MAX_ENV_MAP;
	}
	else if (LOTUS_STAGE_SET_MAX_ENV_MAP == m_stage) {
		if (SetEnvMap(m_max_env_map)) {
			m_stage = LOTUS_STAGE_VIEW_SAMPLERS;
			for (auto vsampler : m_view_samplers) {
				vsampler->SetShouldReset(true);
				vsampler->SetRenderingDone(false);
			}
		}
	}
	else if (LOTUS_STAGE_SET_AVG_ENV_MAP == m_stage) {
		if (SetEnvMap(m_avg_env_map)) {
			m_stage = LOTUS_STAGE_PLANAR_SAMPLERS;
			for (auto psampler : m_planar_samplers) {
				psampler->SetShouldReset(true);
				psampler->SetRenderingDone(false);
			}
		}
	}
	else if (LOTUS_STAGE_VIEW_SAMPLERS == m_stage) {
		bool all_view_samplers_done = true;
		for (const auto vsampler : m_view_samplers) {
			all_view_samplers_done &= vsampler->GetRenderingDone();
		}
		if(all_view_samplers_done)
			m_stage = LOTUS_STAGE_SET_AVG_ENV_MAP;
	} 
	else if (LOTUS_STAGE_PLANAR_SAMPLERS == m_stage) {
		bool all_planar_samplers_done = true;
		for (const auto psampler : m_planar_samplers) {
			all_planar_samplers_done &= psampler->GetRenderingDone();
		}
		if (all_planar_samplers_done)
			m_stage = LOTUS_STAGE_OPT_STEP;
	}
}

// Called every frame
void AOpeningEngine::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!m_enable_optimization) return;

	return MainLoop(DeltaTime);

	// always skip the first to let the engine initiallize everything
	if (m_current_optimization_count == m_max_optimization_steps)
	{
		m_current_optimization_count--;
		return;
	}

	this->EvaluateLoss();

	m_current_optimization_count--;

	// Set New State (MUTATE)
	this->ResetDomains();
	this->SampleOpeningDomain(false);

	if (m_current_optimization_count == 0)
	{
		this->ResetDomains();
		this->FinalizeOpenings();
	}
}

void AOpeningEngine::StartOptimization()
{
	SELECTED_ALGORITHM = ALGORITHM::RANDOM;

	m_enable_optimization = this->PrepareOptimizationComponents();
	if (!m_enable_optimization) return;

	/***********					INITIALIZE ALGORITHM				 ***********/

	// Initialize state
	m_current_optimization_count = 0;
	m_opt_state = FOptimizationState();

	int number_of_cutters = 0;
	int number_of_opt_variables = 0;
	for (int domainIndex = 0; domainIndex < m_opening_domains.Num(); domainIndex++)
	{
		auto domain = m_opening_domains[domainIndex];
		const int cutters = domain->m_number_of_cutters;
		number_of_opt_variables += cutters * domain->GetNumberOfVariables();
		number_of_cutters += cutters;
		for (int i = 0; i < cutters; ++i)
		{
			FOptimizationOpeningState state;
			state.parameters.Init(0, domain->GetNumberOfVariables());
			state.domainIndex = domainIndex;
			m_opt_state.best_cutter.Add(state);
			m_opt_state.previous_cutter.Add(state);
		}
	}
	m_opt_state.per_view_sampler_cost.SetNum(m_view_samplers.Num());
	m_opt_state.per_planar_sampler_cost.SetNum(m_planar_samplers.Num());
	m_opt_state.best_cost = FLT_MAX;

	for (int i = 0; i < m_view_samplers.Num(); ++i)
	{
		m_opt_state.per_view_sampler_cost[i].Sampler = m_view_samplers[i];
	}

	for (int i = 0; i < m_planar_samplers.Num(); ++i)
	{
		m_opt_state.per_planar_sampler_cost[i].Sampler = m_planar_samplers[i];
	}

	int max_number_of_openings = 0;
	for (auto domain : m_opening_domains)
	{
		max_number_of_openings += domain->m_max_cutters;
	}

#if 0
	if (number_of_cutters < m_opening_domains.Num() || number_of_cutters > max_number_of_openings)
	{
		UE_LOG(LogTemp, Error, TEXT("Requested Cutters should be [%d...%d]. %d requested"), m_opening_domains.Num(), max_number_of_openings, number_of_cutters);
		return;
	}
#endif

	// STUPID UNREAL, does not play with std
	auto shuffle_array = [&](const TArray<int>& arr) {
		int* temp_op = new int[arr.Num()];
		for (int32 i = 0; i < arr.Num(); ++i)
			temp_op[i] = arr[i];
		std::shuffle(temp_op, temp_op + arr.Num(), m_random_generator);
		TArray<int> out;
		out.SetNum(arr.Num());
		for (int32 i = 0; i < arr.Num(); ++i)
			out[i] = temp_op[i];
		delete[] temp_op;
	};

	m_init_opt_cb = [&]()
	{
		m_random_generator.seed(1337);
		this->ResetDomains();
		SampleOpeningDomainAG(true);
	};

	m_step_opt_cb = [&]()
	{
		// Compute the cost value
		float cost_value = this->EvaluateLoss();
		
		// Mutate state
		if (m_current_optimization_count >= m_max_optimization_steps)
		{
			TArray<double> sampleX;
			double sampleY = 0;
			//m_opt_state.best_cost = sampleY;
		}
		else // Explore
		{
			SampleOpeningDomainAG(false);
		}
	};

	m_csg_op_cb = [&]()
	{
		this->ResetDomains();

		if (m_current_optimization_count == m_max_optimization_steps)
		{
			// Optimum was set on the previous tick
			this->FinalizeOpenings();
		}
		else // Explore state
		{
			ApplyCutterTransforms();
		}

		++m_current_optimization_count;
	};

}

void AOpeningEngine::StartOptimizationSA()
{
	SELECTED_ALGORITHM = ALGORITHM::RANDOM;

	m_enable_optimization = this->PrepareOptimizationComponents();
	if (!m_enable_optimization) return;

	/***********					INITIALIZE ALGORITHM				 ***********/

	// Initialize state
	m_current_optimization_count = 0;
	m_opt_state = FOptimizationState();

	int number_of_cutters = 0;
	int number_of_opt_variables = 0;
	for (int domainIndex = 0; domainIndex < m_opening_domains.Num(); domainIndex++)
	{
		auto domain = m_opening_domains[domainIndex];
		const int cutters = domain->m_number_of_cutters;
		number_of_opt_variables += cutters * domain->GetNumberOfVariables();
		number_of_cutters += cutters;
		for (int i = 0; i < cutters; ++i)
		{
			FOptimizationOpeningState state;
			state.parameters.Init(0, domain->GetNumberOfVariables());
			state.domainIndex = domainIndex;
			m_opt_state.best_cutter.Add(state);
			m_opt_state.previous_cutter.Add(state);
		}
	}
	m_opt_state.per_view_sampler_cost.SetNum(m_view_samplers.Num());
	m_opt_state.per_planar_sampler_cost.SetNum(m_planar_samplers.Num());
	m_opt_state.best_cost = FLT_MAX;

	for (int i = 0; i < m_view_samplers.Num(); ++i)
	{
		m_opt_state.per_view_sampler_cost[i].Sampler = m_view_samplers[i];
	}

	for (int i = 0; i < m_planar_samplers.Num(); ++i)
	{
		m_opt_state.per_planar_sampler_cost[i].Sampler = m_planar_samplers[i];
	}

	// STUPID UNREAL, does not play with std
	auto shuffle_array = [&](const TArray<int>& arr) {
		int* temp_op = new int[arr.Num()];
		for (int32 i = 0; i < arr.Num(); ++i)
			temp_op[i] = arr[i];
		std::shuffle(temp_op, temp_op + arr.Num(), m_random_generator);
		TArray<int> out;
		out.SetNum(arr.Num());
		for (int32 i = 0; i < arr.Num(); ++i)
			out[i] = temp_op[i];
		delete[] temp_op;
	};

	struct SAOptimizationState {		

		float best_cost = FLT_MAX;
		TArray<FOptimizationOpeningState> best_cutter = { };

		float previous_cost = FLT_MAX;
		TArray<FOptimizationOpeningState> previous_cutter = { };

		int iterations = 0;
		int assignments_counter = 0;
		int prob_assignments_counter = 0;
		int rejected_counter = 0;

		float temperature = 0;
	};

	double e_var = 5.0; // need to do a starting sampling (100 samples) to estimate the e_var

	SAOptimizationState* state = new SAOptimizationState();
	float P_init = 0.7; // the initial probability to jump to a new worse state
	state->temperature = -e_var / log(P_init);
	state->temperature *= 0.9f;

	m_init_opt_cb = [&, state]()
	{
		m_random_generator.seed(1337);
		this->ResetDomains();
		SampleOpeningDomainAG(true);
		state->previous_cutter = m_opt_state.previous_cutter;
	};	

	m_step_opt_cb = [&, state]()
	{
		constexpr int MAX_ITERATIONS_PER_TEMPERATURE = 5;

		// Compute the cost value
		float cost_value = this->EvaluateLoss();
		state->previous_cost = cost_value;

		// Mutate state
		if (m_current_optimization_count >= m_max_optimization_steps)
		{
			TArray<double> sampleX;
			double sampleY = 0;
			//m_opt_state.best_cost = sampleY;
		}
		else // Explore
		{
			if (state->previous_cost < state->best_cost)
			{
				state->assignments_counter++;
				state->best_cost = state->previous_cost;
				state->best_cutter = state->previous_cutter;
			}
			else
			{
				double probability = exp((state->best_cost - state->previous_cost) / (state->temperature));
				double p = GenFloat();
				if (p <= probability)
				{
					state->best_cost = state->previous_cost;
					state->best_cutter = state->previous_cutter;
					state->prob_assignments_counter++;
				}
				else state->rejected_counter++;
			}
			
			state->iterations++;
			state->temperature *= (state->iterations % MAX_ITERATIONS_PER_TEMPERATURE == 0) ? 0.9f : 1.0f;

			const float domain_num = m_opening_domains.Num();
			const int number_of_cutters = m_opt_state.previous_cutter.Num();

			// Create all the available openings
			TArray<int> openings;

			// Mutate one cutter state
			int32 mutatedCutterIndex = rand() % number_of_cutters;
			{
				constexpr bool MUTATE_ALL_PARAMETERS = true;
				// Change all parameters of one domain
				if (MUTATE_ALL_PARAMETERS)
				{
					for (int i = 0; i < m_opt_state.previous_cutter[mutatedCutterIndex].parameters.Num(); i++)
					{
						m_opt_state.previous_cutter[mutatedCutterIndex].parameters[i] = GenFloat();
					}
				}
				else
				{
					// Change one parameter of one domain
					int i = rand() % m_opt_state.previous_cutter[mutatedCutterIndex].parameters.Num();
					m_opt_state.previous_cutter[mutatedCutterIndex].parameters[i] = GenFloat();
				}
				state->previous_cutter = m_opt_state.previous_cutter;
			}
		}
	};

	m_csg_op_cb = [&, state]()
	{
		this->ResetDomains();

		if (m_current_optimization_count == m_max_optimization_steps)
		{
			// Optimum was set on the previous tick
			this->FinalizeOpenings();
		}
		else // Explore state
		{
			ApplyCutterTransforms();
		}

		++m_current_optimization_count;
	};

}

bool AOpeningEngine::ShouldTickIfViewportsOnly() const
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

void AOpeningEngine::FindSamplers()
{
	m_view_samplers.Empty(10);
	m_planar_samplers.Empty(10);

	AActor* MyActor = nullptr;
	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			MyActor = *ActorItr;

			AViewSampler* view_sampler = Cast<AViewSampler>(MyActor);
			APlanarSampler* planar_sampler = Cast<APlanarSampler>(MyActor);

			if (view_sampler)
			{
				m_view_samplers.Add(view_sampler);
			}
			else if (planar_sampler)
			{
				m_planar_samplers.Add(planar_sampler);
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FOUND %d/%d View/Planar Samplers"), m_view_samplers.Num(), m_planar_samplers.Num());
}

/*void AOpeningEngine::PlayFireEffects2()
{
	UE_LOG(LogTemp, Warning, TEXT("PlayFireEffects"));
}*/


double AOpeningEngine::Loss(double YtrueMin, double YtrueMax, double Ytest)
{
	double RetLoss = 0;
	auto sigmoid = [](double x) { return 1.0 / (1.0 + std::exp(-x)); };
	//if (Ytest < YtrueMin) RetLoss = std::powf(YtrueMin - Ytest, 1);
	//if (Ytest > YtrueMax) RetLoss = std::powf(Ytest - YtrueMin, 1);
	if (Ytest < YtrueMin) RetLoss = 2 * sigmoid(-m_low_stiffness * (Ytest - YtrueMin)) - 1;
	if (Ytest > YtrueMax) RetLoss = 2 * sigmoid(m_high_stiffness * (Ytest - YtrueMax)) - 1;
	return RetLoss;
}

void AOpeningEngine::StopOptimization()
{
	m_enable_optimization = false;
	m_current_optimization_count = 0;
	m_stage = LOTUS_STAGE_INIT;
}

void AOpeningEngine::StartBayesOptimization()
{
	SELECTED_ALGORITHM = ALGORITHM::GAUSSIAN_PROCESS;

	m_enable_optimization = this->PrepareOptimizationComponents();
	if (!m_enable_optimization) return;

	m_init_opt_cb = [&]()
	{
		m_current_optimization_count = 0;
		m_max_optimization_steps = m_train_steps + m_explore_steps + 1; // zero tick
		m_opt_state = FOptimizationState();
		m_random_generator.seed(1337);

		int number_of_opt_variables = 0;
		for (int domainIndex = 0; domainIndex < m_opening_domains.Num(); domainIndex++)
		{
			auto domain = m_opening_domains[domainIndex];
			const int cutters = domain->m_number_of_cutters;
			number_of_opt_variables += cutters * domain->GetNumberOfVariables();

			for (int i = 0; i < cutters; ++i)
			{
				FOptimizationOpeningState state;
				state.parameters.Init(0, domain->GetNumberOfVariables());
				state.domainIndex = domainIndex;
				m_opt_state.best_cutter.Add(state);
				m_opt_state.previous_cutter.Add(state);
			}
		}

		m_opt_state.per_view_sampler_cost.SetNum(m_view_samplers.Num());
		m_opt_state.per_planar_sampler_cost.SetNum(m_planar_samplers.Num());

		for (int i = 0; i < m_view_samplers.Num(); ++i)
		{
			m_opt_state.per_view_sampler_cost[i].Sampler = m_view_samplers[i];
		}

		for (int i = 0; i < m_planar_samplers.Num(); ++i)
		{
			m_opt_state.per_planar_sampler_cost[i].Sampler = m_planar_samplers[i];
		}

		m_opt_state.best_cost = FLT_MAX;
		m_opt_state.Total_time_in_seconds = FPlatformTime::Seconds();
		m_global_num_of_dims = 0;
		m_opt_state.top_k_openings.SetNum(Top_k);

		Optimizer.SetTrainIterations(m_train_steps);
		Optimizer.SetIterations(m_max_optimization_steps);
		Optimizer.SetRelearnIterations(m_relearn_steps);
		Optimizer.SetLearningMethod(m_learning_method);
		Optimizer.SetForceJumpStepIters(m_force_jump);
		Optimizer.SetObservationNoise(m_noise_observation);
		Optimizer.SetStackThreshold(m_stack_threshold);
		Optimizer.SetEpsilonThreshold(m_epsilon_step);
		Optimizer.SetExploreExpoit(m_explore_exploit);
		Optimizer.SetCriteriaMethod(m_criteria_method);
		Optimizer.SetPrior(m_prior);
		Optimizer.SetKernelPrior(m_kernel_prior);
		Optimizer.SetKernelMethod(m_kernel_method);
		Optimizer.SetSurrogateMethod(m_surrogate_method);
		Optimizer.SetStudentParams(m_nig_params);
		Optimizer.SetLearnAll(Learn_all);
		Optimizer.InitOptimizer(number_of_opt_variables);
		this->ResetDomains();
	};

	m_step_opt_cb = [&]()
	{
		// Waiting for the first CG operation
		if (m_current_optimization_count == 0) { return; }

		if (m_current_optimization_count == m_max_optimization_steps)
		{
			TArray<double> sampleX;
			double sampleY = 0;
			Optimizer.GetOptimum(sampleX, &sampleY);
			m_opt_state.best_cost = sampleY;
			
			this->BuildCutterDataPoint(m_opt_state.best_cutter, sampleX);
		}
		else if (m_current_optimization_count <= m_train_steps) // Gather train data
		{
			TArray<double> sampleX;
			double sampleY = 0;
			this->BuildBayesOptDataPoint(sampleX, &sampleY);
			Optimizer.AddSample(sampleX, sampleY);
		}
		else // Explore
		{
			if (m_current_optimization_count == m_train_steps + 1)
			{
				double elapsed_time = Optimizer.FitModel();
				//m_opt_state.Total_time_in_seconds += elapsed_time;

#ifdef DEBUG_EXEC
				UE_LOG(LogTemp, Warning, TEXT("BayesOpt fitting: %.2f"), elapsed_time);
#endif
			}
			else // Refit the model from previous iteration. We only refit on new test samples
			{
				TArray<double> sampleX;
				double sampleY = 0;
				this->BuildBayesOptDataPoint(sampleX, &sampleY);
				double elapsed_time = Optimizer.ReFitModel(sampleX, sampleY);
				//m_opt_state.Total_time_in_seconds += elapsed_time;

#ifdef DEBUG_EXEC
				UE_LOG(LogTemp, Warning, TEXT("BayesOpt refitting: %.2f"), elapsed_time);
#endif
			}
		}
	};

	m_csg_op_cb = [&]()
	{
		this->ResetDomains();

		if (m_current_optimization_count == m_max_optimization_steps)
		{
			// Optimum was set on the previous tick
			FinalizeOpenings();
		}
		else if (m_current_optimization_count < m_train_steps) 
		{// m_current_optimization_count == m_train_steps, we fit the model
			TArray<double> sampleX;
			Optimizer.GetTrainStep(sampleX, m_current_optimization_count);
			this->BuildCutterDataPoint(m_opt_state.previous_cutter, sampleX);
			this->ApplyCutterTransforms(m_opt_state.previous_cutter);
			//this->SampleOpeningDomain(true);
		}
		else if (m_current_optimization_count > m_train_steps) // Explore state
		{
			TArray<double> sampleX;
			double elapsed_time = Optimizer.GetNextStep(sampleX);
			//m_opt_state.Total_time_in_seconds += elapsed_time;

#ifdef DEBUG_EXEC
			UE_LOG(LogTemp, Warning, TEXT("BayesOpt nextStep: %.2f"), elapsed_time);
#endif
			this->BuildCutterDataPoint(m_opt_state.previous_cutter, sampleX);
			//this->LogArray(FString("BayesOpt next step: "), sampleX);
			this->ApplyCutterTransforms(m_opt_state.previous_cutter);
		}

		++m_current_optimization_count;
	};
}

void AOpeningEngine::StartDebugBayesCostFunction()
{
	FindSamplers();
	//Optimizer.InitOptimizer(2);

	PredLossValues.SetNum(m_num_samples * m_num_samples);
	m_opt_state = FOptimizationState();
	m_enable_debug_bayes = true;
	m_enable_optimization = true;
	m_current_optimization_count = 0;
	m_current_sample = 0;

	float x = rand() / (float)RAND_MAX;
	float y = rand() / (float)RAND_MAX;

	auto domain = m_opening_domains[0];
	domain->ApplyTransformFromParameterization(x, y);
}

void AOpeningEngine::StartDebugCostFunction()
{
	FindSamplers();
	RefLossValues.SetNum(m_num_samples * m_num_samples);
	m_opt_state = FOptimizationState();
	m_enable_debug_cost = true;
	m_current_sample = 0;
	this->DebugSetCutter();
}

void AOpeningEngine::TickDebugCostFunction()
{
	if (!m_enable_debug_cost) return;
	if (GFrameCounter == 0) return;
	if (m_current_sample >= RefLossValues.Num())
	{
		m_current_sample = 0;
		m_enable_debug_cost = false;
		m_opening_domains[0]->BuildDebugTexture(RefLossValues, m_num_samples);
		return;
	}

	//RefLossValues[m_current_sample++] = this->Loss_L1(m_planar_sampler_target.X, this->EvaluateCost(m_planar_samplers[0]));
	UE_LOG(LogTemp, Warning, TEXT("OpeningDesign: %d - %.1f"), m_current_sample - 1, RefLossValues[m_current_sample - 1]);
	this->DebugSetCutter();
}

void AOpeningEngine::TickDebugBayesFunction()
{
	if (!m_enable_debug_bayes) return;
	if (GFrameCounter == 0) return;
	if (m_current_sample >= PredLossValues.Num())
	{
		m_current_sample = 0;
		m_enable_debug_bayes = false;
		m_opening_domains[0]->BuildDebugTexture(PredLossValues, m_num_samples);
		return;
	}

	//this->TickBayesOptimization();

	if (!m_enable_optimization)
	{
		if (m_current_sample == 0)
		{
			++m_current_sample;
			this->DebugSetCutter();
			return;
		}

		int prev_sample = m_current_sample - 1;
		int dim = m_num_samples;
		int x = prev_sample / dim;
		int y = prev_sample % dim;

		float fx = x / float(dim);
		float fy = y / float(dim);

		TArray<double> sampleX;
		sampleX.SetNum(2);
		sampleX[0] = fx;
		sampleX[1] = fy;
		//Optimizer.GetResponseSurfaceAt(sampleX, &PredLossValues[prev_sample]);
		UE_LOG(LogTemp, Warning, TEXT("OpeningDesign: %d - %.1f"), prev_sample, PredLossValues[prev_sample]);
		++m_current_sample;
		this->DebugSetCutter();
	}
}

void AOpeningEngine::DebugSetCutter()
{
	int dim = m_num_samples;
	int x = m_current_sample / dim;
	int y = m_current_sample % dim;

	float fx = x / float(dim);
	float fy = y / float(dim);

	auto domain = m_opening_domains[0];
	domain->ApplyTransformFromParameterization(fx, fy);
}

bool AOpeningEngine::PrepareOptimizationComponents()
{
	// Start
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::White, TEXT("START !!!"));

	// Initialize the random generator
	//std::random_device rd;
	m_random_generator.seed(1);

	/***********					CHECK IF EVERYTHING HAS BEEN SETUPED CORRECTLY				 ***********/

	int number_of_parameters = 0;
	for (int domainIndex = 0; domainIndex < m_opening_domains.Num(); domainIndex++)
	{
		auto domain = m_opening_domains[domainIndex];
		const int variables = domain->m_number_of_cutters * domain->GetNumberOfVariables();
		number_of_parameters += variables;
	}
	UE_LOG(LogTemp, Error, TEXT("OpeningDesign: START (%d)!!!"), number_of_parameters);

	// Find the samplers in the scene
	FindSamplers();
	for (auto sampler : m_planar_samplers)
		sampler->SetLightEfficacy(m_light_efficacy);
	for (auto sampler : m_view_samplers)
		sampler->SetLightEfficacy(m_light_efficacy);
	if (m_view_samplers.IsEmpty() && m_planar_samplers.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("OpeningDesign: Planar and View Samplers are empty"));
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::White, TEXT("OpeningDesign: Planar and View Samplers are empty"));
		return false;
	}

	// Find the cutters in the scene
	if (m_opening_domains.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("OpeningDesign: Domains are empty"));
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Red, TEXT("OpeningDesign: Domains are empty"));
		return false;
	}
	// check if an opening does not have a cutter assigned
	for (int32 i = 0; i < m_opening_domains.Num(); ++i)
	{
		auto domain = m_opening_domains[i];
		ACuttedDynamicGeometry* cutted = domain->CuttedMesh;
		if (cutted == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("OpeningEngine: Missing Cutted"));
			return false;
		}
	}

	// Check if cutter number is viable
	{
		// No need. Max is unused for now
#if 0
		int available_cutters = 0;
		for (int32 i = 0; i < m_opening_domains.Num(); ++i)
			available_cutters += m_opening_domains[i]->m_max_cutters;
		if (number_of_cutters > available_cutters)
		{
			UE_LOG(LogTemp, Error, TEXT("OpeningEngine: Requested Cutters are more than the domains"));
			return false;
		}
#endif
	}

	return true;
}

void AOpeningEngine::ResetDomains()
{
	// Reset openings
	for (int32 i = 0; i < m_opening_domains.Num(); ++i)
	{
		m_opening_domains[i]->ResetCutted();
	}
}

void AOpeningEngine::SampleOpeningDomain(bool MutateAll)
{
	SampleOpeningDomainAG(MutateAll);

	this->ApplyCutterTransforms();
}

void AOpeningEngine::SampleOpeningDomainAG(bool MutateAll)
{
	const float domain_num = m_opening_domains.Num();
	const int number_of_cutters = m_opt_state.previous_cutter.Num();

	// Create all the available openings
	TArray<int> openings;

	if (MutateAll)
	{
		// Random initialization
		for (int32 cutterIndex = 0; cutterIndex < number_of_cutters; ++cutterIndex)
		{
			for (int i = 0; i < m_opt_state.previous_cutter[cutterIndex].parameters.Num(); i++)
			{
				m_opt_state.previous_cutter[cutterIndex].parameters[i] = GenFloat();
			}
		}
	}
	else
	{
		// Mutate one cutter state
		int32 mutatedCutterIndex = rand() % number_of_cutters;
		{
			for (int i = 0; i < m_opt_state.previous_cutter[mutatedCutterIndex].parameters.Num(); i++)
			{
				m_opt_state.previous_cutter[mutatedCutterIndex].parameters[i] = GenFloat();
			}
		}
	}
}

void AOpeningEngine::ApplyCutterTransforms()
{
	const int domain_num = m_opening_domains.Num();
	const int number_of_cutters = m_opt_state.previous_cutter.Num();

	// Re-apply transformations
	for (int32 cutterIndex = 0; cutterIndex < number_of_cutters; cutterIndex++)
	{
		float parameters[6] = {-1,-1,-1,-1,-1,-1};
		for (int i = 0; i < m_opt_state.previous_cutter[cutterIndex].parameters.Num(); i++)
		{
			parameters[i] = m_opt_state.previous_cutter[cutterIndex].parameters[i];
		}
		const auto [x1, x2, x3, x4, x5, x6] = parameters;
		const int domainIndex = m_opt_state.previous_cutter[cutterIndex].domainIndex;
		auto domain = m_opening_domains[domainIndex]->ApplyTransformFromParameterization(x1, x2, x3, x4, x5, x6);
	}
}

float AOpeningEngine::GenFloat()
{
	return m_UniformF_distr(m_random_generator);
}

FVector4f AOpeningEngine::GenFloat4()
{
	return FVector4f{ this->GenFloat(), this->GenFloat(), this->GenFloat(), this->GenFloat() };
}

int AOpeningEngine::GenInt(int num)
{
	std::uniform_int_distribution<> distrib(0, num);
	return distrib(m_random_generator);
}

float AOpeningEngine::EvaluateOverlapLoss() {
	auto SumOverlapLoss = 0.0f;
	const auto DomainNum = m_opening_domains.Num(); 

	for (const auto& Domain : m_opening_domains) {
		const auto& Bounds = Domain->GetOpeningsBBOX();
		const auto  BoundsNum = Bounds.Num();
		for (auto IndexA = 0; IndexA < BoundsNum; IndexA++) {
			const auto& BoxA = Bounds[IndexA];
			for (auto IndexB = IndexA + 1; IndexB < BoundsNum; IndexB++) {
				const auto& BoxB = Bounds[IndexB];
				const auto& BoxOverlap = BoxA.Overlap(BoxB);
				const auto  MaxVolume = BoxA.GetVolume() + BoxB.GetVolume();
				SumOverlapLoss += BoxOverlap.GetVolume() / MaxVolume;
			}
		}
	}
	return SumOverlapLoss;
}

float AOpeningEngine::EvaluateLoss()
{
	// Evaluate Previous State
	float sampler_loss = 0;
	float sampler_cost = 0;

	TArray<FSamplerPair> per_view_sampler_cost = m_opt_state.per_view_sampler_cost;
	TArray<FSamplerPair> per_planar_sampler_cost = m_opt_state.per_planar_sampler_cost;

	for (int i = 0; i < m_view_samplers.Num(); ++i)
	{
		AViewSampler::RetColorStats stats = m_view_samplers[i]->GetColor();
		auto goal = m_view_samplers[i]->m_illumination_goal;
		double sampler_value = stats.maxValue.Length();
		double view_loss = this->Loss(goal.min_value, goal.max_value, sampler_value);
		
		//UE_LOG(LogTemp, Warning, TEXT("OpeningDesign: View[%d] Color %.2f %.2f %.2f"), i, stats.average.X, stats.average.Y, stats.average.Z);
		per_view_sampler_cost[i].Value = { sampler_value, view_loss };
		sampler_loss += view_loss;
		sampler_cost += sampler_value;
	}

	for (int i = 0; i < m_planar_samplers.Num(); ++i)
	{
		APlanarSampler::RetColorStats stats = m_planar_samplers[i]->GetColor();
		auto goal = m_planar_samplers[i]->m_illumination_goal;

		double sampler_value = stats.illuminance.Length();
		double planar_loss = this->Loss(goal.min_value, goal.max_value, sampler_value);
		per_planar_sampler_cost[i].Value = { sampler_value, planar_loss };
		//UE_LOG(LogTemp, Warning, TEXT("OpeningDesign: Planar[%d] Illuminance: %.4f - %.4f, %.1f %.1f"), i, stats.illuminance.Length(), planar_loss, goal.min_value, goal.max_value);
		sampler_loss += planar_loss;
		sampler_cost += sampler_value;
	}

	float penalty = m_penalty_multiplier * this->EvaluateOverlapLoss();
	float sum_loss = sampler_loss + penalty;

	UE_LOG(LogTemp, Error, TEXT("OpeningDesign: Iter: %d - Loss: %.2f Penalty: %.2f"),
		m_current_optimization_count,
		sampler_loss, penalty);

	// Save the best state
	if (sum_loss < m_opt_state.best_loss)
	{
		m_opt_state.best_loss = sum_loss; // L2 distance from (0,0,0)
		m_opt_state.best_penalty = penalty;
		m_opt_state.best_cost = sampler_cost;
		m_opt_state.best_cutter = m_opt_state.previous_cutter;
		m_opt_state.per_view_sampler_cost = per_view_sampler_cost;
		m_opt_state.per_planar_sampler_cost = per_planar_sampler_cost;

		FString text = FString::Printf(TEXT("Best Found (%f) at %d iter!!!"), sum_loss, m_current_optimization_count);
		UE_LOG(LogTemp, Error, TEXT("OpeningDesign: Best Found at %d iter"), m_current_optimization_count);
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(2, 5.f, FColor::Red, text);
	}

	return sum_loss;
}

void AOpeningEngine::FinalizeOpenings()
{
	ApplyCutterTransforms(m_opt_state.best_cutter);

	// print each sampler cost value
	for (auto sampler : m_opt_state.per_planar_sampler_cost)
	{
		auto planar = Cast<APlanarSampler>(sampler.Sampler);
		if (planar)
		{
			planar->m_illuminance_cost = { (float)sampler.Value.X, (float)sampler.Value.Y };
		}
	}
	for (auto sampler : m_opt_state.per_view_sampler_cost)
	{
		auto view = Cast<AViewSampler>(sampler.Sampler);
		if (view)
		{
			view->m_luminance_cost = { (float)sampler.Value.X, (float)sampler.Value.Y };
		}
	}
}

void AOpeningEngine::ApplyCutterTransforms(const TArray<FOptimizationOpeningState>& Cutters)
{
	const int domain_num = m_opening_domains.Num();
	const int number_of_cutters = Cutters.Num();

	// get the best domain
	for (int32 cutterIndex = 0; cutterIndex < number_of_cutters; cutterIndex++)
	{
		float parameters[6] = { -1,-1,-1,-1,-1,-1 };
		for (int i = 0; i < Cutters[cutterIndex].parameters.Num(); i++)
		{
			parameters[i] = Cutters[cutterIndex].parameters[i];
		}
		const auto [x1, x2, x3, x4, x5, x6] = parameters;
		const int domainIndex = Cutters[cutterIndex].domainIndex;
		auto domain = m_opening_domains[domainIndex]->ApplyTransformFromParameterization(x1, x2, x3, x4, x5, x6);
	}
}

void AOpeningEngine::BuildBayesOptDataPoint(TArray<double>& X, double* Y)
{
	const int number_of_cutters = m_opt_state.previous_cutter.Num();
	int number_of_variables = 0;
	for (int32 cutterIndex = 0; cutterIndex < number_of_cutters; ++cutterIndex)
		number_of_variables += m_opt_state.previous_cutter[cutterIndex].parameters.Num();

	X.SetNum(number_of_variables);

	int offset = 0;
	for (int32 cutterIndex = 0; cutterIndex < number_of_cutters; ++cutterIndex)
	{
		for (int dim_i = 0; dim_i < m_opt_state.previous_cutter[cutterIndex].parameters.Num(); ++dim_i)
		{
			X[offset++] = m_opt_state.previous_cutter[cutterIndex].parameters[dim_i];
		}
	}

	*Y = this->EvaluateLoss();
	this->CacheCutterSolution(m_opt_state.previous_cutter, *Y);
}

void AOpeningEngine::BuildCutterDataPoint(TArray<FOptimizationOpeningState>& Cutters, const TArray<double>& X)
{
	int offset = 0;
	for (int32 cutterIndex = 0; cutterIndex < Cutters.Num(); ++cutterIndex)
	{
		for (int dim_i = 0; dim_i < Cutters[cutterIndex].parameters.Num(); ++dim_i)
		{
			Cutters[cutterIndex].parameters[dim_i] = X[offset++];
		}
	}
}

void AOpeningEngine::LogArray(const FString& prefix, const TArray<double>& Array)
{
	FString array_s = {};

	for (int i = 0; i < Array.Num(); ++i)
	{
		array_s += FString::SanitizeFloat(Array[i], 2) + " ";
	}

	UE_LOG(LogTemp, Warning, TEXT("%s"), *(prefix + array_s));
}

void AOpeningEngine::CacheCutterSolution(const TArray<FOptimizationOpeningState>& Cutters, double cost)
{
	int k = 0;
	FOpeningPair curTmp, prevTmp;
	for (k; k < m_opt_state.top_k_openings.Num(); ++k)
	{
		if (cost < m_opt_state.top_k_openings[k].cost)
		{
			curTmp = m_opt_state.top_k_openings[k];
			m_opt_state.top_k_openings[k].cutter = Cutters;
			m_opt_state.top_k_openings[k].cost = cost;

			if (k == m_opt_state.top_k_openings.Num() - 1) break;

			for (int i = k; i < m_opt_state.top_k_openings.Num() - 1; ++i)
			{
				prevTmp = m_opt_state.top_k_openings[i + 1];
				m_opt_state.top_k_openings[i + 1] = curTmp;
				curTmp = prevTmp;
			}

			break;
		}
	}
}

void AOpeningEngine::Apply_k_Opening()
{
	const int numSolutions = m_opt_state.top_k_openings.Num();
	int opening_i = Apply_k;

	if (Apply_k >= numSolutions)
	{
		UE_LOG(LogTemp, Error, TEXT("Requesting cutter solution larger than the cached size. Falling back to 0."));
		opening_i = 0;
	}

	this->ResetDomains();
	this->ApplyCutterTransforms(m_opt_state.top_k_openings[opening_i].cutter);
	m_opt_state.best_cutter = m_opt_state.top_k_openings[opening_i].cutter;
	m_opt_state.best_cost = m_opt_state.top_k_openings[opening_i].cost;
	m_opt_state.best_loss = m_opt_state.top_k_openings[opening_i].loss;
	m_opt_state.best_penalty = m_opt_state.top_k_openings[opening_i].penalty;
}

void AOpeningEngine::PrintSamplerStats()
{
	this->PrepareOptimizationComponents();
	m_enable_optimization = false;

	auto sigmoid = [](double x) { return 1.0 / (1.0 + std::exp(-x)); };
	auto lowCost = [&](double ref, double pred) { return 2 * sigmoid(-m_low_stiffness * (pred - ref)) - 1; };
	auto highCost = [&](double ref, double pred) { return 2 * sigmoid(m_high_stiffness * (pred - ref)) - 1; };

	const TArray<FSamplerPair>& per_view_sampler_cost = m_opt_state.per_view_sampler_cost;
	const TArray<FSamplerPair>& per_planar_sampler_cost = m_opt_state.per_planar_sampler_cost;

	int planarGoalsReached = 0;
	int viewGoalsReached = 0;
	double globalCost = 0;
	double viewLoss = 0;
	double planarLoss = 0;
	FVector2f planarDiff = { 0, 0 };
	FVector2f viewDiff = { 0, 0 };
	FIntVector2 planarCount = { 0, 0 };
	FIntVector2 viewCount = { 0, 0 };

	for (int i = 0; i < per_view_sampler_cost.Num(); ++i)
	{
		auto goal = Cast<AViewSampler>(per_view_sampler_cost[i].Sampler)->m_illumination_goal;
		double sampler_value = per_view_sampler_cost[i].Value.X;
		//viewLoss += per_view_sampler_cost[i].Value.Y;
		viewLoss += this->Loss(goal.min_value, goal.max_value, per_view_sampler_cost[i].Value.X);

		if (sampler_value < goal.min_value)
		{
			viewDiff.X += std::abs(sampler_value - goal.min_value);
			++viewCount.X;
		}
		else if (sampler_value > goal.max_value)
		{
			viewDiff.Y += std::abs(sampler_value - goal.max_value);
			++viewCount.Y;
		}
		else
		{
			viewGoalsReached += 1;
		}

		UE_LOG(LogTemp, Warning, TEXT("View[%d], cost=%.2f, min=%.2f, max=%.2f"), i, sampler_value, goal.min_value, goal.max_value);
	}

	for (int i = 0; i < per_planar_sampler_cost.Num(); ++i)
	{
		auto goal = Cast<APlanarSampler>(per_planar_sampler_cost[i].Sampler)->m_illumination_goal;
		double sampler_value = per_planar_sampler_cost[i].Value.X;
		//planarLoss += per_planar_sampler_cost[i].Value.Y;
		planarLoss += this->Loss(goal.min_value, goal.max_value, per_planar_sampler_cost[i].Value.X);

		if (sampler_value < goal.min_value)
		{
			planarDiff.X += std::abs(sampler_value - goal.min_value);
			++planarCount.X;
		}
		else if (sampler_value > goal.max_value)
		{
			planarDiff.Y += std::abs(sampler_value - goal.max_value);
			++planarCount.Y;
		}
		else
		{
			planarGoalsReached += 1;
		}

		UE_LOG(LogTemp, Warning, TEXT("Planar[%d], cost=%.2f, min=%.2f, max=%.2f"), i, sampler_value, goal.min_value, goal.max_value);
	}

	UE_LOG(LogTemp, Warning,
		TEXT("Global loss %.4f - View sampler loss %.4f - Planar sampler loss %.2f - Low stiffness %.4f, High stiffness %.4f"),
		viewLoss + planarLoss, viewLoss, planarLoss,
		m_low_stiffness, m_high_stiffness);

	UE_LOG(LogTemp, Warning,
		TEXT("Planar sampler goals reached %d/%d - View sampler goals reached %d/%d"),
		planarGoalsReached, per_planar_sampler_cost.Num(),
		viewGoalsReached, per_view_sampler_cost.Num());

	UE_LOG(LogTemp, Warning,
		TEXT("Planar sampler avg. err [low/high] %.2f/%.2f - View sampler avg. err [low/high] %.2f/%.2f"),
		planarCount.X > 0 ? planarDiff.X / planarCount.X : 0, planarCount.Y > 0 ? planarDiff.Y / planarCount.Y : 0,
		viewCount.X > 0 ? viewDiff.X / viewCount.X : 0, viewCount.Y > 0 ? viewDiff.Y / viewCount.Y : 0);
}