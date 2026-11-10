#pragma once
#include <QString>
class QApplication;
class QWidget;

namespace UiAppearance {
void installPalette(QApplication& app);
QString styleSheet();
// Shared painted glass: no native backdrop sampling, blur or animation timers.
QString glassPanelStyle(const QString& selector);
QWidget* createWorkspaceSurface(QWidget* parent);
}
