#ifndef BALLANCE_HEADLESS_RUNTIME_H
#define BALLANCE_HEADLESS_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BALLANCE_HEADLESS_RUNTIME_ABI_VERSION UINT32_C(1)
#define BALLANCE_HEADLESS_BUILD_ID_CAPACITY UINT32_C(65)
#define BALLANCE_HEADLESS_TEXT_CAPACITY UINT32_C(128)
#define BALLANCE_HEADLESS_MODULE_NAME_CAPACITY UINT32_C(64)

typedef struct BallanceHeadlessRuntime BallanceHeadlessRuntime;

typedef enum BallanceHeadlessResult
{
    BALLANCE_HEADLESS_OK = 0,
    BALLANCE_HEADLESS_ERROR_INVALID_ARGUMENT = -1,
    BALLANCE_HEADLESS_ERROR_INCOMPATIBLE_COMPONENT = -2,
    BALLANCE_HEADLESS_ERROR_ENGINE_STARTUP = -3,
    BALLANCE_HEADLESS_ERROR_CONTEXT_CREATION = -4,
    BALLANCE_HEADLESS_ERROR_PLUGIN_INITIALIZATION = -5,
    BALLANCE_HEADLESS_ERROR_PHYSICS = -6,
    BALLANCE_HEADLESS_ERROR_WRONG_THREAD = -7,
    BALLANCE_HEADLESS_ERROR_ENGINE_STEP = -8,
    BALLANCE_HEADLESS_ERROR_UNKNOWN_PLUGIN = -9
} BallanceHeadlessResult;

typedef enum BallanceHeadlessComponent
{
    BALLANCE_HEADLESS_COMPONENT_CK2 = UINT32_C(1) << 0,
    BALLANCE_HEADLESS_COMPONENT_NULL_RENDERER = UINT32_C(1) << 1,
    BALLANCE_HEADLESS_COMPONENT_PARAMETER_OPERATIONS = UINT32_C(1) << 2,
    BALLANCE_HEADLESS_COMPONENT_VIRTOOLS_LOADER = UINT32_C(1) << 3,
    BALLANCE_HEADLESS_COMPONENT_PHYSICS_RT = UINT32_C(1) << 4,
    BALLANCE_HEADLESS_COMPONENT_INPUT_STUB = UINT32_C(1) << 5
} BallanceHeadlessComponent;

typedef enum BallanceHeadlessPluginFlags
{
    BALLANCE_HEADLESS_PLUGIN_AVAILABLE = UINT32_C(1) << 0,
    BALLANCE_HEADLESS_PLUGIN_ENGINE = UINT32_C(1) << 1,
    BALLANCE_HEADLESS_PLUGIN_GAMEPLAY = UINT32_C(1) << 2,
    BALLANCE_HEADLESS_PLUGIN_PRESENTATION = UINT32_C(1) << 3,
    BALLANCE_HEADLESS_PLUGIN_PLAYER_SCOPED = UINT32_C(1) << 4,
    BALLANCE_HEADLESS_PLUGIN_PHYSICS = UINT32_C(1) << 5,
    BALLANCE_HEADLESS_PLUGIN_NO_BACKEND = UINT32_C(1) << 6
} BallanceHeadlessPluginFlags;

typedef struct BallanceHeadlessPluginClassification
{
    uint32_t struct_size;
    uint32_t flags;
    uint32_t plugin_category;
    uint32_t reserved;
    char module_name[BALLANCE_HEADLESS_MODULE_NAME_CAPACITY];
} BallanceHeadlessPluginClassification;

typedef struct BallanceHeadlessBuildInfo
{
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t linked_components;
    uint32_t fixed_tick_hz;
    char ballanced_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char vxmath_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char ck2_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char render_engine_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char building_blocks_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char parameter_operations_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char plugins_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char physics_source_build_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char solver_compatibility_id[BALLANCE_HEADLESS_BUILD_ID_CAPACITY];
    char compiler[BALLANCE_HEADLESS_TEXT_CAPACITY];
} BallanceHeadlessBuildInfo;

/* Validates every statically linked component and its solver contract. */
int32_t BallanceHeadlessRuntime_VerifyLinkedComponents(void);

/* out_info->struct_size must be initialized to sizeof(*out_info). */
int32_t BallanceHeadlessRuntime_GetBuildInfo(BallanceHeadlessBuildInfo *out_info);

/*
 * Resolves a CK dependency GUID through the static server allowlist and
 * reports its execution semantics. Unknown GUIDs fail closed.
 * out_classification->struct_size must be initialized by the caller.
 */
int32_t BallanceHeadlessRuntime_ClassifyPlugin(
        uint32_t plugin_category,
        uint32_t guid_d1,
        uint32_t guid_d2,
        BallanceHeadlessPluginClassification *out_classification);

/* Creates one independent CK/IVP room world on the calling thread. */
int32_t BallanceHeadlessRuntime_Create(BallanceHeadlessRuntime **out_runtime);

/* Returns the owned CKContext as an opaque pointer for map loading/certification. */
void *BallanceHeadlessRuntime_GetCKContext(BallanceHeadlessRuntime *runtime);

/* Returns the physics_RT world handle, or zero for an invalid runtime. */
uint64_t BallanceHeadlessRuntime_GetPhysicsWorld(BallanceHeadlessRuntime *runtime);

/* Runs CK behavior processing followed by exact 1/66 s IVP ticks. */
int32_t BallanceHeadlessRuntime_Step(BallanceHeadlessRuntime *runtime,
                                    uint32_t tick_count);

/* Idempotent for a null pointer and clears *runtime on success. */
int32_t BallanceHeadlessRuntime_Destroy(BallanceHeadlessRuntime **runtime);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BALLANCE_HEADLESS_RUNTIME_H */
