#include "label_converter.h"

#include <catboost/libs/logging/logging.h>
#include <catboost/libs/options/json_helper.h>
#include <catboost/libs/options/multiclass_label_options.h>
#include <catboost/libs/options/option.h>

#include <util/generic/algorithm.h>


void TLabelConverter::Initialize(int approxDimension) {
    CB_ENSURE(!Initialized, "Can't initialize initialized object of TLabelConverter");

    ClassesCount = approxDimension;

    ClassToLabel.resize(approxDimension);

    for (int id = 0; id < approxDimension; ++id) {
        ClassToLabel[id] = id;
    }

    LabelToClass = CalcLabelToClassMap(ClassToLabel, 0);

    Initialized = true;
}


void TLabelConverter::Initialize(const TString& multiclassLabelParams) {
    CB_ENSURE(!Initialized, "Can't initialize initialized object of TLabelConverter");
    NCatboostOptions::TMulticlassLabelOptions multiclassOptions;
    multiclassOptions.Load(ReadTJsonValue(multiclassLabelParams));

    int classesCount = multiclassOptions.ClassesCount.Get();
    const auto& classNames = multiclassOptions.ClassNames.Get();

    ClassesCount = GetClassesCount(classesCount, classNames);

    ClassToLabel = multiclassOptions.ClassToLabel.Get();
    LabelToClass = CalcLabelToClassMap(ClassToLabel, ClassesCount);

    ClassesCount = Max(ClassesCount, ClassToLabel.ysize());

    Initialized = true;
}

void TLabelConverter::Initialize(const TVector<float>& targets, int classesCount) {
    CB_ENSURE(!Initialized, "Can't initialize initialized object of TLabelConverter");

    LabelToClass = CalcLabelToClassMap(targets, classesCount);
    ClassesCount = Max(classesCount, LabelToClass.ysize());

    ClassToLabel.resize(LabelToClass.ysize());
    for (const auto& keyValue: LabelToClass) {
        ClassToLabel[keyValue.second] = keyValue.first;
    }
    Initialized = true;
}

int TLabelConverter::GetApproxDimension() const {
    CB_ENSURE(Initialized, "Can't use uninitialized object of TLabelConverter");
    return LabelToClass.ysize();
}

int TLabelConverter::CalcClassIdx(float label, THashSet<float>* MissingLabels) const {
    CB_ENSURE(Initialized, "Can't use uninitialized object of TLabelConverter");
    const auto it = LabelToClass.find(label);

    if (it == LabelToClass.end()) {
        if (ClassesCount > 0 && int(label) == label && label < ClassesCount) {
            if(MissingLabels->find(label) == MissingLabels->end()) {
                MissingLabels->emplace(label);
                MATRIXNET_WARNING_LOG << "Label " << label << " isn't contained in train set but still valid "
                                  << "and will be processed as 0 class of internal approx."
                                  << Endl;
            }
            return 0;
        }
        CB_ENSURE(it != LabelToClass.end(), "Label " << label << " is bad label and not contained in train set.");
    }

    return it->second;
}

bool TLabelConverter::IsInitialized() const {
    return Initialized;
}

TString TLabelConverter::SerializeMulticlassParams(int classesCount, const TVector<TString>& classNames) {
    CB_ENSURE(Initialized, "Can't use uninitialized object of TLabelConverter");
    NCatboostOptions::TMulticlassLabelOptions multiclassLabelOptions;
    multiclassLabelOptions.ClassToLabel = ClassToLabel;
    multiclassLabelOptions.ClassesCount = classesCount;
    multiclassLabelOptions.ClassNames = classNames;
    NJson::TJsonValue json;

    multiclassLabelOptions.Save(&json);
    return ToString(json);
}

void PrepareTargetCompressed(const TLabelConverter& labelConverter, TVector<float>* labels) {
    CB_ENSURE(labelConverter.IsInitialized(), "Label converter isn't built.");
    THashSet<float> missingLabels;
    for (auto& label : *labels) {
        label = labelConverter.CalcClassIdx(label, &missingLabels);
    }
}

THashMap<float, int> CalcLabelToClassMap(TVector<float> targets, int classesCount) {
    SortUnique(targets);
    THashMap<float, int> labels;
    if (classesCount != 0) {  // classes-count or class-names are set
        CB_ENSURE(AllOf(targets, [&classesCount](float x) { return int(x) == x && x >= 0 && x < classesCount; }),
                  "If classes count is specified each target label should be nonnegative integer in [0,..,classes_count - 1].");

        if (classesCount > targets.ysize()) {
            MATRIXNET_WARNING_LOG << "Found only " << targets.ysize() <<
                                  " unique classes but defined " << classesCount
                                  << " classes probably something is wrong with data." << Endl;
        }
    }

    labels.reserve(targets.size());
    int id = 0;
    for (const auto& target : targets) {
        labels.emplace(target, id++);
    }

    return labels;
}

int GetClassesCount(int classesCount, const TVector<TString>& classNames) {
    if (classNames.empty() || classesCount == 0) {
        return Max(classNames.ysize(), classesCount);
    }

    CB_ENSURE(classesCount == classNames.ysize(),
              "classes-count must be equal to size of class-names if both are specified.");
    return classesCount;
}