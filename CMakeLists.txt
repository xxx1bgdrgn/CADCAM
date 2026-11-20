cmake_minimum_required(VERSION 3.24)
project(CADCAM VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CADCAM_PROJECT_FILES
    src/Features/Project/Project.cpp
    src/Features/Project/Project.h
    src/Features/Views/CurrentZone.cpp
    src/Features/Views/CurrentZone.h
    src/Geometry/Primitives.h
)
add_library(cadcam_project ${CADCAM_PROJECT_FILES})
target_include_directories(cadcam_project PUBLIC src)

include(CTest)
if(BUILD_TESTING)
    add_executable(cadcam_project_tests tests/ProjectTests.cpp)
    target_link_libraries(cadcam_project_tests PRIVATE cadcam_project)
    add_test(NAME ProjectViews COMMAND cadcam_project_tests)
endif()

option(CADCAM_BUILD_DESKTOP "Build the Windows Qt and OCCT desktop application" ON)
if(CADCAM_BUILD_DESKTOP)
    if(NOT WIN32)
        message(FATAL_ERROR "The OCCT viewport embeds a Windows native window.")
    endif()
    list(APPEND CMAKE_PREFIX_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ui/qt"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cad/occt")
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    find_package(OpenCASCADE REQUIRED COMPONENTS FoundationClasses ModelingData ModelingAlgorithms Visualization)
    set(CADCAM_DESKTOP_FILES
        ${CADCAM_PROJECT_FILES}
        src/App/main.cpp
        src/Common/Angles.h
        src/Features/Editing/AddShapeCommand.h
        src/Geometry/OcctGeometry.cpp
        src/Geometry/OcctGeometry.h
        src/Input/Mouse/CadViewMouse.cpp
        src/Rendering/CadView.cpp
        src/Rendering/CadView.h
        src/UI/MainWindow.cpp
        src/UI/MainWindow.h
    )
    qt_add_executable(CADCAM WIN32 ${CADCAM_DESKTOP_FILES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${CADCAM_DESKTOP_FILES})
    target_include_directories(CADCAM PRIVATE src "${OpenCASCADE_INCLUDE_DIR}")
    target_link_libraries(CADCAM PRIVATE
        Qt6::Widgets          # UI
        TKTopAlgo TKGeomBase  # Geometry
        TKOpenGl TKV3d        # Rendering
    )
    set_property(TARGET CADCAM PROPERTY VS_DEBUGGER_ENVIRONMENT
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$<TARGET_FILE_DIR:TKernel>;%PATH%")

    if(CMAKE_GENERATOR STREQUAL "Visual Studio 18 2026")
        # The IDE solution must build the application without hidden project references.
        set(CMAKE_SUPPRESS_REGENERATION ON)
        file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ide/CADCAM.slnx" CONTENT [=[
<Solution>
  <Configurations>
    <BuildType Name="Debug" />
    <BuildType Name="Release" />
    <BuildType Name="MinSizeRel" />
    <BuildType Name="RelWithDebInfo" />
    <Platform Name="@CMAKE_VS_PLATFORM_NAME@" />
  </Configurations>
  <Project Path="../CADCAM.vcxproj" DefaultStartup="true" />
</Solution>
]=] @ONLY)
    endif()
endif()
