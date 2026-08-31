# Headless runtime building blocks owned by the Ballanced superproject.  These
# targets deliberately do not enter the SDL-backed VxMath project so that a
# server-only configure works on machines where SDL is not installed.

if (MSVC AND BALLANCE_HEADLESS_ONLY)
    # CMake's MSVC platform defaults add these libraries to every executable's
    # explicit link line. Remove them for the server-only directory so the
    # headless targets cannot silently regain Win32 UI or GDI dependencies.
    foreach (_ballance_headless_language IN ITEMS C CXX)
        separate_arguments(_ballance_headless_standard_libraries WINDOWS_COMMAND
                "${CMAKE_${_ballance_headless_language}_STANDARD_LIBRARIES}")
        list(FILTER _ballance_headless_standard_libraries EXCLUDE REGEX
                "^(user32|gdi32)\\.lib$")
        string(JOIN " " _ballance_headless_standard_libraries
                ${_ballance_headless_standard_libraries})
        set(CMAKE_${_ballance_headless_language}_STANDARD_LIBRARIES
                "${_ballance_headless_standard_libraries}")
    endforeach ()
endif ()

set(_ballance_vxmath_root "${CMAKE_CURRENT_LIST_DIR}/../VxMath")
set(_ballance_vxmath_include "${_ballance_vxmath_root}/include")
set(_ballance_vxmath_source "${_ballance_vxmath_root}/src")

set(_ballance_vxmath_common_sources
        VxMath.cpp
        VxSIMDDispatch.cpp
        VxBlitDispatchBridge.cpp
        VxGraphicUtils.cpp
        VxImageKernels.cpp
        VxImageKernelsSSE2.cpp
        VxBlitKernels.cpp
        VxBlitEngineSSE2.cpp
        VxBlitEngineSSSE3.cpp
        VxBlitEngineClass.cpp
        VxProcessor.cpp
        CKPathSplitter.cpp
        CKDirectoryParser.cpp
        VxVector.cpp
        VxConfiguration.cpp
        VxQuaternion.cpp
        VxIntersect_BoxBox.cpp
        VxIntersect_Plane.cpp
        VxIntersect_FaceFace.cpp
        VxIntersect_Frustum.cpp
        VxIntersect_Sphere.cpp
        VxDistance.cpp
        VxMemory.cpp
        XString.cpp
        EscapeURL.cpp
        NeuQuant.cpp
)
list(TRANSFORM _ballance_vxmath_common_sources
        PREPEND "${_ballance_vxmath_source}/")

if (WIN32)
    set(_ballance_vxmath_platform_sources
            src/VxSharedLibraryHeadlessWin32.cpp
            src/VxMemoryMappedFileHeadlessWin32.cpp
            src/VxWindowFunctionsHeadlessWin32.cpp
            src/VxTimeProfilerHeadless.cpp
            src/VxMutexHeadless.cpp
            src/VxThreadHeadlessWin32.cpp
    )
else ()
    # The checked-in POSIX implementations use only libc, libdl and pthread.
    set(_ballance_vxmath_platform_sources
            "${_ballance_vxmath_source}/VxSharedLibraryPosix.cpp"
            "${_ballance_vxmath_source}/VxMemoryMappedFilePosix.cpp"
            "${_ballance_vxmath_source}/VxWindowFunctionsPosix.cpp"
            "${_ballance_vxmath_source}/VxTimeProfilerPosix.cpp"
            "${_ballance_vxmath_source}/VxMutexPosix.cpp"
            "${_ballance_vxmath_source}/VxThreadPosix.cpp"
            src/VxPlatformSupplementPosix.cpp
    )
endif ()

add_library(VxMathHeadlessStatic STATIC
        ${_ballance_vxmath_common_sources}
        ${_ballance_vxmath_platform_sources}
)
add_library(VxMath::HeadlessStatic ALIAS VxMathHeadlessStatic)

target_compile_features(VxMathHeadlessStatic PUBLIC cxx_std_17)
target_compile_definitions(VxMathHeadlessStatic
        PUBLIC
        VX_LIB
        VX_SIMD_NONE=1
        VX_HEADLESS=1
)
target_include_directories(VxMathHeadlessStatic
        PUBLIC
        $<BUILD_INTERFACE:${_ballance_vxmath_include}>
        PRIVATE
        "${_ballance_vxmath_source}"
        "${_ballance_vxmath_root}/deps/stb"
)
set_target_properties(VxMathHeadlessStatic PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        POSITION_INDEPENDENT_CODE ON
)

if (MSVC)
    target_compile_definitions(VxMathHeadlessStatic PRIVATE
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
    )
    # Make GUI regressions fail at link time for every consumer, even though
    # CMake's stock MSVC platform file lists these libraries by default.
    target_link_options(VxMathHeadlessStatic INTERFACE
            /NODEFAULTLIB:user32.lib
            /NODEFAULTLIB:gdi32.lib
    )
else ()
    find_package(Threads REQUIRED)
    target_link_libraries(VxMathHeadlessStatic PUBLIC
            Threads::Threads
            ${CMAKE_DL_LIBS}
    )
endif ()

if (BALLANCE_HEADLESS_ONLY)
    # Existing component projects discover these historical target names.  In
    # a headless-only configure they deliberately resolve to the native
    # VxMath implementation above instead of entering VxMath's SDL project.
    if (NOT TARGET VxMath)
        add_library(VxMath ALIAS VxMathHeadlessStatic)
    endif ()
    if (NOT TARGET VxMathStatic)
        add_library(VxMathStatic ALIAS VxMathHeadlessStatic)
    endif ()
    if (NOT TARGET stb)
        add_library(stb INTERFACE)
        target_include_directories(stb INTERFACE
                "${_ballance_vxmath_root}/deps/stb")
    endif ()

    set(CK2_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CK2_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(CK2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CK2_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../CK2"
            "${CMAKE_CURRENT_BINARY_DIR}/CK2")

    set(CKRE_BUILD_HEADLESS ON CACHE BOOL "" FORCE)
    set(CKRE_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CKRE_BUILD_STATIC OFF CACHE BOOL "" FORCE)
    set(CKRE_BUILD_BGFX_RASTERIZER OFF CACHE BOOL "" FORCE)
    set(CKRE_GENERATE_SHADERS OFF CACHE BOOL "" FORCE)
    set(CKRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CKRE_BUILD_HEADLESS_TESTS OFF CACHE BOOL "" FORCE)
    set(CKRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CKRE_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../RenderEngine"
            "${CMAKE_CURRENT_BINARY_DIR}/RenderEngine")

    set(CKPARAMOP_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CKPARAMOP_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(CKPARAMOP_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../Managers/ParameterOperations"
            "${CMAKE_CURRENT_BINARY_DIR}/ParameterOperations")

    set(CKPLUGINS_BUILD_AVIREADER OFF CACHE BOOL "" FORCE)
    set(CKPLUGINS_BUILD_IMAGEREADER OFF CACHE BOOL "" FORCE)
    # WAV decoding is an asset reader used by the original levels.  It is
    # independent of the SDL sound-output manager and is therefore part of the
    # certified headless graph.
    set(CKPLUGINS_BUILD_WAVREADER ON CACHE BOOL "" FORCE)
    set(CKPLUGINS_BUILD_VIRTOOLSLOADER ON CACHE BOOL "" FORCE)
    set(CKPLUGINS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CKPLUGINS_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(CKPLUGINS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CKPLUGINS_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../Plugins"
            "${CMAKE_CURRENT_BINARY_DIR}/Plugins")

    # Explicit cross-platform allowlist used by the original Ballance behavior
    # graphs. MidiManager is intentionally absent, as are the SDL managers.
    # Keeping CKBB_BUILD_ALL_MODULES disabled prevents newly-added modules from
    # entering the authoritative server without an explicit review here.
    set(CKBB_BUILD_ALL_MODULES OFF CACHE BOOL "" FORCE)
    set(_ballance_headless_bb_modules
            3DTrans
            BuildingBlocksAddons1
            Cameras
            Characters
            Collision
            Controllers
            Grids
            Interface
            Lights
            Logics
            Materials-Textures
            MeshModifiers
            Narratives
            Sounds
            Visuals
            WorldEnvironment
            physics_RT
            TT_DatabaseManager_RT
            TT_Gravity_RT
            TT_InterfaceManager_RT
            TT_ParticleSystems_RT
            TT_Toolbox_RT)
    foreach (_ballance_headless_bb IN LISTS _ballance_headless_bb_modules)
        set("CKBB_BUILD_${_ballance_headless_bb}" ON CACHE BOOL "" FORCE)
    endforeach ()
    set(CKBB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(CKBB_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(CKBB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CKBB_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../BuildingBlocks"
            "${CMAKE_CURRENT_BINARY_DIR}/BuildingBlocks")

    if (WIN32 AND TARGET InterfaceStatic)
        # The server keeps Interface BB type/behavior registration for strict
        # map loading, but it has no display or system-font device context.
        # CKBuildingBlocks already provides this windowless implementation;
        # scope it to the Windows headless target so ordinary clients and the
        # Linux stb font backend retain their existing behavior.
        target_compile_definitions(InterfaceStatic PRIVATE
                FONTMANAGER_NOSYSFONT=1)
    endif ()

    foreach (_ballance_headless_required_target IN ITEMS
            CK2Static
            CKRasterizerLib
            CK2_3DHeadlessStatic
            ParameterOperationsStatic
            VirtoolsLoaderStatic
            WavReaderStatic
            3DTransStatic
            BuildingBlocksAddons1Static
            CamerasStatic
            CharactersStatic
            CollisionStatic
            ControllersStatic
            GridsStatic
            InterfaceStatic
            LightsStatic
            LogicsStatic
            MaterialsStatic
            MeshModifiersStatic
            NarrativesStatic
            SoundsStatic
            VisualsStatic
            WorldEnvironmentStatic
            TT_DatabaseManager_RTStatic
            TT_Gravity_RTStatic
            TT_InterfaceManager_RTStatic
            TT_ParticleSystems_RTStatic
            TT_Toolbox_RTStatic
            physics_RTStatic
            # CKIpionManager.h is part of the private integration seam used by
            # this runtime and includes IVP's flat public headers directly.
            # Keep the staged include root and its owning archive attached to
            # the consumer instead of reaching into IVP's source tree.
            ivp_utility)
        if (NOT TARGET ${_ballance_headless_required_target})
            message(FATAL_ERROR
                    "Required headless target was not created: ${_ballance_headless_required_target}")
        endif ()
    endforeach ()

    set(_ballance_headless_generated_dir
            "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_ballance_headless_generated_dir}")
    configure_file(
            src/BallanceHeadlessRuntimeBuild.h.in
            "${_ballance_headless_generated_dir}/BallanceHeadlessRuntimeBuild.h"
            @ONLY)

    add_library(BallanceHeadlessRuntimeStatic STATIC
            src/BallanceHeadlessRuntime.cpp
            include/BallanceHeadlessRuntime.h
            "${_ballance_headless_generated_dir}/BallanceHeadlessRuntimeBuild.h")
    add_library(Ballance::HeadlessRuntime ALIAS BallanceHeadlessRuntimeStatic)
    target_compile_features(BallanceHeadlessRuntimeStatic PUBLIC cxx_std_20)
    target_include_directories(BallanceHeadlessRuntimeStatic
            PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
            PRIVATE
            "${_ballance_headless_generated_dir}")
    target_link_libraries(BallanceHeadlessRuntimeStatic
            PUBLIC
            VxMathHeadlessStatic
            CK2Static
            CKRasterizerLib
            CK2_3DHeadlessStatic
            ParameterOperationsStatic
            VirtoolsLoaderStatic
            WavReaderStatic
            3DTransStatic
            BuildingBlocksAddons1Static
            CamerasStatic
            CharactersStatic
            CollisionStatic
            ControllersStatic
            GridsStatic
            InterfaceStatic
            LightsStatic
            LogicsStatic
            MaterialsStatic
            MeshModifiersStatic
            NarrativesStatic
            SoundsStatic
            VisualsStatic
            WorldEnvironmentStatic
            TT_DatabaseManager_RTStatic
            TT_Gravity_RTStatic
            TT_InterfaceManager_RTStatic
            TT_ParticleSystems_RTStatic
            TT_Toolbox_RTStatic
            physics_RTStatic
            # CKIpionManager.h is part of the private integration seam used by
            # this runtime and includes IVP's flat public headers directly.
            # Keep the staged include root and its owning archive attached to
            # the consumer instead of reaching into IVP's source tree.
            ivp_utility)
    set_target_properties(BallanceHeadlessRuntimeStatic PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            POSITION_INDEPENDENT_CODE ON)

    install(TARGETS BallanceHeadlessRuntimeStatic
            ARCHIVE DESTINATION lib
            COMPONENT Development)
    install(FILES include/BallanceHeadlessRuntime.h
            DESTINATION include
            COMPONENT Development)

    if (BUILD_TESTING)
        add_executable(BallanceHeadlessRuntimeSmoke
                tests/BallanceHeadlessRuntimeSmoke.cpp)
        target_compile_features(BallanceHeadlessRuntimeSmoke PRIVATE cxx_std_20)
        target_link_libraries(BallanceHeadlessRuntimeSmoke PRIVATE
                BallanceHeadlessRuntimeStatic)
        set_target_properties(BallanceHeadlessRuntimeSmoke PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        add_test(NAME BallanceHeadlessRuntimeSmoke
                COMMAND $<TARGET_FILE:BallanceHeadlessRuntimeSmoke>)
        set_tests_properties(BallanceHeadlessRuntimeSmoke PROPERTIES TIMEOUT 60)

        # Asset-backed certification probe.  It is intentionally not a CTest:
        # callers supply a trusted local NMO/CMO path and no game asset is
        # copied into either the build tree or repository.
        add_executable(BallanceHeadlessMapProbe
                tests/BallanceHeadlessMapProbe.cpp)
        target_compile_features(BallanceHeadlessMapProbe PRIVATE cxx_std_20)
        target_link_libraries(BallanceHeadlessMapProbe PRIVATE
                BallanceHeadlessRuntimeStatic)
        set_target_properties(BallanceHeadlessMapProbe PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endif ()
endif ()

if (BUILD_TESTING)
    add_executable(VxMathHeadlessProbe tests/VxMathHeadlessProbe.cpp)
    target_compile_features(VxMathHeadlessProbe PRIVATE cxx_std_17)
    target_link_libraries(VxMathHeadlessProbe PRIVATE VxMathHeadlessStatic)
    add_test(NAME VxMathHeadlessProbe COMMAND VxMathHeadlessProbe)
endif ()
