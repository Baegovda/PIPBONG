#include "app/ProjectRecoveryController.h"
#include "storage/ProjectHistoryStore.h"
#include "storage/JsonSerializer.h"
#include "model/Project.h"
#include <QDateTime>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QObject>

bool ProjectRecoveryController::restore(QWidget* parent, const QString& projectPath,
                                       const QString& projectDirectory,
                                       const std::function<bool()>& prepareCurrent) {
    const QStringList snapshots = ProjectHistoryStore::snapshots(projectPath);
    QStringList labels;
    QStringList validPaths;
    for (const QString& path : snapshots) {
        auto project = JsonSerializer::loadFromFile(path);
        if (!project) continue;
        labels.append(QObject::tr("%1 · 기능 %2개 · 저장본 %3")
            .arg(QFileInfo(path).lastModified().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
            .arg(project->features().size()).arg(labels.size() + 1));
        validPaths.append(path);
    }
    if (validPaths.isEmpty()) {
        QMessageBox::information(parent, QObject::tr("저장본 복구"),
            QObject::tr("아직 복구할 저장본이 없어요. 내용을 변경하고 저장하면 이전 정상 저장본을 자동으로 보관해요."));
        return false;
    }
    bool accepted = false;
    const QString choice = QInputDialog::getItem(parent, QObject::tr("저장본 복구"),
        QObject::tr("복구할 저장본을 선택하세요. 현재 내용도 복구 전에 보관돼요."), labels, 0, false, &accepted);
    if (!accepted) return false;
    auto recovered = JsonSerializer::loadFromFile(validPaths.at(labels.indexOf(choice)));
    if (!recovered || !prepareCurrent()) return false;
    if (!JsonSerializer::saveToFile(*recovered, projectPath, projectDirectory)) {
        QMessageBox::warning(parent, QObject::tr("저장본 복구"), QObject::tr("저장본을 복구하지 못했어요. 기존 파일은 유지돼요."));
        return false;
    }
    return true;
}
