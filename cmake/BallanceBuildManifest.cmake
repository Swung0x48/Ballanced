# Generate a machine-readable record of the exact component revisions used by
# this assembled runtime. Top-level component commits also pin their nested
# submodules.

find_package(Git QUIET)

set(_ballance_manifest_module_dir "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(BALLANCE_SOURCE_ROOT
        "${_ballance_manifest_module_dir}/.." ABSOLUTE)

function(_ballance_read_revision relative_path variable_prefix)
    set(_revision "unknown")
    set(_dirty true)
    set(_source_path "${BALLANCE_SOURCE_ROOT}/${relative_path}")

    if (GIT_FOUND AND EXISTS "${_source_path}")
        execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${_source_path}" rev-parse HEAD
                RESULT_VARIABLE _revision_result
                OUTPUT_VARIABLE _revision_output
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (_revision_result EQUAL 0)
            set(_revision "${_revision_output}")
        endif ()

        execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${_source_path}" status --porcelain --untracked-files=no
                RESULT_VARIABLE _status_result
                OUTPUT_VARIABLE _status_output
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (_status_result EQUAL 0 AND _status_output STREQUAL "")
            set(_dirty false)
        endif ()
    endif ()

    set(${variable_prefix}_REVISION "${_revision}" PARENT_SCOPE)
    set(${variable_prefix}_DIRTY "${_dirty}" PARENT_SCOPE)
endfunction()

function(_ballance_json_escape input output_variable)
    set(_escaped "${input}")
    string(REPLACE "\\" "\\\\" _escaped "${_escaped}")
    string(REPLACE "\"" "\\\"" _escaped "${_escaped}")
    string(REPLACE "\r" "\\r" _escaped "${_escaped}")
    string(REPLACE "\n" "\\n" _escaped "${_escaped}")
    string(REPLACE "\t" "\\t" _escaped "${_escaped}")
    set(${output_variable} "${_escaped}" PARENT_SCOPE)
endfunction()

_ballance_read_revision("." BALLANCE_ROOT)
_ballance_read_revision("Source/Player" BALLANCE_PLAYER)
_ballance_read_revision("Source/VxMath" BALLANCE_VXMATH)
_ballance_read_revision("Source/CK2" BALLANCE_CK2)
_ballance_read_revision("Source/RenderEngine" BALLANCE_RENDER_ENGINE)
_ballance_read_revision("Source/BuildingBlocks" BALLANCE_BUILDING_BLOCKS)
_ballance_read_revision("Source/Plugins" BALLANCE_PLUGINS)
_ballance_read_revision("Source/Managers/ParameterOperations" BALLANCE_PARAMETER_OPERATIONS)
_ballance_read_revision("Source/Managers/SdlInputManager" BALLANCE_SDL_INPUT_MANAGER)
_ballance_read_revision("Source/Managers/SdlSoundManager" BALLANCE_SDL_SOUND_MANAGER)

# Preserve every nested gitlink in addition to the named top-level components.
# This records IVP, miniz, bgfx's own recursion, stb/SIMDe, and any future
# nested dependency without requiring the manifest schema to know it in advance.
set(BALLANCE_RECURSIVE_SUBMODULES_JSON "")
if (GIT_FOUND)
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${BALLANCE_SOURCE_ROOT}"
                    submodule status --recursive
            RESULT_VARIABLE _ballance_submodule_result
            OUTPUT_VARIABLE _ballance_submodule_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    if (_ballance_submodule_result EQUAL 0 AND NOT _ballance_submodule_output STREQUAL "")
        string(REPLACE "\n" ";" _ballance_submodule_lines "${_ballance_submodule_output}")
        foreach (_ballance_submodule_line IN LISTS _ballance_submodule_lines)
            string(SUBSTRING "${_ballance_submodule_line}" 0 1 _ballance_submodule_state)
            string(SUBSTRING "${_ballance_submodule_line}" 1 -1 _ballance_submodule_body)
            if (_ballance_submodule_body MATCHES "^([0-9a-fA-F]+) ([^ ]+)")
                set(_ballance_submodule_revision "${CMAKE_MATCH_1}")
                set(_ballance_submodule_path "${CMAKE_MATCH_2}")
                string(LENGTH "${_ballance_submodule_revision}"
                        _ballance_submodule_revision_length)
                if (NOT _ballance_submodule_revision_length EQUAL 40)
                    continue()
                endif ()
                _ballance_json_escape("${_ballance_submodule_path}" _ballance_submodule_path_json)
                set(_ballance_submodule_initialized true)
                set(_ballance_submodule_matches_gitlink true)
                if (_ballance_submodule_state STREQUAL "-")
                    set(_ballance_submodule_initialized false)
                elseif (NOT _ballance_submodule_state STREQUAL " ")
                    set(_ballance_submodule_matches_gitlink false)
                endif ()
                if (NOT BALLANCE_RECURSIVE_SUBMODULES_JSON STREQUAL "")
                    string(APPEND BALLANCE_RECURSIVE_SUBMODULES_JSON ",\n")
                endif ()
                string(APPEND BALLANCE_RECURSIVE_SUBMODULES_JSON
                        "    { \"path\": \"${_ballance_submodule_path_json}\", "
                        "\"revision\": \"${_ballance_submodule_revision}\", "
                        "\"initialized\": ${_ballance_submodule_initialized}, "
                        "\"matchesGitlink\": ${_ballance_submodule_matches_gitlink} }")
            endif ()
        endforeach ()
    endif ()
endif ()

if (BALLANCE_HEADLESS_ONLY)
    set(BALLANCE_HEADLESS_ONLY_JSON true)
else ()
    set(BALLANCE_HEADLESS_ONLY_JSON false)
endif ()

if (CMAKE_CXX_STANDARD)
    set(_ballance_cxx_standard "${CMAKE_CXX_STANDARD}")
else ()
    set(_ballance_cxx_standard "per-target")
endif ()
set(_ballance_compile_flags
        "${CMAKE_CXX_FLAGS} | Debug=${CMAKE_CXX_FLAGS_DEBUG} | Release=${CMAKE_CXX_FLAGS_RELEASE} | RelWithDebInfo=${CMAKE_CXX_FLAGS_RELWITHDEBINFO} | MinSizeRel=${CMAKE_CXX_FLAGS_MINSIZEREL}")

foreach (_ballance_manifest_field IN ITEMS
        CMAKE_GENERATOR
        CMAKE_SYSTEM_NAME
        CMAKE_SYSTEM_PROCESSOR
        CMAKE_CXX_COMPILER_ID
        CMAKE_CXX_COMPILER_VERSION
        CMAKE_BUILD_TYPE
        PHYSICS_RT_SOLVER_COMPATIBILITY_ID)
    _ballance_json_escape("${${_ballance_manifest_field}}"
            "BALLANCE_${_ballance_manifest_field}_JSON")
endforeach ()
_ballance_json_escape("${BALLANCE_SOURCE_ROOT}" BALLANCE_SOURCE_ROOT_JSON)
_ballance_json_escape("${_ballance_cxx_standard}" BALLANCE_CXX_STANDARD_JSON)
_ballance_json_escape("${_ballance_compile_flags}" BALLANCE_COMPILE_FLAGS_JSON)

set(_ballance_manifest_dir "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_ballance_manifest_dir}")
set(BALLANCE_BUILD_MANIFEST "${_ballance_manifest_dir}/BallancedBuildManifest.json")
configure_file(
        "${_ballance_manifest_module_dir}/BallancedBuildManifest.json.in"
        "${BALLANCE_BUILD_MANIFEST}"
        @ONLY
)

install(FILES "${BALLANCE_BUILD_MANIFEST}"
        DESTINATION Bin
        COMPONENT Runtime
)
