#pragma once

#include <QObject>

#include <functional>

#include <QSettings>
#include <QVector>

class QHeaderView;
class QMainWindow;
class QSplitter;
class QTimer;

// Persists main-window geometry and registered splitter / table-header layouts via QSettings.
class UiStateManager : public QObject {
    Q_OBJECT
public:
    explicit UiStateManager(QObject* parent = nullptr);

    void registerMainWindow(QMainWindow* window, const QString& id = QStringLiteral("mainWindow"));
    void registerSplitter(QSplitter* splitter, const QString& id);
    void registerHeader(QHeaderView* header, const QString& id);
    void registerSettingsHooks(const QString& id,
                               const std::function<void(QSettings&)>& writer,
                               const std::function<void(const QSettings&)>& reader);

    void restoreAll();
    void restoreMainWindowGeometry();
    void saveNow();

    static QString settingsKey(const QString& id);

public slots:
    void scheduleSave();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct SettingsHookEntry {
        QString id;
        std::function<void(QSettings&)> writer;
        std::function<void(const QSettings&)> reader;
    };

    QTimer* m_saveTimer = nullptr;
    QMainWindow* m_mainWindow = nullptr;
    QString m_mainWindowId;
    QVector<QPair<QSplitter*, QString>> m_splitters;
    QVector<QPair<QHeaderView*, QString>> m_headers;
    QVector<SettingsHookEntry> m_settingsHooks;
    bool m_restoring = false;
    bool m_mainWindowGeometryRestored = false;
};
