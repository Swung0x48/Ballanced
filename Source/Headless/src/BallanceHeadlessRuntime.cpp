#include "BallanceHeadlessRuntime.h"

#include <cmath>
#include <cstring>
#include <mutex>
#include <new>
#include <thread>

#include "BallanceHeadlessRuntimeBuild.h"
#include "CKAll.h"
#include "CKIpionManager.h"
#include "PhysicsRTApi.h"

CKPluginInfo *CKGet_CK2_3D_PluginInfo(int index);
CKPluginInfo *CKGet_ParamOp_PluginInfo(int index);
int CKGet_NemoLoader_PluginInfoCount();
CKPluginInfo *CKGet_NemoLoader_PluginInfo(int index);
CKDataReader *CKGet_NemoLoader_Reader(int index);

#define BALLANCE_DECLARE_STATIC_BB(symbol)                                  \
    int CKGet_##symbol##_PluginInfoCount();                                 \
    CKPluginInfo *CKGet_##symbol##_PluginInfo(int index);                   \
    void Register_##symbol##_BehaviorDeclarations(                          \
            XObjectDeclarationArray *declarations)

BALLANCE_DECLARE_STATIC_BB(3DTransfo);
BALLANCE_DECLARE_STATIC_BB(BBAddons);
BALLANCE_DECLARE_STATIC_BB(Cameras);
BALLANCE_DECLARE_STATIC_BB(Characters);
BALLANCE_DECLARE_STATIC_BB(Collisions);
BALLANCE_DECLARE_STATIC_BB(Controllers);
BALLANCE_DECLARE_STATIC_BB(Grids);
BALLANCE_DECLARE_STATIC_BB(Interface);
BALLANCE_DECLARE_STATIC_BB(Lights);
BALLANCE_DECLARE_STATIC_BB(Logics);
BALLANCE_DECLARE_STATIC_BB(Materials);
BALLANCE_DECLARE_STATIC_BB(MeshModifiers);
BALLANCE_DECLARE_STATIC_BB(Narratives);
BALLANCE_DECLARE_STATIC_BB(Sounds);
BALLANCE_DECLARE_STATIC_BB(Visuals);
BALLANCE_DECLARE_STATIC_BB(WorldEnvironment);
BALLANCE_DECLARE_STATIC_BB(TT_Physics);
BALLANCE_DECLARE_STATIC_BB(TT_Database_Manager);
BALLANCE_DECLARE_STATIC_BB(TT_Gravity);
BALLANCE_DECLARE_STATIC_BB(TT_Interface_Manager);
BALLANCE_DECLARE_STATIC_BB(TT_ParticleSystems);
BALLANCE_DECLARE_STATIC_BB(TT_Toolbox);

#undef BALLANCE_DECLARE_STATIC_BB

namespace {

constexpr uint32_t kLinkedComponents =
        BALLANCE_HEADLESS_COMPONENT_CK2 |
        BALLANCE_HEADLESS_COMPONENT_NULL_RENDERER |
        BALLANCE_HEADLESS_COMPONENT_PARAMETER_OPERATIONS |
        BALLANCE_HEADLESS_COMPONENT_VIRTOOLS_LOADER |
        BALLANCE_HEADLESS_COMPONENT_PHYSICS_RT |
        BALLANCE_HEADLESS_COMPONENT_INPUT_STUB;

struct StaticPluginPolicy {
    const char *name;
    uint32_t flags;
};

constexpr uint32_t kAvailable = BALLANCE_HEADLESS_PLUGIN_AVAILABLE;
constexpr StaticPluginPolicy kStaticPluginPolicies[] = {
        {"BallanceHeadless.InputStub",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_ENGINE |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.RenderEngine",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_ENGINE |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.ParameterOperations",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_ENGINE},
        {"BallanceHeadless.VirtoolsLoader",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_ENGINE},
        {"BallanceHeadless.BB.3DTransfo",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY},
        {"BallanceHeadless.BB.BuildingBlocksAddons1",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PRESENTATION},
        {"BallanceHeadless.BB.Cameras",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED},
        {"BallanceHeadless.BB.Characters",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED},
        {"BallanceHeadless.BB.Collisions",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY},
        {"BallanceHeadless.BB.Controllers",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED},
        {"BallanceHeadless.BB.Grids",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY},
        {"BallanceHeadless.BB.Interface",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.Lights",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.Logics",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY},
        {"BallanceHeadless.BB.Materials",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.MeshModifiers",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PRESENTATION},
        {"BallanceHeadless.BB.Narratives",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED},
        {"BallanceHeadless.BB.Sounds",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.Visuals",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.WorldEnvironment",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.TT_DatabaseManager_RT",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED},
        {"BallanceHeadless.BB.TT_Gravity_RT",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY},
        {"BallanceHeadless.BB.TT_InterfaceManager_RT",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.TT_ParticleSystems_RT",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_NO_BACKEND},
        {"BallanceHeadless.BB.TT_Toolbox_RT",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PRESENTATION |
                 BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED},
        {"BallanceHeadless.physics_RT",
         kAvailable | BALLANCE_HEADLESS_PLUGIN_GAMEPLAY |
                 BALLANCE_HEADLESS_PLUGIN_PHYSICS},
};

std::mutex g_startupMutex;
uint32_t g_startupReferences = 0;

CKPluginInfo *GetHeadlessInputPluginInfo(int) {
    static CKPluginInfo pluginInfo;
    pluginInfo.m_Author = "BallanceMMO";
    pluginInfo.m_Description = "Headless Disabled Input Capability";
    pluginInfo.m_Extension = "";
    pluginInfo.m_Type = CKPLUGIN_MANAGER_DLL;
    pluginInfo.m_Version = 1;
    pluginInfo.m_InitInstanceFct = nullptr;
    pluginInfo.m_ExitInstanceFct = nullptr;
    pluginInfo.m_GUID = INPUT_MANAGER_GUID;
    pluginInfo.m_Summary =
            "Dependency marker only; headless contexts disable input";
    return &pluginInfo;
}

bool RegisterStaticPlugin(CKPluginManager *pluginManager,
                          const char *name,
                          CKPluginGetInfoCountFunction getInfoCount,
                          CKPluginGetInfoFunction getInfo,
                          CKReaderGetReaderFunction getReader = nullptr,
                          CKDLL_OBJECTDECLARATIONFUNCTION registerDeclarations = nullptr) {
    const CKERROR result = pluginManager->RegisterStaticPlugin(
            const_cast<CKSTRING>(name), getInfoCount, getInfo, getReader,
            registerDeclarations);
    return result == CK_OK || result == CKERR_ALREADYPRESENT;
}

bool RegisterBehaviorPlugin(
        CKPluginManager *pluginManager,
        const char *name,
        CKPluginGetInfoCountFunction getInfoCount,
        CKPluginGetInfoFunction getInfo,
        CKDLL_OBJECTDECLARATIONFUNCTION registerDeclarations) {
    return RegisterStaticPlugin(pluginManager, name, getInfoCount, getInfo,
                                nullptr, registerDeclarations);
}

int32_t RegisterHeadlessStaticPlugins() {
    CKPluginManager *pluginManager = CKGetPluginManager();
    if (!pluginManager ||
            !RegisterStaticPlugin(pluginManager,
                    "BallanceHeadless.InputStub", nullptr,
                    GetHeadlessInputPluginInfo) ||
            !RegisterStaticPlugin(pluginManager,
                    "BallanceHeadless.RenderEngine", nullptr,
                    CKGet_CK2_3D_PluginInfo) ||
            !RegisterStaticPlugin(pluginManager,
                    "BallanceHeadless.ParameterOperations", nullptr,
                    CKGet_ParamOp_PluginInfo) ||
            !RegisterStaticPlugin(pluginManager,
                    "BallanceHeadless.VirtoolsLoader",
                    CKGet_NemoLoader_PluginInfoCount,
                    CKGet_NemoLoader_PluginInfo,
                    CKGet_NemoLoader_Reader) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.3DTransfo",
                    CKGet_3DTransfo_PluginInfoCount,
                    CKGet_3DTransfo_PluginInfo,
                    Register_3DTransfo_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.BuildingBlocksAddons1",
                    CKGet_BBAddons_PluginInfoCount,
                    CKGet_BBAddons_PluginInfo,
                    Register_BBAddons_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Cameras",
                    CKGet_Cameras_PluginInfoCount,
                    CKGet_Cameras_PluginInfo,
                    Register_Cameras_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Characters",
                    CKGet_Characters_PluginInfoCount,
                    CKGet_Characters_PluginInfo,
                    Register_Characters_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Collisions",
                    CKGet_Collisions_PluginInfoCount,
                    CKGet_Collisions_PluginInfo,
                    Register_Collisions_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Controllers",
                    CKGet_Controllers_PluginInfoCount,
                    CKGet_Controllers_PluginInfo,
                    Register_Controllers_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Grids",
                    CKGet_Grids_PluginInfoCount,
                    CKGet_Grids_PluginInfo,
                    Register_Grids_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Interface",
                    CKGet_Interface_PluginInfoCount,
                    CKGet_Interface_PluginInfo,
                    Register_Interface_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Lights",
                    CKGet_Lights_PluginInfoCount,
                    CKGet_Lights_PluginInfo,
                    Register_Lights_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Logics",
                    CKGet_Logics_PluginInfoCount,
                    CKGet_Logics_PluginInfo,
                    Register_Logics_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Materials",
                    CKGet_Materials_PluginInfoCount,
                    CKGet_Materials_PluginInfo,
                    Register_Materials_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.MeshModifiers",
                    CKGet_MeshModifiers_PluginInfoCount,
                    CKGet_MeshModifiers_PluginInfo,
                    Register_MeshModifiers_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Narratives",
                    CKGet_Narratives_PluginInfoCount,
                    CKGet_Narratives_PluginInfo,
                    Register_Narratives_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Sounds",
                    CKGet_Sounds_PluginInfoCount,
                    CKGet_Sounds_PluginInfo,
                    Register_Sounds_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.Visuals",
                    CKGet_Visuals_PluginInfoCount,
                    CKGet_Visuals_PluginInfo,
                    Register_Visuals_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.WorldEnvironment",
                    CKGet_WorldEnvironment_PluginInfoCount,
                    CKGet_WorldEnvironment_PluginInfo,
                    Register_WorldEnvironment_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.TT_DatabaseManager_RT",
                    CKGet_TT_Database_Manager_PluginInfoCount,
                    CKGet_TT_Database_Manager_PluginInfo,
                    Register_TT_Database_Manager_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.TT_Gravity_RT",
                    CKGet_TT_Gravity_PluginInfoCount,
                    CKGet_TT_Gravity_PluginInfo,
                    Register_TT_Gravity_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.TT_InterfaceManager_RT",
                    CKGet_TT_Interface_Manager_PluginInfoCount,
                    CKGet_TT_Interface_Manager_PluginInfo,
                    Register_TT_Interface_Manager_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.TT_ParticleSystems_RT",
                    CKGet_TT_ParticleSystems_PluginInfoCount,
                    CKGet_TT_ParticleSystems_PluginInfo,
                    Register_TT_ParticleSystems_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.BB.TT_Toolbox_RT",
                    CKGet_TT_Toolbox_PluginInfoCount,
                    CKGet_TT_Toolbox_PluginInfo,
                    Register_TT_Toolbox_BehaviorDeclarations) ||
            !RegisterBehaviorPlugin(pluginManager,
                    "BallanceHeadless.physics_RT",
                    CKGet_TT_Physics_PluginInfoCount,
                    CKGet_TT_Physics_PluginInfo,
                    Register_TT_Physics_BehaviorDeclarations)) {
        return BALLANCE_HEADLESS_ERROR_PLUGIN_INITIALIZATION;
    }
    return BALLANCE_HEADLESS_OK;
}

template <size_t Capacity>
void CopyText(char (&destination)[Capacity], const char *source) {
    std::memset(destination, 0, Capacity);
    if (!source) {
        return;
    }
#if defined(_MSC_VER)
    strncpy_s(destination, Capacity, source, _TRUNCATE);
#else
    std::strncpy(destination, source, Capacity - 1);
#endif
}

int32_t AcquireEngineStartup() {
    std::lock_guard<std::mutex> lock(g_startupMutex);
    if (g_startupReferences == 0) {
        if (CKStartUp() != CK_OK) {
            return BALLANCE_HEADLESS_ERROR_ENGINE_STARTUP;
        }
        const int32_t registration = RegisterHeadlessStaticPlugins();
        if (registration != BALLANCE_HEADLESS_OK) {
            CKShutdown();
            return registration;
        }
    }
    ++g_startupReferences;
    return BALLANCE_HEADLESS_OK;
}

void ReleaseEngineStartup() {
    std::lock_guard<std::mutex> lock(g_startupMutex);
    if (g_startupReferences == 0) {
        return;
    }
    --g_startupReferences;
    if (g_startupReferences == 0) {
        CKShutdown();
    }
}

bool PhysicsBuildInfo(PhysicsRT_BuildInfo *buildInfo,
                      const PhysicsRT_ApiV1 **outApi = nullptr) {
    if (!buildInfo) {
        return false;
    }
    const PhysicsRT_ApiV1 *api = PhysicsRT_GetApi(PHYSICSRT_ABI_VERSION_1);
    if (!api || api->struct_size < sizeof(PhysicsRT_ApiV1) ||
            api->abi_version != PHYSICSRT_ABI_VERSION_1 ||
            !api->get_build_info) {
        return false;
    }
    std::memset(buildInfo, 0, sizeof(*buildInfo));
    buildInfo->struct_size = sizeof(*buildInfo);
    if (api->get_build_info(buildInfo) != PHYSICSRT_OK ||
            buildInfo->abi_version != PHYSICSRT_ABI_VERSION_1) {
        return false;
    }
    if (outApi) {
        *outApi = api;
    }
    return true;
}

int32_t VerifyRegisteredComponents() {
    CKPluginManager *pluginManager = CKGetPluginManager();
    if (!pluginManager ||
            !pluginManager->GetPluginDllInfo("BallanceHeadless.RenderEngine") ||
            !pluginManager->GetPluginDllInfo(
                    "BallanceHeadless.ParameterOperations") ||
            !pluginManager->GetPluginDllInfo("BallanceHeadless.InputStub") ||
            !pluginManager->GetPluginDllInfo("BallanceHeadless.VirtoolsLoader") ||
            !pluginManager->GetPluginDllInfo("BallanceHeadless.physics_RT") ||
            pluginManager->GetPluginCount(CKPLUGIN_RENDERENGINE_DLL) != 1 ||
            pluginManager->GetPluginCount(CKPLUGIN_MODEL_READER) != 4) {
        return BALLANCE_HEADLESS_ERROR_INCOMPATIBLE_COMPONENT;
    }

    PhysicsRT_BuildInfo physicsBuildInfo = {};
    if (!PhysicsBuildInfo(&physicsBuildInfo) ||
            std::strcmp(physicsBuildInfo.solver_compatibility_id,
                        BALLANCE_HEADLESS_BUILD_SOLVER_ID) != 0 ||
            std::fabs(physicsBuildInfo.fixed_step_seconds -
                      (1.0f / static_cast<float>(PHYSICSRT_FIXED_TICK_HZ))) >
                    1.0e-8f) {
        return BALLANCE_HEADLESS_ERROR_INCOMPATIBLE_COMPONENT;
    }
    return BALLANCE_HEADLESS_OK;
}

int32_t ClassifyRegisteredPlugin(
        uint32_t pluginCategory,
        const CKGUID &guid,
        BallanceHeadlessPluginClassification *outClassification) {
    if (pluginCategory > static_cast<uint32_t>(CKPLUGIN_EXTENSION_DLL)) {
        return BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT;
    }
    CKPluginManager *pluginManager = CKGetPluginManager();
    CKPluginEntry *entry = pluginManager ? pluginManager->FindComponent(
            guid, static_cast<int>(pluginCategory)) : nullptr;
    if (!entry) {
        return BALLANCE_HEADLESS_ERROR_UNKNOWN_PLUGIN;
    }
    CKPluginDll *pluginDll = pluginManager->GetPluginDllInfo(
            entry->m_PluginDllIndex);
    const char *moduleName = pluginDll ? pluginDll->m_DllFileName.CStr() : nullptr;
    if (!moduleName) {
        return BALLANCE_HEADLESS_ERROR_UNKNOWN_PLUGIN;
    }
    for (const StaticPluginPolicy &policy : kStaticPluginPolicies) {
        if (std::strcmp(moduleName, policy.name) != 0) {
            continue;
        }
        BallanceHeadlessPluginClassification classification = {};
        classification.struct_size = sizeof(classification);
        classification.flags = policy.flags;
        classification.plugin_category = pluginCategory;
        CopyText(classification.module_name, moduleName);
        *outClassification = classification;
        return BALLANCE_HEADLESS_OK;
    }
    return BALLANCE_HEADLESS_ERROR_UNKNOWN_PLUGIN;
}

} // namespace

struct BallanceHeadlessRuntime {
    std::thread::id ownerThread;
    CKContext *context = nullptr;
    const PhysicsRT_ApiV1 *physicsApi = nullptr;
    PhysicsRT_WorldHandle physicsWorld = PHYSICSRT_INVALID_WORLD;
    bool startupAcquired = false;
};

extern "C" int32_t BallanceHeadlessRuntime_VerifyLinkedComponents(void) {
    const int32_t startup = AcquireEngineStartup();
    if (startup != BALLANCE_HEADLESS_OK) {
        return startup;
    }
    const int32_t verification = VerifyRegisteredComponents();
    ReleaseEngineStartup();
    return verification;
}

extern "C" int32_t BallanceHeadlessRuntime_GetBuildInfo(
        BallanceHeadlessBuildInfo *outInfo) {
    if (!outInfo || outInfo->struct_size < sizeof(BallanceHeadlessBuildInfo)) {
        return BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT;
    }

    PhysicsRT_BuildInfo physicsBuildInfo = {};
    if (!PhysicsBuildInfo(&physicsBuildInfo)) {
        return BALLANCE_HEADLESS_ERROR_INCOMPATIBLE_COMPONENT;
    }

    BallanceHeadlessBuildInfo info = {};
    info.struct_size = sizeof(info);
    info.abi_version = BALLANCE_HEADLESS_RUNTIME_ABI_VERSION;
    info.linked_components = kLinkedComponents;
    info.fixed_tick_hz = PHYSICSRT_FIXED_TICK_HZ;
    CopyText(info.ballanced_build_id, BALLANCE_HEADLESS_BUILD_ROOT);
    CopyText(info.vxmath_build_id, BALLANCE_HEADLESS_BUILD_VXMATH);
    CopyText(info.ck2_build_id, BALLANCE_HEADLESS_BUILD_CK2);
    CopyText(info.render_engine_build_id, BALLANCE_HEADLESS_BUILD_RENDER_ENGINE);
    CopyText(info.building_blocks_build_id, BALLANCE_HEADLESS_BUILD_BUILDING_BLOCKS);
    CopyText(info.parameter_operations_build_id,
             BALLANCE_HEADLESS_BUILD_PARAMETER_OPERATIONS);
    CopyText(info.plugins_build_id, BALLANCE_HEADLESS_BUILD_PLUGINS);
    CopyText(info.physics_source_build_id, physicsBuildInfo.source_build_id);
    CopyText(info.solver_compatibility_id,
             physicsBuildInfo.solver_compatibility_id);
    CopyText(info.compiler, BALLANCE_HEADLESS_BUILD_COMPILER);
    *outInfo = info;
    return BALLANCE_HEADLESS_OK;
}

extern "C" int32_t BallanceHeadlessRuntime_ClassifyPlugin(
        uint32_t pluginCategory,
        uint32_t guidD1,
        uint32_t guidD2,
        BallanceHeadlessPluginClassification *outClassification) {
    if (!outClassification ||
            outClassification->struct_size <
                    sizeof(BallanceHeadlessPluginClassification)) {
        return BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT;
    }
    const int32_t startup = AcquireEngineStartup();
    if (startup != BALLANCE_HEADLESS_OK) {
        return startup;
    }
    const int32_t result = ClassifyRegisteredPlugin(
            pluginCategory, CKGUID(guidD1, guidD2), outClassification);
    ReleaseEngineStartup();
    return result;
}

extern "C" int32_t BallanceHeadlessRuntime_Create(
        BallanceHeadlessRuntime **outRuntime) {
    if (!outRuntime || *outRuntime) {
        return BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT;
    }

    BallanceHeadlessRuntime *runtime = new (std::nothrow) BallanceHeadlessRuntime();
    if (!runtime) {
        return BALLANCE_HEADLESS_ERROR_CONTEXT_CREATION;
    }
    runtime->ownerThread = std::this_thread::get_id();

    int32_t result = AcquireEngineStartup();
    if (result != BALLANCE_HEADLESS_OK) {
        delete runtime;
        return result;
    }
    runtime->startupAcquired = true;

    result = VerifyRegisteredComponents();
    if (result != BALLANCE_HEADLESS_OK) {
        goto fail;
    }

    if (CKCreateContext(&runtime->context, nullptr, 0,
                        CK_CONFIG_DISABLEDINPUT) != CK_OK ||
            !runtime->context) {
        result = BALLANCE_HEADLESS_ERROR_CONTEXT_CREATION;
        goto fail;
    }
    if (runtime->context->SetLoadPolicy(CK_LOAD_POLICY_STRICT) != CK_OK) {
        result = BALLANCE_HEADLESS_ERROR_CONTEXT_CREATION;
        goto fail;
    }

    if (!runtime->context->GetRenderManager()) {
        result = BALLANCE_HEADLESS_ERROR_PLUGIN_INITIALIZATION;
        goto fail;
    }

    {
        CKIpionManager *physicsManager = CKIpionManager::GetManager(runtime->context);
        if (!physicsManager) {
            result = BALLANCE_HEADLESS_ERROR_PHYSICS;
            goto fail;
        }
        physicsManager->CreateEnvironment();
        if (!physicsManager->GetEnvironment()) {
            result = BALLANCE_HEADLESS_ERROR_PHYSICS;
            goto fail;
        }

        PhysicsRT_BuildInfo physicsBuildInfo = {};
        if (!PhysicsBuildInfo(&physicsBuildInfo, &runtime->physicsApi) ||
                !runtime->physicsApi->acquire_world ||
                !runtime->physicsApi->set_authority_mode ||
                !runtime->physicsApi->step_fixed ||
                runtime->physicsApi->acquire_world(
                        runtime->context, &runtime->physicsWorld) != PHYSICSRT_OK ||
                runtime->physicsApi->set_authority_mode(
                        runtime->physicsWorld, 1) != PHYSICSRT_OK) {
            result = BALLANCE_HEADLESS_ERROR_PHYSICS;
            goto fail;
        }
    }

    *outRuntime = runtime;
    return BALLANCE_HEADLESS_OK;

fail:
    if (runtime->context) {
        CKCloseContext(runtime->context);
    }
    if (runtime->startupAcquired) {
        ReleaseEngineStartup();
    }
    delete runtime;
    return result;
}

extern "C" void *BallanceHeadlessRuntime_GetCKContext(
        BallanceHeadlessRuntime *runtime) {
    if (!runtime || runtime->ownerThread != std::this_thread::get_id()) {
        return nullptr;
    }
    return runtime->context;
}

extern "C" uint64_t BallanceHeadlessRuntime_GetPhysicsWorld(
        BallanceHeadlessRuntime *runtime) {
    if (!runtime || runtime->ownerThread != std::this_thread::get_id()) {
        return PHYSICSRT_INVALID_WORLD;
    }
    return runtime->physicsWorld;
}

extern "C" int32_t BallanceHeadlessRuntime_Step(
        BallanceHeadlessRuntime *runtime, uint32_t tickCount) {
    if (!runtime || tickCount == 0 ||
            tickCount > PHYSICSRT_MAX_FIXED_STEPS_PER_CALL) {
        return BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT;
    }
    if (runtime->ownerThread != std::this_thread::get_id()) {
        return BALLANCE_HEADLESS_ERROR_WRONG_THREAD;
    }

    if (!runtime->context->IsPlaying() &&
            runtime->context->GetCurrentLevel() &&
            runtime->context->Play() != CK_OK) {
        return BALLANCE_HEADLESS_ERROR_ENGINE_STEP;
    }

    constexpr float kFixedDeltaMilliseconds =
            1000.0f / static_cast<float>(PHYSICSRT_FIXED_TICK_HZ);
    for (uint32_t tick = 0; tick < tickCount; ++tick) {
        if (runtime->context->ProcessFixed(kFixedDeltaMilliseconds) != CK_OK) {
            return BALLANCE_HEADLESS_ERROR_ENGINE_STEP;
        }
        if (runtime->physicsApi->step_fixed(runtime->physicsWorld, 1) !=
                PHYSICSRT_OK) {
            return BALLANCE_HEADLESS_ERROR_PHYSICS;
        }
    }
    return BALLANCE_HEADLESS_OK;
}

extern "C" int32_t BallanceHeadlessRuntime_Destroy(
        BallanceHeadlessRuntime **runtimePointer) {
    if (!runtimePointer || !*runtimePointer) {
        return BALLANCE_HEADLESS_OK;
    }
    BallanceHeadlessRuntime *runtime = *runtimePointer;
    if (runtime->ownerThread != std::this_thread::get_id()) {
        return BALLANCE_HEADLESS_ERROR_WRONG_THREAD;
    }

    if (runtime->context) {
        CKCloseContext(runtime->context);
    }
    if (runtime->startupAcquired) {
        ReleaseEngineStartup();
    }
    delete runtime;
    *runtimePointer = nullptr;
    return BALLANCE_HEADLESS_OK;
}
