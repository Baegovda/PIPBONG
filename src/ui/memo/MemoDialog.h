#pragma once

#include <QDialog>

class QTextEdit;
class QTimer;

class MemoDialog : public QDialog {
    Q_OBJECT
public:
    explicit MemoDialog(QWidget* parent = nullptr);

    /// Saves the current editor to the previous profile folder, then loads @p profileDirectory.
    void setProfile(const QString& profileId,
                    const QString& profileDirectory,
                    const QString& profileDisplayName);
    void saveNow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void scheduleSave();
    void persistGeometry();
    void restorePersistedGeometry();
    void updateWindowTitle();

    QString m_profileId;
    QString m_profileDirectory;
    QString m_profileDisplayName;
    QTextEdit* m_editor = nullptr;
    QTimer* m_saveTimer = nullptr;
    bool m_loading = false;
};
