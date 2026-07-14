#pragma once

class QWidget;

/// In-app help and about dialogs for the main window.
class AppHelpDialog {
public:
    static void showHelp(QWidget* parent);
    static void showAbout(QWidget* parent);
};
