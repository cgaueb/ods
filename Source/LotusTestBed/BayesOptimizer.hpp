#pragma once

THIRD_PARTY_INCLUDES_START
#pragma push_macro("check")
#pragma push_macro("TEXT")
#undef check
#undef TEXT

#define USE_SOBOL
#define BAYESOPT_DLL

#include "bayesopt\parameters.h"
#include "bayesopt\bayesopt.h"

#pragma pop_macro("check")
#pragma pop_macro("TEXT")
THIRD_PARTY_INCLUDES_END

#include "Containers/Map.h"
#include "Containers/Array.h"

class BayesOptimizer final
{
public:

    BayesOptimizer();
    ~BayesOptimizer();

    void LoadDLL();

    double GetNextStep(TArray<double>& X);
    void GetTrainStep(TArray<double>& X, int SampleIdx);
    void GetArgMin(TArray<double>& X);
    void GetMinValue(double* Y);
    void GetOptimum(TArray<double>& X, double* Y);
    void GetResponseSurfaceAt(const TArray<double>& X, double* Y);
    void AddSample(const TArray<double>& X, const double Y);
    void InitOptimizer(const int NumDims);
    double FitModel();
    double ReFitModel(TArray<double>& X, double Y);
    void RestartModel();

    void SetTrainIterations(int NumSamples);
    void SetIterations(int NumIters);
    void SetRelearnIterations(int NumIters);
    void SetForceJumpStepIters(int Iters);
    void SetObservationNoise(float Noise);
    void SetStackThreshold(float Noise);
    void SetEpsilonThreshold(float Epsilon);
    void SetExploreExpoit(float Epsilon);
    void SetPrior(const FVector2f& ModelPrior);
    void SetKernelPrior(const FVector2f& KernelPrior);
    void SetStudentParams(const FVector2f& Params);
    void SetLearningMethod(const FName& Method);
    void SetKernelMethod(const FName& Method);
    void SetCriteriaMethod(const FName& Method);
    void SetSurrogateMethod(const FName& Method);
    void SetLearnAll(bool Value);

    static int GetDefualtTrainingIters() { return 10; }
    static int GetDefualtExploreIters() { return 15; }
    static int GetDefualtRelearnIters() { return 20; }
    static int GetDefaultForceJumpStepIters() { return 0; }
    static bool GetDefaultLearnAll() { return false; }
    static float GetDefaultObservationNoise() { return 0; }
    static float GetDefaultStackThreshold() { return 0; }
    static float GetDefaultEpsilonGreedyStep() { return 0; }
    static float GetDefaultExploreExploit() { return -1; }
    static FVector2f GetDefaultPrior() { return FVector2f{ 0, 1 }; }
    static FVector2f GetDefaultKernelLogPrior() { return FVector2f{ 1, 1 }; }
    static FVector2f GetDefaultNIGParams() { return FVector2f{ 1, 1 }; }

    static TArray<FName> KernelMethods() {
        return {
            TEXT("Squared Exp. Iso."),
            TEXT("Squared Exp. non-Iso."),
            TEXT("MATERN5 Iso."), 
            TEXT("MATERN5 non-Iso."), };
    };

    static TArray<FName> LearningMethods() { return { TEXT("Empirical"), TEXT("MCMC") }; };
    static TArray<FName> CriteriaMethods()
    { 
        return {
            TEXT("Probability of Improvement"),
            TEXT("Expected Improvement"), TEXT("a-Expected Improvement"),
            TEXT("Lower Confindence"), TEXT("a-Lower Confindence"),
            TEXT("ThompsonSampling"),
            TEXT("Mutual Information"),
            TEXT("Hedge4"),
            TEXT("Hedge8"),
        };
    };

    static TArray<FName> SurrogateMethods() {
        return {
            TEXT("GP_ML"),
            TEXT("Student-t NIG"), };
    };

protected:
private:

    typedef TPair<TArray<double>, double> TDataPoint;
    typedef TArray<TDataPoint> TDataset;
    typedef TArray<double> TSample;
    typedef TArray<TSample> TSamples;

    void SetupInternalParameters(bopt_params& Params);

    FString PathToDLL;
    void* LibraryHandle;
    void* Model;
    int NumDims;
    TDataset Dataset;
    TSamples UniformSamples;

    bopt_params ModelParams;
};