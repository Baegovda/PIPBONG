cmake_minimum_required(VERSION 3.20)
project(PIPBONG VERSION 0.7.78 LANGUAGES CXX)

option(PIPBONG_SKIP_WINDEPLOY "Skip windeployqt on incremental dev links; run deploy-qt when DLLs are needed" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(OpenCV CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(PIPBONG_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/app/TemplateCaptureHotkeySettings.cpp
    src/app/ClickCursorHotkeySettings.cpp
    src/app/HotkeyManager.cpp
    src/app/FeatureHotkeyGate.cpp
    src/app/UserInputInterruptMonitor.cpp
    src/app/MouseCenterLock.cpp
    src/app/ProgramSettings.cpp
    src/app/WindowsLaunchAtStartup.cpp
    src/app/WindowsRunAsAdmin.cpp
    src/app/PointerFeedbackSettings.cpp
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
    src/core/poeninja/PoeNinjaClient.cpp
    src/core/poeninja/CurrencyNameKo.cpp
    src/core/poeninja/EconomyNameKo.cpp
    src/core/poeninja/PoeNinjaEconomyCategories.cpp
    src/ui/calculator/FormulaEvaluator.cpp
    src/ui/calculator/SpreadsheetModel.cpp
    src/ui/calculator/CurrencyIconCache.cpp
    src/ui/calculator/SpreadsheetCellDelegate.cpp
    src/ui/calculator/CurrencyPickerDialog.cpp
    src/ui/calculator/EconomyFavoritesStore.cpp
    src/ui/calculator/FormulaBuilderDialog.cpp
    src/ui/calculator/CalculatorDialog.cpp
    src/storage/JsonSerializer.cpp
    src/ui/FeatureListPanel.cpp
    src/ui/FeatureListWidget.cpp
    src/ui/HotkeyBindingIcon.cpp
    src/ui/TargetWindowDetailPanel.cpp
    src/ui/CustomTitleBar.cpp
    src/ui/TitleBarIconEasterEgg.cpp
    src/ui/UiStateManager.cpp
    src/ui/ProgramSettingsDialog.cpp
    src/ui/ClickPointerFeedbackSettingsDialog.cpp
    src/ui/ClickPointerFeedbackRenderer.cpp
    src/ui/editors/FeatureEditDialog.cpp
    src/ui/WorkflowEditorPanel.cpp
    src/ui/BlockListWidget.cpp
    src/ui/widgets/AnimatedTwoWaySwitch.cpp
    src/ui/widgets/DragAdjustSpinBox.cpp
    src/ui/widgets/DragAdjustDoubleSpinBox.cpp
    src/ui/widgets/HintLabel.cpp
    src/ui/widgets/ClickPointerFeedbackPreviewWidget.cpp
    src/ui/editors/BlockEditorDialog.cpp
    src/ui/editors/ImageFindEditor.cpp
    src/ui/editors/ImageFindPollIntervalPrefs.cpp
    src/ui/editors/MatchTestOverlay.cpp
    src/ui/editors/WorkflowMatchFeedbackOverlay.cpp
    src/ui/editors/WorkflowRoiFlashOverlay.cpp
    src/ui/TargetWindowHighlightOverlay.cpp
    src/ui/WindowPickerHoverOverlay.cpp
    src/ui/OpenCvQtUtil.cpp
    src/ui/editors/ClickEditor.cpp
    src/ui/editors/KeyPressEditor.cpp
    src/ui/editors/HotkeyCaptureDialog.cpp
    src/ui/editors/WaitEditor.cpp
    src/ui/editors/WorkflowLoopRegionsDialog.cpp
    src/ui/editors/ScreenRegionOverlay.cpp
    src/ui/editors/RoiPreviewOverlay.cpp
    src/ui/editors/CaptureConfirmDialog.cpp
    resources/app.qrc
)

if(WIN32)
    enable_language(RC)
    list(APPEND PIPBONG_SOURCES resources/PIPBONG.rc)
endif()

add_executable(PIPBONG WIN32 ${PIPBONG_SOURCES})

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/PipbongVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/PipbongVersion.h
    @ONLY)

target_include_directories(PIPBONG PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(PIPBONG PRIVATE
    Qt6::Widgets
    Qt6::Network
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    nlohmann_json::nlohmann_json
)

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "$ENV{Qt6_DIR}/../../../bin" "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        if(NOT PIPBONG_SKIP_WINDEPLOY)
            add_custom_command(TARGET PIPBONG POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:PIPBONG>"
                COMMENT "Deploying Qt runtime..."
            )
        endif()
        add_custom_target(deploy-qt
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" "$<TARGET_FILE:PIPBONG>"
            DEPENDS PIPBONG
            COMMENT "Deploying Qt runtime DLLs next to PIPBONG.exe"
        )
    endif()
endif()

if(WIN32)
    target_link_libraries(PIPBONG PRIVATE user32 gdi32 dwmapi dxgi d3d11 winmm shell32)
endif()

target_compile_definitions(PIPBONG PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

if(WIN32)
    add_executable(PIPBONGUpdater WIN32 src/updater/main.cpp)
    target_link_libraries(PIPBONGUpdater PRIVATE shell32)
    target_compile_definitions(PIPBONGUpdater PRIVATE
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )

    add_custom_command(TARGET PIPBONG POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:PIPBONGUpdater>"
                "$<TARGET_FILE_DIR:PIPBONG>/PIPBONGUpdater.exe"
        COMMENT "Copying PIPBONGUpdater next to main executable"
    )
endif()
