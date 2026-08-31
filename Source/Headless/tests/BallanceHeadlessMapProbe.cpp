#include "BallanceHeadlessRuntime.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_map>

#include "CKAll.h"
#include "PhysicsRTApi.h"

namespace {

struct LoadObserverState {
    uint64_t batchCount = 0;
    uint64_t nestedBatchCount = 0;
    uint64_t recordCount = 0;
    bool valid = true;
    std::unordered_map<CK_ID, std::pair<int, std::string>> sourceRecords;
};

void ObserveFileLoad(CKContext *context,
                     CK_ID initiatorBehavior,
                     CKSTRING fileName,
                     CKERROR result,
                     const CKFileLoadObjectRecord *records,
                     int recordCount,
                     void *argument) {
    LoadObserverState *state = static_cast<LoadObserverState *>(argument);
    if (!state) {
        return;
    }
    ++state->batchCount;
    if (initiatorBehavior != 0) {
        ++state->nestedBatchCount;
        CKObject *initiator = context ?
                context->GetObject(initiatorBehavior) : nullptr;
        if (!initiator || !CKIsChildClassOf(initiator, CKCID_BEHAVIOR)) {
            state->valid = false;
        }
        const auto source = state->sourceRecords.find(initiatorBehavior);
        if (source == state->sourceRecords.end()) {
            state->valid = false;
        } else {
            std::printf("[load-observer] initiator source ordinal=%d name=%s\n",
                        source->second.first, source->second.second.c_str());
        }
    }
    std::printf("[load-observer] batch=%llu initiator=%u source=%s "
                "result=%d records=%d\n",
                static_cast<unsigned long long>(state->batchCount),
                static_cast<unsigned int>(initiatorBehavior),
                fileName ? fileName : "<memory>", result, recordCount);
    if (recordCount < 0 || (recordCount > 0 && !records)) {
        state->valid = false;
        return;
    }
    for (int i = 0; i < recordCount; ++i) {
        const CKFileLoadObjectRecord &record = records[i];
        if (record.FileObjectIndex != i ||
                (record.CreatedObject != 0 &&
                 (!context || !context->GetObject(record.CreatedObject)))) {
            state->valid = false;
        }
        if (record.CreatedObject != 0) {
            state->sourceRecords[record.CreatedObject] = {
                    record.FileObjectIndex,
                    record.Name ? record.Name : ""};
        }
    }
    state->recordCount += static_cast<uint64_t>(recordCount);
}

void AddSearchPath(CKPathManager *pathManager,
                   int category,
                   const std::filesystem::path &path) {
    if (!pathManager || path.empty() || !std::filesystem::exists(path)) {
        return;
    }
    const std::string native = path.string();
    XString ckPath = native.c_str();
    if (pathManager->GetPathIndex(category, ckPath) < 0) {
        pathManager->AddPath(category, ckPath);
    }
}

void AddGameSearchPaths(CKPathManager *pathManager,
                        const std::filesystem::path &gameDirectory,
                        const std::filesystem::path &mapDirectory) {
    const std::filesystem::path entitiesDirectory =
            gameDirectory / "3D Entities";
    for (int category : {BITMAP_PATH_IDX, DATA_PATH_IDX, SOUND_PATH_IDX}) {
        AddSearchPath(pathManager, category, mapDirectory);
        AddSearchPath(pathManager, category, gameDirectory);
        AddSearchPath(pathManager, category, entitiesDirectory);
        AddSearchPath(pathManager, category, entitiesDirectory / "Level");
        AddSearchPath(pathManager, category, entitiesDirectory / "PH");
        AddSearchPath(pathManager, category, gameDirectory / "Textures");
        AddSearchPath(pathManager, category, gameDirectory / "Sounds");
    }
}

std::filesystem::path FindGameDirectory(
        const std::filesystem::path &assetPath) {
    for (std::filesystem::path candidate = assetPath.parent_path();
         !candidate.empty(); candidate = candidate.parent_path()) {
        if (candidate.filename() == "3D Entities") {
            return candidate.parent_path();
        }
        if (std::filesystem::exists(candidate / "3D Entities")) {
            return candidate;
        }
        if (candidate == candidate.parent_path()) {
            break;
        }
    }
    return assetPath.parent_path();
}

void PrintDependencies(CKFile *file) {
    XClassArray<CKFilePluginDependencies> *dependencies =
            file ? file->GetMissingPlugins() : nullptr;
    if (!dependencies) {
        return;
    }
    for (int categoryIndex = 0; categoryIndex < dependencies->Size();
         ++categoryIndex) {
        CKFilePluginDependencies &category = (*dependencies)[categoryIndex];
        for (int guidIndex = 0; guidIndex < category.m_Guids.Size();
             ++guidIndex) {
            if (category.ValidGuids.IsSet(guidIndex)) {
                continue;
            }
            const CKGUID &guid = category.m_Guids[guidIndex];
            std::fprintf(stderr,
                    "missing plugin: category=%d guid=%08x-%08x\n",
                    category.m_PluginCategory,
                    static_cast<unsigned int>(guid.d1),
                    static_cast<unsigned int>(guid.d2));
        }
    }
}

CKERROR LoadStrict(CKContext *context,
                   const std::filesystem::path &assetPath,
                   LoadObserverState *observer,
                   CKObjectArray **outObjects = nullptr,
                   CK_LOAD_FLAGS loadFlags = CK_LOAD_DEFAULT) {
    if (outObjects) {
        *outObjects = nullptr;
    }
    CKFile *file = context ? context->CreateCKFile() : nullptr;
    CKObjectArray *objects = CreateCKObjectArray();
    if (!file || !objects) {
        if (objects) {
            DeleteCKObjectArray(objects);
        }
        if (file) {
            context->DeleteCKFile(file);
        }
        return CKERR_OUTOFMEMORY;
    }

    const uint64_t batchesBefore = observer ? observer->batchCount : 0;
    const std::string nativePath = assetPath.string();
    std::printf("[map-probe] strict OpenFile %s\n", nativePath.c_str());
    std::fflush(stdout);
    CKERROR result = file->OpenFile(
            const_cast<CKSTRING>(nativePath.c_str()),
            static_cast<CK_LOAD_FLAGS>(
                    loadFlags | CK_LOAD_CHECKDEPENDENCIES));
    if (result != CK_OK) {
        std::fprintf(stderr, "strict OpenFile failed: %d\n", result);
        PrintDependencies(file);
    } else {
        result = file->LoadFileData(objects);
        if (result != CK_OK) {
            std::fprintf(stderr, "LoadFileData failed: %d\n", result);
        }
    }

    if (result == CK_OK) {
        for (int includedIndex = 0;
             includedIndex < file->GetIncludedFileCount();
             ++includedIndex) {
            const char *name = file->GetIncludedFileName(includedIndex);
            const int size = file->GetIncludedFileSize(includedIndex);
            const CKBYTE *data = file->GetIncludedFileData(includedIndex);
            if (!name || size < 0 || (size > 0 && !data)) {
                std::fprintf(stderr,
                        "invalid included-file metadata at index %d\n",
                        includedIndex);
                result = CKERR_INVALIDFILE;
                break;
            }
            std::printf("included[%d]: %s (%d bytes)\n",
                        includedIndex, name, size);
        }
    }

    if (result == CK_OK && observer &&
            (observer->batchCount != batchesBefore + 1 || !observer->valid)) {
        std::fprintf(stderr, "file-load observer contract failed\n");
        result = CKERR_INVALIDFILE;
    }
    if (result == CK_OK) {
        std::printf("loaded %s (%d objects)\n", nativePath.c_str(),
                    objects->GetCount());
    }

    if (result == CK_OK && outObjects) {
        *outObjects = objects;
    } else {
        DeleteCKObjectArray(objects);
    }
    context->DeleteCKFile(file);
    return result;
}

CKERROR LaunchBootstrapScene(CKContext *context) {
    CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
    if (!level || !level->GetLevelScene()) {
        return CKERR_INVALIDFILE;
    }
    std::puts("[map-probe] launch bootstrap default scene");
    return level->LaunchScene(nullptr);
}

CKERROR IntegrateLikeObjectLoader(CKContext *context,
                                  CKObjectArray *objects) {
    CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
    CKScene *scene = context ? context->GetCurrentScene() : nullptr;
    if (!level || !scene || !objects) {
        return CKERR_INVALIDPARAMETER;
    }

    CKLevel *loadedLevel = nullptr;
    for (objects->Reset(); !objects->EndOfList(); objects->Next()) {
        CKObject *object = objects->GetData(context);
        if (object && CKIsChildClassOf(object, CKCID_LEVEL)) {
            loadedLevel = static_cast<CKLevel *>(object);
            break;
        }
    }
    if (loadedLevel) {
        std::puts("[map-probe] merge loaded level");
        return level->Merge(loadedLevel, FALSE);
    }

    std::puts("[map-probe] attach map objects to current level/scene");
    level->BeginAddSequence(TRUE);
    CKERROR result = CK_OK;
    for (objects->Reset(); !objects->EndOfList(); objects->Next()) {
        CKObject *object = objects->GetData(context);
        if (!object) {
            continue;
        }
        if (CKIsChildClassOf(object, CKCID_SCENE)) {
            result = level->AddScene(static_cast<CKScene *>(object));
        } else {
            result = level->AddObject(object);
        }
        if (result != CK_OK) {
            break;
        }
        if (CKIsChildClassOf(object, CKCID_SCENEOBJECT) &&
                !CKIsChildClassOf(object, CKCID_LEVEL) &&
                !CKIsChildClassOf(object, CKCID_SCENE)) {
            scene->AddObjectToScene(
                    static_cast<CKSceneObject *>(object));
        }
    }
    level->BeginAddSequence(FALSE);
    return result;
}

int32_t StepProbe(BallanceHeadlessRuntime *runtime, uint32_t ticks) {
    uint32_t remaining = ticks;
    while (remaining > 0) {
        const uint32_t batch = remaining > PHYSICSRT_MAX_FIXED_STEPS_PER_CALL ?
                PHYSICSRT_MAX_FIXED_STEPS_PER_CALL : remaining;
        const int32_t result = BallanceHeadlessRuntime_Step(runtime, batch);
        if (result != BALLANCE_HEADLESS_OK) {
            return result;
        }
        remaining -= batch;
    }
    return BALLANCE_HEADLESS_OK;
}

bool GetPhysicsBodyCount(BallanceHeadlessRuntime *runtime,
                         uint32_t *outBodyCount) {
    if (!runtime || !outBodyCount) {
        return false;
    }
    const PhysicsRT_ApiV1 *physics =
            PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_1);
    const PhysicsRT_WorldHandle world =
            BallanceHeadlessRuntime_GetPhysicsWorld(runtime);
    if (!physics || !physics->enumerate_bodies ||
            world == PHYSICSRT_INVALID_WORLD) {
        return false;
    }
    uint32_t count = 0;
    const PhysicsRT_Result result =
            physics->enumerate_bodies(world, nullptr, 0, &count);
    if (result != PHYSICSRT_OK &&
            result != PHYSICSRT_ERROR_BUFFER_TOO_SMALL) {
        return false;
    }
    *outBodyCount = count;
    return true;
}

CKBehavior *FindObjectLoadBehavior(CKContext *context) {
    if (!context) {
        return nullptr;
    }
    const CKGUID kObjectLoadGuid(0x7bd977d7, 0x26396c0c);
    const XObjectPointerArray &behaviors =
            context->GetObjectListByType(CKCID_BEHAVIOR, TRUE);
    for (XObjectPointerArray::Iterator it = behaviors.Begin();
         it != behaviors.End(); ++it) {
        CKBehavior *behavior = static_cast<CKBehavior *>(*it);
        if (!behavior || behavior->GetPrototypeGuid() != kObjectLoadGuid) {
            continue;
        }
        return behavior;
    }
    return nullptr;
}

bool LoadThroughObjectBehavior(CKContext *context,
                               const std::filesystem::path &assetPath) {
    CKBehavior *behavior = FindObjectLoadBehavior(context);
    CKParameterIn *fileParameter = behavior ?
            behavior->GetInputParameter(0) : nullptr;
    CKParameter *source = fileParameter ? fileParameter->GetRealSource() : nullptr;
    if (!behavior || !source || behavior->GetInputCount() < 1 ||
            behavior->GetOutputCount() < 3) {
        return false;
    }
    const std::string nativePath = assetPath.string();
    if (source->SetStringValue(
                const_cast<CKSTRING>(nativePath.c_str())) != CK_OK) {
        return false;
    }
    behavior->ActivateOutput(0, FALSE);
    behavior->ActivateOutput(1, FALSE);
    behavior->ActivateOutput(2, FALSE);
    behavior->ActivateInput(0, TRUE);
    behavior->Activate(TRUE);
    std::printf("[map-probe] execute Object Load id=%u file=%s\n",
                static_cast<unsigned int>(behavior->GetID()),
                nativePath.c_str());
    behavior->Execute(1000.0f / 66.0f);
    return behavior->IsOutputActive(0) &&
           !behavior->IsOutputActive(2);
}

int ExecutePhysicalizeBehaviors(CKContext *context) {
    if (!context) {
        return 0;
    }
    const CKGUID physicalizeGuid(0x7522370e, 0x37ec15ec);
    const XObjectPointerArray &behaviors =
            context->GetObjectListByType(CKCID_BEHAVIOR, TRUE);
    int executed = 0;
    for (XObjectPointerArray::Iterator it = behaviors.Begin();
         it != behaviors.End(); ++it) {
        CKBehavior *behavior = static_cast<CKBehavior *>(*it);
        if (!behavior || behavior->GetPrototypeGuid() != physicalizeGuid ||
                !behavior->GetTarget() || behavior->GetInputCount() < 1) {
            continue;
        }
        behavior->ActivateOutput(0, FALSE);
        behavior->ActivateInput(0, TRUE);
        behavior->Execute(1000.0f / 66.0f);
        if (behavior->IsOutputActive(0)) {
            ++executed;
        }
    }
    std::printf("[map-probe] executed Physicalize behaviors=%d\n", executed);
    return executed;
}

bool ParseTickCount(const char *text, uint32_t *ticks) {
    if (!text || !*text || !ticks) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value == 0 ||
            value > 660 ||
            value > (std::numeric_limits<uint32_t>::max)()) {
        return false;
    }
    *ticks = static_cast<uint32_t>(value);
    return true;
}

void PrintUsage() {
    std::fprintf(stderr,
            "usage: BallanceHeadlessMapProbe [--bootstrap <base.cmo>] "
            "[--ticks <1..660>] <map.nmo|map.cmo>\n");
}

} // namespace

int main(int argc, char **argv) {
    std::filesystem::path bootstrapPath;
    std::filesystem::path mapPath;
    uint32_t ticks = 120;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i] ? argv[i] : "";
        if (argument == "--bootstrap" && i + 1 < argc) {
            bootstrapPath = argv[++i];
        } else if (argument == "--ticks" && i + 1 < argc) {
            if (!ParseTickCount(argv[++i], &ticks)) {
                PrintUsage();
                return 2;
            }
        } else if (!argument.empty() && argument[0] != '-' &&
                   mapPath.empty()) {
            mapPath = argument;
        } else {
            PrintUsage();
            return 2;
        }
    }
    if (mapPath.empty()) {
        PrintUsage();
        return 2;
    }
    mapPath = std::filesystem::absolute(mapPath);
    if (!bootstrapPath.empty()) {
        bootstrapPath = std::filesystem::absolute(bootstrapPath);
    }

    std::puts("[map-probe] create runtime");
    std::fflush(stdout);
    BallanceHeadlessRuntime *runtime = nullptr;
    const int32_t createResult = BallanceHeadlessRuntime_Create(&runtime);
    if (createResult != BALLANCE_HEADLESS_OK || !runtime) {
        std::fprintf(stderr, "runtime creation failed: %d\n", createResult);
        return 1;
    }

    CKContext *context = static_cast<CKContext *>(
            BallanceHeadlessRuntime_GetCKContext(runtime));
    CKPathManager *pathManager = context ? context->GetPathManager() : nullptr;
    if (!context || !pathManager ||
            context->GetLoadPolicy() != CK_LOAD_POLICY_STRICT) {
        std::fprintf(stderr, "strict CK context is unavailable\n");
        BallanceHeadlessRuntime_Destroy(&runtime);
        return 1;
    }

    const std::filesystem::path gameDirectory = FindGameDirectory(mapPath);
    AddGameSearchPaths(pathManager, gameDirectory, mapPath.parent_path());
    if (!bootstrapPath.empty()) {
        AddGameSearchPaths(pathManager, FindGameDirectory(bootstrapPath),
                           bootstrapPath.parent_path());
    }

    LoadObserverState observer;
    context->SetFileLoadObserver(ObserveFileLoad, &observer);
    CKERROR result = CK_OK;
    CKObjectArray *mapObjects = nullptr;
    if (!bootstrapPath.empty()) {
        std::puts("[map-probe] load bootstrap");
        result = LoadStrict(context, bootstrapPath, &observer);
        if (result == CK_OK) {
            result = LaunchBootstrapScene(context);
        }
    }
    if (result == CK_OK) {
        std::puts("[map-probe] load map");
        if (!bootstrapPath.empty()) {
            context->SetAutomaticLoadMode(
                    CKLOAD_OK, CKLOAD_OK, CKLOAD_USECURRENT,
                    CKLOAD_USECURRENT);
        }
        const CK_LOAD_FLAGS mapFlags = bootstrapPath.empty() ?
                CK_LOAD_DEFAULT : static_cast<CK_LOAD_FLAGS>(
                        CK_LOAD_DEFAULT | CK_LOAD_AUTOMATICMODE |
                        CK_LOAD_AS_DYNAMIC_OBJECT);
        result = LoadStrict(context, mapPath, &observer,
                            bootstrapPath.empty() ? nullptr : &mapObjects,
                            mapFlags);
        if (result == CK_OK && mapObjects) {
            result = IntegrateLikeObjectLoader(context, mapObjects);
        }
        if (mapObjects) {
            DeleteCKObjectArray(mapObjects);
            mapObjects = nullptr;
        }
    }
    if (result == CK_OK && !bootstrapPath.empty()) {
        if (!context->IsPlaying() && context->Play() != CK_OK) {
            std::fprintf(stderr, "could not start bootstrap behavior world\n");
            result = CKERR_INVALIDFILE;
        }
        const std::filesystem::path physicsTemplate =
                gameDirectory / "3D Entities" / "PH" / "P_Modul_01.nmo";
        if (result == CK_OK &&
                !LoadThroughObjectBehavior(context, physicsTemplate)) {
            std::fprintf(stderr,
                    "real Object Load behavior failed for %s\n",
                    physicsTemplate.string().c_str());
            result = CKERR_INVALIDFILE;
        }
        if (result == CK_OK && ExecutePhysicalizeBehaviors(context) == 0) {
            std::fprintf(stderr,
                    "loaded PH template exposed no executable Physicalize BB\n");
            result = CKERR_INVALIDFILE;
        }
    }
    if (result == CK_OK) {
        std::printf("[map-probe] step %u fixed ticks\n", ticks);
        const int32_t stepResult = StepProbe(runtime, ticks);
        if (stepResult != BALLANCE_HEADLESS_OK) {
            std::fprintf(stderr, "fixed stepping failed: %d\n", stepResult);
            result = CKERR_INVALIDFILE;
        }
    }
    uint32_t bodyCount = 0;
    if (result == CK_OK && !GetPhysicsBodyCount(runtime, &bodyCount)) {
        std::fprintf(stderr, "physics body enumeration failed\n");
        result = CKERR_INVALIDFILE;
    }
    std::printf("[map-probe] physics bodies=%u nested-loads=%llu\n",
                bodyCount,
                static_cast<unsigned long long>(observer.nestedBatchCount));
    if (result == CK_OK && !bootstrapPath.empty() &&
            (observer.nestedBatchCount == 0 || bodyCount == 0)) {
        std::fprintf(stderr,
                "bootstrap world did not produce nested loads/physics bodies\n");
        result = CKERR_INVALIDFILE;
    }
    context->SetFileLoadObserver(nullptr, nullptr);

    std::printf("[map-probe] observed %llu loads / %llu object records\n",
                static_cast<unsigned long long>(observer.batchCount),
                static_cast<unsigned long long>(observer.recordCount));
    std::puts("[map-probe] destroy runtime");
    std::fflush(stdout);
    const int32_t destroyResult = BallanceHeadlessRuntime_Destroy(&runtime);
    return result == CK_OK && observer.valid &&
                   destroyResult == BALLANCE_HEADLESS_OK ? 0 : 1;
}
