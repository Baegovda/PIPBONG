#pragma once
#include <QObject>
#include <QPointer>
#include <QList>
#include <functional>
class QWidget;
class QDialog;
class QPushButton;
class CalculatorDialog;
class SpikeWatchDialog;
class MemoDialog;
class ProfileManager;

// Owns auxiliary dialog lifetimes and memo/profile binding on the GUI thread.
class AuxiliaryToolsController : public QObject {
    friend class AuxiliaryToolsUiChecks;
    friend class WorkspaceHistoryUiChecks;
public:
    struct Host {
        std::function<bool()> featureRunning;
        std::function<void(const QString&)> warning;
    };
    AuxiliaryToolsController(ProfileManager&,QWidget& owner,Host);
    ~AuxiliaryToolsController() override;
    void setButtons(QPushButton* calculator,QPushButton* spike,QPushButton* memo);
    void toggleCalculator();
    void toggleSpikeWatch();
    void toggleMemo();
    bool syncMemoProfile();
    bool saveMemo();
    void hideForTray();
    void closeForShutdown();
    bool calculatorVisible() const;
    bool spikeWatchVisible() const;
    bool memoVisible() const;
    MemoDialog* memo() const;
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    enum class Kind { Calculator,Spike,Memo };
    void wire(QDialog*,Kind);
    void updateButtons();
    MemoDialog* ensureMemo();
    static void present(QDialog*);
    ProfileManager& m_profiles;
    QPointer<QWidget> m_owner;
    Host m_host;
    QPointer<CalculatorDialog> m_calculator;
    QPointer<SpikeWatchDialog> m_spike;
    QPointer<MemoDialog> m_memo;
    QPointer<QPushButton> m_calculatorButton,m_spikeButton,m_memoButton;
    QList<QPointer<QDialog>> m_owned;
};
