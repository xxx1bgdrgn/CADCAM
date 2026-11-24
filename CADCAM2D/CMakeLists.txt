cmake_minimum_required(VERSION 3.24)
project(CADCAM2D VERSION 0.1.0 LANGUAGES CXX)

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
)
set(CADCAM_SHARED_FILES
    ../common/Common/Angles.h
    ../common/Rendering/ContourColor.h
    ../common/Shape/Primitives.h
    ../common/Shape/Primitives2D.h
    ../common/Shape/Shape.h
    ../common/Shape/OcctGeometry2D.h
    ../common/Shape/OcctGeometry2D.cpp)
add_library(cadcam_project ${CADCAM_PROJECT_FILES})
target_include_directories(cadcam_project PUBLIC src ../common)

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
    include(../cmake/DesktopSdk.cmake)
    find_package(OpenCASCADE REQUIRED COMPONENTS ${CADCAM_OCCT_COMPONENTS})
    set(CADCAM_DESKTOP_FILES
        ${CADCAM_PROJECT_FILES}
        src/App/main.cpp
        src/Features/Editing/AddShapeCommand.h
        src/Input/Mouse/CadViewMouse.cpp
        src/Rendering/CadView.cpp
        src/Rendering/CadView.h
        src/UI/MainWindow.cpp
        src/UI/MainWindow.h
    )
    qt_add_executable(CADCAM2D WIN32 ${CADCAM_DESKTOP_FILES} ${CADCAM_SHARED_FILES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${CADCAM_DESKTOP_FILES})
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../common" PREFIX "Shared" FILES ${CADCAM_SHARED_FILES})
    target_include_directories(CADCAM2D PRIVATE src ../common "${OpenCASCADE_INCLUDE_DIR}")
    target_link_libraries(CADCAM2D PRIVATE
        Qt6::Widgets          # UI
        TKTopAlgo TKGeomBase  # Geometry
        TKOpenGl TKV3d        # Rendering
    )
    cadcam_set_debugger_path(CADCAM2D)
endif()
