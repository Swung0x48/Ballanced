#include "BallanceHeadlessRuntime.h"

#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include "CKAll.h"
#include "PhysicsRTApi.h"

namespace {

struct ObserverCapture {
    std::unordered_map<CK_ID, std::pair<int, std::string>> sources;
    CK_ID initiator = 0;
    int sourceOrdinal = -1;
    std::string sourceName;
    bool resolved = false;
};

void CaptureLoad(CKContext *,
                 CK_ID initiatorBehavior,
                 CKSTRING,
                 CKERROR,
                 const CKFileLoadObjectRecord *records,
                 int recordCount,
                 void *argument) {
    ObserverCapture *capture = static_cast<ObserverCapture *>(argument);
    if (!capture) {
        return;
    }
    if (initiatorBehavior != 0) {
        capture->initiator = initiatorBehavior;
        const auto source = capture->sources.find(initiatorBehavior);
        if (source != capture->sources.end()) {
            capture->sourceOrdinal = source->second.first;
            capture->sourceName = source->second.second;
            capture->resolved = true;
        }
    }
    for (int i = 0; records && i < recordCount; ++i) {
        if (records[i].CreatedObject != 0) {
            capture->sources[records[i].CreatedObject] = {
                    records[i].FileObjectIndex,
                    records[i].Name ? records[i].Name : ""};
        }
    }
}

struct NestedLoadState {
    std::string fileName;
    CKERROR result = CKERR_INVALIDFILE;
    CKObjectArray objects;
};

NestedLoadState *g_nestedLoadState = nullptr;

int RunNestedLoad(const CKBehaviorContext &behaviorContext) {
    NestedLoadState *state = g_nestedLoadState;
    if (!state || !behaviorContext.Context) {
        return CKBR_GENERICERROR;
    }
    state->result = behaviorContext.Context->Load(
            const_cast<CKSTRING>(state->fileName.c_str()),
            &state->objects,
            CK_LOAD_DEFAULT);
    return state->result == CK_OK ? CKBR_OK : CKBR_GENERICERROR;
}

int Fail(const char *message) {
    std::fprintf(stderr, "Ballance headless runtime smoke failed: %s\n", message);
    return 1;
}

bool TestStrictIncludedFile(CKContext *context) {
    if (!context) {
        return false;
    }

    const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
    const std::filesystem::path root = std::filesystem::temp_directory_path();
    const std::filesystem::path containerPath = root /
            ("ballance-headless-included-" + std::to_string(stamp) + ".nmo");
    const std::string embeddedName =
            "ballance-headless-payload-" + std::to_string(stamp) + ".bin";
    const XString ckTemporaryDirectory =
            context->GetPathManager()->GetVirtoolsTemporaryFolder();
    const std::filesystem::path extractedPath =
            std::filesystem::path(ckTemporaryDirectory.CStr()) / embeddedName;
    std::error_code ignored;
    std::filesystem::remove(extractedPath, ignored);
    constexpr unsigned char kPayload[] = {
            0x00, 0x42, 0xff, 0x17, 0x6e, 0x65, 0x6d, 0x6f};

    struct HeaderPart0 {
        char signature[8];
        CKDWORD crc;
        CKDWORD ckVersion;
        CKDWORD fileVersion;
        CKDWORD fileVersion2;
        CKDWORD fileWriteMode;
        CKDWORD headerPackSize;
    } part0 = {};
    struct HeaderPart1 {
        CKDWORD dataPackSize;
        CKDWORD dataUnpackSize;
        CKDWORD managerCount;
        CKDWORD objectCount;
        CKDWORD maxIdSaved;
        CKDWORD productVersion;
        CKDWORD productBuild;
        CKDWORD headerUnpackSize;
    } part1 = {};
    static_assert(sizeof(HeaderPart0) == 32);
    static_assert(sizeof(HeaderPart1) == 32);

    const int headerPayload[] = {0, static_cast<int>(sizeof(int)), 1};
    std::memcpy(part0.signature, "Nemo Fi", sizeof(part0.signature));
    part0.ckVersion = CKVERSION;
    part0.fileVersion = 8;
    part0.fileWriteMode = CKFILE_UNCOMPRESSED;
    part0.headerPackSize = sizeof(headerPayload);
    part1.headerUnpackSize = sizeof(headerPayload);
    part0.crc = CKComputeDataCRC(
            reinterpret_cast<const char *>(&part0), sizeof(part0), 0);
    part0.crc = CKComputeDataCRC(
            reinterpret_cast<const char *>(&part1), sizeof(part1), part0.crc);
    part0.crc = CKComputeDataCRC(
            reinterpret_cast<const char *>(headerPayload),
            sizeof(headerPayload), part0.crc);

    bool success = false;
    {
        std::ofstream container(containerPath,
                                std::ios::binary | std::ios::trunc);
        const int nameLength = static_cast<int>(embeddedName.size());
        const int payloadSize = sizeof(kPayload);
        if (container &&
                container.write(reinterpret_cast<const char *>(&part0),
                                sizeof(part0)) &&
                container.write(reinterpret_cast<const char *>(&part1),
                                sizeof(part1)) &&
                container.write(
                        reinterpret_cast<const char *>(headerPayload),
                        sizeof(headerPayload)) &&
                container.write(reinterpret_cast<const char *>(&nameLength),
                                sizeof(nameLength)) &&
                container.write(embeddedName.data(), embeddedName.size()) &&
                container.write(reinterpret_cast<const char *>(&payloadSize),
                                sizeof(payloadSize)) &&
                container.write(reinterpret_cast<const char *>(kPayload),
                                sizeof(kPayload))) {
            success = true;
        }
    }

    CKFile *reader = success ? context->CreateCKFile() : nullptr;
    CKObjectArray *objects = success ? CreateCKObjectArray() : nullptr;
    if (reader && objects) {
        const std::string container = containerPath.string();
        success = reader->OpenFile(container.c_str(),
                                   CK_LOAD_CHECKDEPENDENCIES) == CK_OK &&
                  reader->LoadFileData(objects) == CK_OK;
        success = success && reader->GetIncludedFileCount() == 1 &&
                  reader->GetIncludedFileName(0) &&
                  embeddedName == reader->GetIncludedFileName(0) &&
                  reader->GetIncludedFileSize(0) == sizeof(kPayload) &&
                  reader->GetIncludedFileData(0) &&
                  std::memcmp(reader->GetIncludedFileData(0), kPayload,
                              sizeof(kPayload)) == 0 &&
                  !std::filesystem::exists(extractedPath);
    } else {
        success = false;
    }
    if (objects) {
        DeleteCKObjectArray(objects);
    }
    if (reader) {
        context->DeleteCKFile(reader);
    }

    std::filesystem::remove(containerPath, ignored);
    std::filesystem::remove(extractedPath, ignored);
    return success;
}

bool TestBehaviorLoadInitiator(CKContext *context) {
    if (!context) {
        return false;
    }
    const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
    const std::filesystem::path nestedPath =
            std::filesystem::temp_directory_path() /
            ("ballance-observer-nested-" + std::to_string(stamp) + ".nmo");
    const std::string nestedFile = nestedPath.string();
    std::error_code ignored;

    CKBehavior *sourceBehavior = static_cast<CKBehavior *>(
            context->CreateObject(CKCID_BEHAVIOR,
                                  "SyntheticLoadInitiator"));
    CKGroup *nestedObject = static_cast<CKGroup *>(
            context->CreateObject(CKCID_GROUP,
                                  "SyntheticNestedObject"));
    bool success = sourceBehavior && nestedObject;
    if (success) {
        CKObjectArray nestedObjects;
        nestedObjects.InsertRear(nestedObject);
        success = context->Save(
                const_cast<CKSTRING>(nestedFile.c_str()),
                &nestedObjects, CK_STATESAVE_ALL) == CK_OK;
    }
    if (nestedObject) {
        context->DestroyObject(nestedObject);
    }

    ObserverCapture capture;
    if (success) {
        sourceBehavior->UseFunction();
        sourceBehavior->SetFunction(RunNestedLoad);
        context->SetFileLoadObserver(CaptureLoad, &capture);
        NestedLoadState nestedState;
        nestedState.fileName = nestedFile;
        g_nestedLoadState = &nestedState;
        sourceBehavior->Execute(1000.0f / 66.0f);
        g_nestedLoadState = nullptr;
        success = nestedState.result == CK_OK &&
                  capture.initiator == sourceBehavior->GetID() &&
                  !capture.sources.empty();
    }
    g_nestedLoadState = nullptr;
    context->SetFileLoadObserver(nullptr, nullptr);
    if (sourceBehavior) {
        context->DestroyObject(sourceBehavior);
    }
    std::filesystem::remove(nestedPath, ignored);
    return success;
}

} // namespace

int main() {
    std::puts("[smoke] verify linked components");
    std::fflush(stdout);
    if (BallanceHeadlessRuntime_VerifyLinkedComponents() !=
            BALLANCE_HEADLESS_OK) {
        return Fail("linked component verification failed");
    }

    BallanceHeadlessBuildInfo buildInfo = {};
    buildInfo.struct_size = sizeof(buildInfo);
    if (BallanceHeadlessRuntime_GetBuildInfo(&buildInfo) !=
                BALLANCE_HEADLESS_OK ||
            buildInfo.abi_version != BALLANCE_HEADLESS_RUNTIME_ABI_VERSION ||
            buildInfo.fixed_tick_hz != PHYSICSRT_FIXED_TICK_HZ ||
            std::strcmp(buildInfo.solver_compatibility_id,
                        "ivp-2.1-authority-66hz-v1") != 0) {
        return Fail("build/solver manifest is invalid");
    }

    std::puts("[smoke] create CK/IVP runtime");
    std::fflush(stdout);
    BallanceHeadlessRuntime *runtime = nullptr;
    if (BallanceHeadlessRuntime_Create(&runtime) != BALLANCE_HEADLESS_OK ||
            !runtime) {
        return Fail("runtime creation failed");
    }

    std::puts("[smoke] inspect strict context and NULL renderer");
    std::fflush(stdout);
    CKContext *context = static_cast<CKContext *>(
            BallanceHeadlessRuntime_GetCKContext(runtime));
    if (!context || context->GetLoadPolicy() != CK_LOAD_POLICY_STRICT) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("strict CK context is unavailable");
    }

    CK3dEntity *headlessEntity = static_cast<CK3dEntity *>(
            context->CreateObject(CKCID_3DENTITY, "HeadlessSmokeEntity"));
    if (!headlessEntity) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("headless CK3dEntity creation failed");
    }
    context->DestroyObject(headlessEntity);

    std::puts("[smoke] strict embedded-file memory ownership");
    std::fflush(stdout);
    if (!TestStrictIncludedFile(context)) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("strict embedded-file inspection failed");
    }

    std::puts("[smoke] behavior-initiated load identity");
    std::fflush(stdout);
    if (!TestBehaviorLoadInitiator(context)) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("behavior load initiator identity was not reported");
    }

    BallanceHeadlessPluginClassification classification = {};
    classification.struct_size = sizeof(classification);
    if (BallanceHeadlessRuntime_ClassifyPlugin(
                CKPLUGIN_MANAGER_DLL, INPUT_MANAGER_GUID1, 0,
                &classification) != BALLANCE_HEADLESS_OK ||
            (classification.flags &
             (BALLANCE_HEADLESS_PLUGIN_ENGINE |
              BALLANCE_HEADLESS_PLUGIN_NO_BACKEND)) !=
                    (BALLANCE_HEADLESS_PLUGIN_ENGINE |
                     BALLANCE_HEADLESS_PLUGIN_NO_BACKEND) ||
            BallanceHeadlessRuntime_ClassifyPlugin(
                CKPLUGIN_SOUND_READER, 0x61abc44fu, 0xe1233343u,
                &classification) != BALLANCE_HEADLESS_OK ||
            (classification.flags &
             (BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
              BALLANCE_HEADLESS_PLUGIN_NO_BACKEND)) !=
                    (BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                     BALLANCE_HEADLESS_PLUGIN_NO_BACKEND) ||
            BallanceHeadlessRuntime_ClassifyPlugin(
                CKPLUGIN_BEHAVIOR_DLL, 0xdeadbeefu, 0xbaadf00du,
                &classification) != BALLANCE_HEADLESS_ERROR_UNKNOWN_PLUGIN ||
            BallanceHeadlessRuntime_ClassifyPlugin(
                UINT32_MAX, INPUT_MANAGER_GUID1, 0,
                &classification) != BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("static plugin classification failed");
    }

    CKRenderManager *renderManager = context->GetRenderManager();
    if (!renderManager || renderManager->GetRenderDriverCount() != 1) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("NULL rasterizer was not the sole render driver");
    }
    VxDriverDesc *driver = renderManager->GetRenderDriverDescription(0);
    if (!driver || !std::strstr(driver->DriverDesc, "NULL Rasterizer")) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("unexpected render driver");
    }

    const PhysicsRT_ApiV1 *physics = PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_1);
    const PhysicsRT_WorldHandle world =
            BallanceHeadlessRuntime_GetPhysicsWorld(runtime);
    uint32_t authorityMode = 0;
    if (!physics || world == PHYSICSRT_INVALID_WORLD ||
            physics->validate_world(world) != PHYSICSRT_OK ||
            physics->get_authority_mode(world, &authorityMode) != PHYSICSRT_OK ||
            authorityMode != 1) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("authoritative physics world is unavailable");
    }

    std::puts("[smoke] step two fixed ticks");
    std::fflush(stdout);
    if (BallanceHeadlessRuntime_Step(runtime, 2) != BALLANCE_HEADLESS_OK) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("fixed CK/IVP stepping failed");
    }
    CKTimeManager *timeManager = context->GetTimeManager();
    if (!timeManager || !timeManager->IsExternalClockEnabled() ||
            std::fabs(timeManager->GetExternalClockDelta() -
                      (1000.0f / 66.0f)) > 0.0001f) {
        BallanceHeadlessRuntime_Destroy(&runtime);
        return Fail("external fixed clock was not retained");
    }

    if (BallanceHeadlessRuntime_Destroy(&runtime) != BALLANCE_HEADLESS_OK ||
            runtime != nullptr ||
            BallanceHeadlessRuntime_Destroy(&runtime) != BALLANCE_HEADLESS_OK) {
        return Fail("idempotent shutdown failed");
    }

    std::puts("Ballance headless runtime smoke passed");
    return 0;
}
