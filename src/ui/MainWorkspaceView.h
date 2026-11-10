#pragma once
#include <QObject>
#include <QPointer>
class QMainWindow;
class FeatureListPanel;
class QWidget;
class ProfileListWidget;
class QPushButton;
class WorkflowEditorPanel;
class QSplitter;
class LogPanelWidget;
class QToolButton;
class QCheckBox;
class TargetWindowDetailPanel;
class CustomTitleBar;
class UiStateManager;

// Non-owning handles into the Qt widget tree constructed by this view.
struct MainWorkspaceWidgets {
    FeatureListPanel* m_featureList = nullptr;
    QWidget* m_profilePanel = nullptr;
    ProfileListWidget* m_profileList = nullptr;
    QPushButton* m_addProfileButton = nullptr;
    QPushButton* m_renameProfileButton = nullptr;
    QPushButton* m_deleteProfileButton = nullptr;
    WorkflowEditorPanel* m_workflowEditor = nullptr;
    QSplitter* m_mainHorizontalSplitter = nullptr;
    QSplitter* m_mainVerticalSplitter = nullptr;
    QSplitter* m_bottomHorizontalSplitter = nullptr;
    LogPanelWidget* m_logPanel = nullptr;
    QToolButton* m_pickWindowButton = nullptr;
    QToolButton* m_pickWindowListButton = nullptr;
    QToolButton* m_pickSubWindowButton = nullptr;
    QToolButton* m_pickSubWindowListButton = nullptr;
    QToolButton* m_showSubTargetWindowButton = nullptr;
    QToolButton* m_showTargetWindowButton = nullptr;
    QToolButton* m_pinTargetWindowCenterButton = nullptr;
    QToolButton* m_pinSubTargetWindowCenterButton = nullptr;
    QCheckBox* m_alwaysOnTopCheck = nullptr;
    QPushButton* m_exitButton = nullptr;
    QPushButton* m_updateButton = nullptr;
    QPushButton* m_calculatorButton = nullptr;
    QPushButton* m_spikeWatchButton = nullptr;
    QPushButton* m_memoButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    TargetWindowDetailPanel* m_targetWindowDetailPanel = nullptr;
    CustomTitleBar* m_titleBar = nullptr;
};
// Widget assembly, pane minimums and layout persistence; no application model,
// hotkey/engine/profile transitions or MainWindow dependency.
class MainWorkspaceView : public QObject, private MainWorkspaceWidgets {
public:
    explicit MainWorkspaceView(QMainWindow&);
    ~MainWorkspaceView() override;
    const MainWorkspaceWidgets& widgets()const{return *this;}
    UiStateManager* restoreState();
    UiStateManager* state()const{return m_uiState;}
    void clampVertical();
private:
    void build();
    QPointer<QMainWindow> m_window;
    UiStateManager* m_uiState=nullptr;
};
