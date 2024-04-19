#include "BayesOptimizer.hpp"

#include "Core.h"
#include <assert.h>

BayesOptimizer::BayesOptimizer()
{
	FString basePath = FPaths::Combine(FPaths::ProjectDir(),
		"ThirdParty", "BayesOpt", "bin");

	this->PathToDLL = FPaths::Combine(basePath, "Release", "bayesopt.dll");
	this->LoadDLL();

	ModelParams = initialize_parameters_to_default();
}

BayesOptimizer::~BayesOptimizer()
{
	if (this->LibraryHandle)
	{
		FPlatformProcess::FreeDllHandle(this->LibraryHandle);
		this->LibraryHandle = nullptr;
	}

	releaseOptimizer(this->Model);
}

void BayesOptimizer::SetTrainIterations(int NumSamples)
{
	this->UniformSamples.SetNum(NumSamples);
}

void BayesOptimizer::SetIterations(int NumIters)
{
	ModelParams.n_iterations = NumIters;
}

void BayesOptimizer::SetRelearnIterations(int NumIters)
{
	ModelParams.n_iter_relearn = NumIters;
}

void BayesOptimizer::SetLearningMethod(const FName& Method)
{
	if(Method == "Empirical")
		ModelParams.l_type = L_EMPIRICAL;
	else
		ModelParams.l_type = L_MCMC;
}

void BayesOptimizer::SetExploreExpoit(float Epsilon)
{
	ModelParams.crit_params[0] = Epsilon;
	ModelParams.n_crit_params = Epsilon >= 0. ? 1 : 0;
}

void BayesOptimizer::SetCriteriaMethod(const FName& Method)
{
	const TArray<FName>& methods = BayesOptimizer::CriteriaMethods();

	if (Method == methods[0])
		set_criteria(&ModelParams, "cPOI");
	else if (Method == methods[1])
		set_criteria(&ModelParams, "cEI");
	else if (Method == methods[2])
		set_criteria(&ModelParams, "cEIa");
	else if (Method == methods[3])
		set_criteria(&ModelParams, "cLCB");
	else if (Method == methods[4])
		set_criteria(&ModelParams, "cLCBa");
	else if (Method == methods[5])
		set_criteria(&ModelParams, "cThompsonSampling");
	else if (Method == methods[6])
		set_criteria(&ModelParams, "cMI");
	else if (Method == methods[7])
		set_criteria(&ModelParams, "cHedge(cEI, cPOI, cMI, cLCB)");
	else if (Method == methods[8])
		set_criteria(&ModelParams, "cHedge(cEI, cPOI, cMI, cLCB, cAopt, cExpReturn, cOptimisticSampling, cThompsonSampling)");
	else
		set_criteria(&ModelParams, "cLCB");
}

void BayesOptimizer::SetForceJumpStepIters(int Iters)
{
	ModelParams.force_jump = Iters;
}

void BayesOptimizer::SetObservationNoise(float Noise)
{
	ModelParams.noise = Noise;
}

void BayesOptimizer::SetStackThreshold(float Noise)
{
	ModelParams.noise_jump = Noise;
}

void BayesOptimizer::SetEpsilonThreshold(float Epsilon)
{
	ModelParams.epsilon = Epsilon;
}

void BayesOptimizer::SetPrior(const FVector2f& ModelPrior)
{
	set_mean(&ModelParams, "mConst");
	ModelParams.mean.coef_mean[0] = ModelPrior.X;
	ModelParams.mean.coef_std[0] = ModelPrior.Y;
	ModelParams.mean.n_coef = 1;
	ModelParams.sigma_s = ModelPrior.Y;
}

void BayesOptimizer::SetKernelMethod(const FName& Method)
{
	const TArray<FName>& methods = BayesOptimizer::KernelMethods();

	if (Method == methods[0])
		set_kernel(&ModelParams, "kSEISO");
	else if (Method == methods[1])
		set_kernel(&ModelParams, "kSEARD");
	else if (Method == methods[2])
		set_kernel(&ModelParams, "kMaternISO5");
	else if (Method == methods[3])
		set_kernel(&ModelParams, "kMaternARD5");
	else
		set_kernel(&ModelParams, "kSEISO");
}

void BayesOptimizer::SetKernelPrior(const FVector2f& KernelPrior)
{
	ModelParams.kernel.hp_mean[0] = KernelPrior.X;
	ModelParams.kernel.hp_std[0] = KernelPrior.Y;
	ModelParams.kernel.n_hp = 1;
}

void BayesOptimizer::SetStudentParams(const FVector2f& Params)
{
	ModelParams.alpha = Params.X;
	ModelParams.beta = Params.Y;
}

void BayesOptimizer::SetSurrogateMethod(const FName& Method)
{
	const TArray<FName>& methods = BayesOptimizer::SurrogateMethods();

	if (Method == methods[0])
		set_surrogate(&ModelParams, "sGaussianProcessML");
	else if (Method == methods[1])
		set_surrogate(&ModelParams, "sStudentTProcessNIG");
	else
		set_surrogate(&ModelParams, "sGaussianProcessML");
}

void BayesOptimizer::SetLearnAll(bool Value)
{
	ModelParams.l_all = Value;
}

void BayesOptimizer::SetupInternalParameters(bopt_params& Params)
{
	//set_kernel(&Params, "kSum(kSEISO,kSEISO)");
	
	Params.sc_type = SC_MAP;
	Params.random_seed = 1337;
	Params.n_inner_iterations = 500;
	Params.verbose_level = 0; // Dont enable, it does not work as intended
}

void BayesOptimizer::LoadDLL()
{
	this->LibraryHandle = !this->PathToDLL.IsEmpty() ?
		FPlatformProcess::GetDllHandle(*PathToDLL) : nullptr;

	if (!this->LibraryHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load BayesOpt DLL."));
	}
}

double BayesOptimizer::ReFitModel(TArray<double>& X, double Y)
{
	/*
	for (int dim_i = 0; dim_i < this->NumDims; ++dim_i)
	{
		if (X[dim_i] < 0 || X[dim_i] > 1)
			UE_LOG(LogTemp, Warning, TEXT("Adding out of bounding box training sample."));
	}*/

	double start = FPlatformTime::Seconds() * 1000;
	updateOptimizer(this->Model, X.GetData(), Y);
	double end = FPlatformTime::Seconds() * 1000;

	this->Dataset.Add({ X, Y });
	return end - start;
}

void BayesOptimizer::GetArgMin(TArray<double>& X)
{
	X.SetNum(this->NumDims);
	argMin(this->Model, X.GetData());
}

void BayesOptimizer::GetMinValue(double* Y)
{
	minValue(this->Model, Y);
}

void BayesOptimizer::GetOptimum(TArray<double>& X, double* Y)
{
	this->GetArgMin(X);
	this->GetMinValue(Y);
}

double BayesOptimizer::GetNextStep(TArray<double>& X)
{
	X.SetNum(this->NumDims);
	double start = FPlatformTime::Seconds() * 1000;
	const bool err = OptAcquisition(this->Model, X.GetData());
	double end = FPlatformTime::Seconds() * 1000;

	if (err) { UE_LOG(LogTemp, Error,
		TEXT("BayesOpt: Failed to Optimize Acquisition.\nFalling back to pertrubation of optimal value.")); }

	return end - start;
}

void BayesOptimizer::GetTrainStep(TArray<double>& X, int SampleIdx)
{
	X = this->UniformSamples[SampleIdx];
}

void BayesOptimizer::AddSample(const TArray<double>& X, const double Y)
{
	this->Dataset.Add({ X, Y });
}

void BayesOptimizer::InitOptimizer(const int InNumDims)
{
	releaseOptimizer(this->Model);

	this->SetupInternalParameters(this->ModelParams);
	this->NumDims = InNumDims;
	double low[128], up[128];

	for (int i = 0; i < this->NumDims; ++i)
	{
		low[i] = 0.;
		up[i] = 1.;
	}

	createOptimizer(this->Model, this->ModelParams, this->NumDims, low, up);

	double** X = new double* [this->UniformSamples.Num()];
	for (int sample_i = 0; sample_i < this->UniformSamples.Num(); ++sample_i)
	{
		X[sample_i] = new double[this->NumDims];
		this->UniformSamples[sample_i].SetNum(this->NumDims);
	}

	generateLHSamples(this->Model, X, this->UniformSamples.Num(), this->NumDims);

	for (int sample_i = 0; sample_i < this->UniformSamples.Num(); ++sample_i)
	{
		memcpy_s(this->UniformSamples[sample_i].GetData(),
			this->NumDims * sizeof(double),
			X[sample_i],
			this->NumDims * sizeof(double));

		delete[] X[sample_i];
	}

	delete[] X;

	//attachLog(this->Model);
	this->Dataset.Empty();
}

double BayesOptimizer::FitModel()
{
	if (this->Dataset.Num() == 0 || !this->Model)
	{
		UE_LOG(LogTemp, Warning, TEXT("Calling model fit on empty dataset or null model."));
		return 0;
	}

	//Unoptimized but safe

	double** X = new double*[this->Dataset.Num()];
	double* Y = new double[this->Dataset.Num()];

	for (int sample_i = 0; sample_i < this->Dataset.Num(); ++sample_i)
	{
		const TDataPoint& sample = this->Dataset[sample_i];
		Y[sample_i] = sample.Value;
		X[sample_i] = new double[this->NumDims];
		memcpy_s(
			X[sample_i], sizeof(double) * this->NumDims,
			sample.Key.GetData(), sizeof(double) * this->NumDims);
	}

	double start = FPlatformTime::Seconds() * 1000;
	initOptimizerContinuous(this->Model, X, Y, this->Dataset.Num());
	double end = FPlatformTime::Seconds() * 1000;

	//const char* log = nullptr;
	//int logSize = viewLog(this->Model, log);

	for (int sample_i = 0; sample_i < this->Dataset.Num(); ++sample_i)
	{
		delete[] X[sample_i];
	}

	delete[] X;
	delete[] Y;

	return end - start;
}

void BayesOptimizer::GetResponseSurfaceAt(const TArray<double>& X, double* Y)
{
	double Mu = 0;
	double Std = 0;
	getDistribution(this->Model, X.GetData(), &Mu, &Std);
	*Y = Mu;
}

void BayesOptimizer::RestartModel()
{
	this->InitOptimizer(this->NumDims);
	this->FitModel();
}