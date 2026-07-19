#include "core/diagnostics/CrashReportSystemInfo.h"

#include <QApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QScreen>
#include <QStringList>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace CrashReportSystemInfo {

QString buildSystemSection() {
    QStringList lines;
    lines.append(QStringLiteral("--- system ---"));

#ifdef _WIN32
    OSVERSIONINFOEXW versionInfo{};
    versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
#pragma warning(push)
#pragma warning(disable : 4996)
    if (GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&versionInfo))) {
        lines.append(QStringLiteral("  OS: Windows %1.%2 build %3")
                         .arg(versionInfo.dwMajorVersion)
                         .arg(versionInfo.dwMinorVersion)
                         .arg(versionInfo.dwBuildNumber));
    }
#pragma warning(pop)
#endif

    if (QCoreApplication::instance()) {
        lines.append(QStringLiteral("  Qt: %1").arg(QString::fromLatin1(qVersion())));
        lines.append(QStringLiteral("  locale: %1").arg(QLocale::system().name()));
    }

    if (const QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        const QList<QScreen*> screens = guiApp->screens();
        lines.append(QStringLiteral("  screens: %1").arg(screens.size()));
        for (int index = 0; index < screens.size(); ++index) {
            const QScreen* screen = screens.at(index);
            const QRect geometry = screen->geometry();
            lines.append(QStringLiteral("    [%1] %2x%3 @%4,%5 dpi=%6 primary=%7")
                             .arg(index)
                             .arg(geometry.width())
                             .arg(geometry.height())
                             .arg(geometry.x())
                             .arg(geometry.y())
                             .arg(screen->devicePixelRatio(), 0, 'f', 2)
                             .arg(screen == guiApp->primaryScreen() ? QStringLiteral("yes")
                                                                    : QStringLiteral("no")));
        }
    }

    return lines.join(QLatin1Char('\n'));
}

} // namespace CrashReportSystemInfo
