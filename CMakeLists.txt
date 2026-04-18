cmake_minimum_required(VERSION 3.20)
project(cpp-fsrs VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Header-only C++23 port of py-fsrs")

# ---- Core: headers only, no external deps. ----
add_library(fsrs INTERFACE)
add_library(fsrs::fsrs ALIAS fsrs)

target_include_directories(fsrs INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(fsrs INTERFACE cxx_std_23)

# ---- JSON variant: core + nlohmann/json. Opt-out via -DCPP_FSRS_WITH_JSON=OFF. ----
option(CPP_FSRS_WITH_JSON "Build the fsrs-json target (requires nlohmann/json >= 3.0)" ON)
if(CPP_FSRS_WITH_JSON)
    find_package(nlohmann_json 3.0.0 QUIET)
    if(nlohmann_json_FOUND)
        add_library(fsrs-json INTERFACE)
        add_library(fsrs::fsrs-json ALIAS fsrs-json)
        target_link_libraries(fsrs-json INTERFACE fsrs nlohmann_json::nlohmann_json)
    else()
        message(STATUS
            "cpp-fsrs: nlohmann/json not found; the fsrs-json target is disabled.\n"
            "          Install nlohmann/json or link it yourself alongside fsrs::fsrs,\n"
            "          or pass -DCPP_FSRS_WITH_JSON=OFF to silence this message.")
    endif()
endif()

# ---- Install target (only when this project is built standalone, not embedded). ----
if(NOT DEFINED CPP_FSRS_INSTALL)
    if(PROJECT_IS_TOP_LEVEL)
        set(CPP_FSRS_INSTALL ON)
    else()
        set(CPP_FSRS_INSTALL OFF)
    endif()
endif()

if(CPP_FSRS_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/fsrs
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    set(_targets fsrs)
    if(TARGET fsrs-json)
        list(APPEND _targets fsrs-json)
    endif()

    install(TARGETS ${_targets}
            EXPORT cpp-fsrsTargets)

    install(EXPORT cpp-fsrsTargets
            NAMESPACE fsrs::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-fsrs
            FILE cpp-fsrsTargets.cmake)

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-fsrsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cpp-fsrsConfig.cmake"
"include(CMakeFindDependencyMacro)
if(@CPP_FSRS_WITH_JSON@)
    find_dependency(nlohmann_json 3.0.0)
endif()
include(\"\${CMAKE_CURRENT_LIST_DIR}/cpp-fsrsTargets.cmake\")
")

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-fsrsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cpp-fsrsConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp-fsrs)
endif()
