cmake_minimum_required(VERSION 3.15)

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    foreach (VCPKG_ROOT_CANDIDATE "${CMAKE_SOURCE_DIR}/vcpkg" "${CMAKE_SOURCE_DIR}/../vcpkg")
        if (EXISTS "${VCPKG_ROOT_CANDIDATE}/scripts/buildsystems/vcpkg.cmake")
            set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT_CANDIDATE}/scripts/buildsystems/vcpkg.cmake")
            break()
        endif ()
    endforeach ()
endif ()

project(ManyTargetsFix CXX)

set(ZHMMODSDK_VER "v4.1.1")
include(cmake/setup-zhmmodsdk.cmake)

set(CMAKE_CXX_STANDARD 23)

add_library(ManyTargetsFix SHARED
    src/ManyTargetsFix.cpp
    src/ManyTargetsFix.h
    src/GameCode.cpp
    src/GameCode.h
    src/TouchBufferFix.cpp
    src/TouchBufferFix.h
    src/TrackerRays.cpp
    src/TrackerRays.h
    src/InstinctGlow.cpp
    src/InstinctGlow.h
    src/GlowState.h
    src/GlowGroups.cpp
    src/GlowGroups.h
    src/GlowSpeedups.cpp
    src/GlowSpeedups.h
)

set_target_properties(ManyTargetsFix PROPERTIES COMPILE_FLAGS "/utf-8")
target_link_options(ManyTargetsFix PRIVATE "/PDBALTPATH:%_PDB%")

find_package(directx-headers CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

target_link_libraries(ManyTargetsFix
    ZHMModSDK
    Microsoft::DirectX-Guids
    Microsoft::DirectX-Headers
    implot
    minhook::minhook
)

install(TARGETS ManyTargetsFix
    RUNTIME DESTINATION bin
)

zhmmodsdk_install(ManyTargetsFix)
