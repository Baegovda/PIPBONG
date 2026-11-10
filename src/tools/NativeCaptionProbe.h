#pragma once
#include <QJsonObject>
class MainWindow;
class NativeCaptionProbe {
public:
    // Own-window read-only evaluation, called only by the isolated smoke capture.
    static QJsonObject inspect(MainWindow&);
};
