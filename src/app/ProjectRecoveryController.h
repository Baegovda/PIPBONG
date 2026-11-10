#pragma once
#include <QString>
#include <functional>
class QWidget;

class ProjectRecoveryController {
public:
    static bool restore(QWidget* parent, const QString& projectPath,
                        const QString& projectDirectory,
                        const std::function<bool()>& prepareCurrent);
};
