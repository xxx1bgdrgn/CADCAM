list(APPEND CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/common/ui/qt"
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/common/cad/occt")

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
set(CADCAM_OCCT_COMPONENTS FoundationClasses ModelingData ModelingAlgorithms Visualization)

if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    # The application-only solutions must not reference a hidden ZERO_CHECK project.
    set(CMAKE_SUPPRESS_REGENERATION ON)
endif()

function(cadcam_set_debugger_path target)
    set_property(TARGET ${target} PROPERTY VS_DEBUGGER_ENVIRONMENT
        "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$<TARGET_FILE_DIR:TKernel>;%PATH%")
endfunction()
