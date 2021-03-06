#include "target_classifier.h"

#include <catboost/libs/helpers/vector_helpers.h>
#include <library/grid_creator/binarization.h>

#include <util/generic/algorithm.h>

static TVector<float> GetMultiClassBorders(int cnt) {
    TVector<float> borders(cnt);
    for (int i = 0; i < cnt; ++i) {
        borders[i] = 0.5 + i;
    }
    return borders;
}

static TVector<float> SelectBorders(const TVector<float>& target,
                                    int targetBorderCount,
                                    EBorderSelectionType targetBorderType,
                                    bool allowConstLabel) {
    TVector<float> learnTarget(target);

    THashSet<float> borderSet = BestSplit(learnTarget, targetBorderCount, targetBorderType);
    TVector<float> borders(borderSet.begin(), borderSet.end());
    CB_ENSURE((borders.ysize() > 0) || allowConstLabel, "0 target borders");
    if (borders.empty()) {
        borders.push_back(target.front());
    }

    Sort(borders.begin(), borders.end());

    return borders;
}

TTargetClassifier BuildTargetClassifier(const TVector<float>& target,
                                        ELossFunction loss,
                                        const TMaybe<TCustomObjectiveDescriptor>& objectiveDescriptor,
                                        int targetBorderCount,
                                        EBorderSelectionType targetBorderType,
                                        bool allowConstLabel) {
    if (targetBorderCount == 0) {
        return TTargetClassifier();
    }

    CB_ENSURE(!target.empty(), "train target should not be empty");

    TMinMax<float> targetBounds = CalcMinMax(target);
    CB_ENSURE((targetBounds.Min != targetBounds.Max) || allowConstLabel, "target in train should not be constant");

    switch (loss) {
        case ELossFunction::RMSE:
        case ELossFunction::Quantile:
        case ELossFunction::LogLinQuantile:
        case ELossFunction::Poisson:
        case ELossFunction::MAE:
        case ELossFunction::MAPE:
        case ELossFunction::PairLogit:
        case ELossFunction::PairLogitPairwise:
        case ELossFunction::QueryRMSE:
        case ELossFunction::QuerySoftMax:
        case ELossFunction::YetiRank:
        case ELossFunction::YetiRankPairwise:
        case ELossFunction::Logloss:
        case ELossFunction::CrossEntropy:
        case ELossFunction::UserPerObjMetric:
        case ELossFunction::UserQuerywiseMetric:
            return TTargetClassifier(SelectBorders(
                target,
                targetBorderCount,
                targetBorderType,
                allowConstLabel));

        case ELossFunction::MultiClass:
        case ELossFunction::MultiClassOneVsAll:
            return TTargetClassifier(GetMultiClassBorders(targetBorderCount));

        case ELossFunction::Custom: {
            Y_ASSERT(objectiveDescriptor.Defined());
            return TTargetClassifier(SelectBorders(
                target,
                targetBorderCount,
                targetBorderType,
                allowConstLabel));
        }

        default:
            CB_ENSURE(false, "provided error function is not supported");
    }
}
