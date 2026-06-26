cmake_minimum_required(VERSION 3.20)
project(SuckbongMachine VERSION 0.5.43 LANGUAGES CXX)

option(SBM_STATIC_BUILD "Link Qt/OpenCV statically for a single portable executable" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(SBM_STATIC_BUILD)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(OpenCV CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(SBM_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/app/TemplateCaptureHotkeySettings.cpp
    src/app/ClickCursorHotkeySettings.cpp
    src/app/HotkeyManager.cpp
    src/app/FeatureHotkeyGate.cpp
    src/app/UserInputInterruptMonitor.cpp
    src/app/ProgramSettings.cpp
    src/app/UpdateChecker.cpp
    src/model/Feature.cpp
    src/model/FeatureRunMode.cpp
    src/model/UserInputInterruptMode.cpp
    src/model/Project.cpp
    src/core/RunWarmup.cpp
    src/core/capture/ScreenCapture.cpp
    src/core/capture/DxgiScreenCapture.cpp
    src/core/capture/WindowPicker.cpp
    src/core/capture/CursorPositionPicker.cpp
    src/core/input/InputSimulator.cpp
    src/core/input/HotkeyBinding.cpp
    src/core/vision/ImageMatcher.cpp
    src/core/workflow/BlockFactory.cpp
    src/core/workflow/Workflow.cpp
    src/core/workflow/WorkflowEngine.cpp
    src/core/workflow/WorkflowRunner.cpp
    src/core/workflow/WorkflowLoopRegion.cpp
    src/core/workflow/LoopExitCondition.cpp
    src/core/workflow/ExecutionContext.cpp
    src/core/workflow/blocks/ImageFindBlock.cpp
    src/core/workflow/blocks/ClickBlock.cpp
    src/core/workflow/blocks/KeyPressBlock.cpp
    src/core/workflow/blocks/WaitBlock.cpp
    src/core/workflow/blocks/IfBlock.cpp
    src/core/workflow/blocks/LoopBlock.cpp
    src/core/workflow/blocks/CommentBlock.cpp
    src/storage/JsonSerializer.cpp
    src/ui/FeatureListPanel.cpp
    src/ui/FeatureListWidget.cpp
    src/ui/HotkeyBindingIcon.cpp
    src/ui/TargetWindowDetailPanel.cpp
    src/ui/CustomTitleBar.cpp
    src/ui/UiStateManager.cpp
    src/ui/ProgramSettingsDialog.cpp
    src/ui/editors/FeatureRunModeDialog.cpp
    src/ui/editors/FeatureEditDialog.cpp
    src/ui/WorkflowEditorPanel.cpp
    src/ui/BlockListWidget.cpp
    src/ui/widgets/AnimatedTwoWaySwitch.cpp
    src/ui/widgets/DragAdjustSpinBox.cpp
    src/ui/widgets/DragAdjustDoubleSpinBox.cpp
    src/ui/widgets/HintLabel.cpp
    src/ui/editors/BlockEditorDialog.cpp
    src/ui/editors/ImageFindEditor.cpp
    src/ui/editors/RoiPreviewWidget.cpp
    src/ui/editors/ImageFindPreviewDialog.cpp
    src/ui/editors/MatchTestOverlay.cpp
    src/ui/editors/WorkflowMatchFeedbackOverlay.cpp
    src/ui/OpenCvQtUtil.cpp
    src/ui/editors/ClickEditor.cpp
    src/ui/editors/KeyPressEditor.cpp
    src/ui/editors/HotkeyCaptureDialog.cpp
    src/ui/editors/WaitEditor.cpp
    src/ui/editors/IfEditor.cpp
    src/ui/editors/LoopEditor.cpp
    src/ui/editors/WorkflowLoopRegionsDialog.cpp
    src/ui/editors/BranchWorkflowEditor.cpp
    src/ui/editors/CommentEditor.cpp
    src/ui/editors/ScreenRegionOverlay.cpp
    src/ui/editors/RoiPreviewOverlay.cpp
    src/ui/editors/CaptureConfirmDialog.cpp
    src/ui/editors/TemplateCaptureDialog.cpp
    src/ui/editors/CaptureCanvas.cpp
    resources/app.qrc
)

if(WIN32)
    enable_language(RC)
    list(APPEND SBM_SOURCES resources/SuckbongMachine.rc)
endif()

add_executable(SuckbongMachine WIN32 ${SBM_SOURCES})

target_include_directories(SuckbongMachine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(SuckbongMachine PRIVATE
    Qt6::Widgets
    Qt6::Network
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    nlohmann_json::nlohmann_json
)

if(WIN32 AND NOT SBM_STATIC_BUILD)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "$ENV{Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET SuckbongMachine POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:SuckbongMachine>"
            COMMENT "Deploying Qt runtime..."
        )
    endif()
endif()

if(SBM_STATIC_BUILD)
    qt_import_plugins(SuckbongMachine
        INCLUDE Qt6::QWindowsIntegrationPlugin
        INCLUDE Qt6::QModernWindowsStylePlugin
        INCLUDE Qt6::QGifPlugin
        INCLUDE Qt6::QICOPlugin
        INCLUDE Qt6::QJpegPlugin
        INCLUDE Qt6::QSchannelBackendPlugin
    )
    add_custom_command(TARGET SuckbongMachine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:SuckbongMachine>" "${CMAKE_SOURCE_DIR}/dist/SuckbongMachine.exe"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:SuckbongMachineUpdater>" "${CMAKE_SOURCE_DIR}/dist/SuckbongMachineUpdater.exe"
        COMMENT "Copying single-exe build to dist/"
    )
endif()

if(WIN32)
    target_link_libraries(SuckbongMachine PRIVATE user32 gdi32 dwmapi dxgi d3d11 winmm)
endif()

target_compile_definitions(SuckbongMachine PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    SBM_VERSION="${PROJECT_VERSION}"
    SBM_UPDATE_GITHUB_REPO="Baegovda/SBM-releases"
    SBM_UPDATE_ASSET_NAME="SuckbongMachine.exe"
)

if(SBM_STATIC_BUILD)
    target_compile_definitions(SuckbongMachine PRIVATE QT_STATIC_BUILD)
endif()

if(WIN32)
    add_executable(SuckbongMachineUpdater WIN32 src/updater/main.cpp)
    target_link_libraries(SuckbongMachineUpdater PRIVATE shell32)
    target_compile_definitions(SuckbongMachineUpdater PRIVATE
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )

    add_custom_command(TARGET SuckbongMachine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:SuckbongMachineUpdater>"
                "$<TARGET_FILE_DIR:SuckbongMachine>/SuckbongMachineUpdater.exe"
        COMMENT "Copying SuckbongMachineUpdater next to main executable"
    )
endif()
