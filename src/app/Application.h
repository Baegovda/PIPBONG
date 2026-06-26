#pragma once

#include <QApplication>
#include <QString>

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);

    static Application* instance();
    static void ensureDpiAwareness();
    static QString dataDirectory();
    static QString autoSaveFilePath();
    QString projectDirectory() const;
    void setProjectDirectory(const QString& path);

private:
    QString m_projectDirectory;
};
