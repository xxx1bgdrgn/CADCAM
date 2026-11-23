cmake_minimum_required(VERSION 3.24)
project(CADCAM3D VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "The OCCT viewport embeds a Windows native window.")
endif()
if(MSVC)
    add_compile_options(/utf-8)
endif()

include(../cmake/DesktopSdk.cmake)
find_package(OpenCASCADE REQUIRED COMPONENTS
    ${CADCAM_OCCT_COMPONENTS} ApplicationFramework DataExchange)

set(CADCAM_DESKTOP_FILES
    src/App/main.cpp
    src/Features/Project/TubePart.h
    src/Features/Import/StepImport.cpp
    src/Features/Import/StepImport.h
    src/Geometry/Primitives3D.h
    src/Geometry/TubeGeometry.cpp
    src/Geometry/TubeGeometry.h
    src/Rendering/TubeView.cpp
    src/Rendering/TubeView.h
    src/UI/MainWindow.cpp
    src/UI/MainWindow.h
    src/UI/TubeWizard.cpp
    src/UI/TubeWizard.h)

set(CADCAM_SHARED_FILES ../common/Shape/Primitives.h ../common/Shape/Shape.h)
qt_add_executable(CADCAM3D WIN32 ${CADCAM_DESKTOP_FILES} ${CADCAM_SHARED_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${CADCAM_DESKTOP_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/../common" PREFIX "Shared" FILES ${CADCAM_SHARED_FILES})
target_include_directories(CADCAM3D PRIVATE src ../common "${OpenCASCADE_INCLUDE_DIR}")
target_link_libraries(CADCAM3D PRIVATE
    Qt6::Widgets TKTopAlgo TKGeomBase TKPrim TKOpenGl TKV3d TKDESTEP TKXCAF)
cadcam_set_debugger_path(CADCAM3D)
