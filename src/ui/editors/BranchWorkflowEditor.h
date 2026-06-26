#pragma once

#include "core/workflow/Block.h"
#include "core/workflow/Workflow.h"

#include <QString>
#include <QWidget>

class QListWidget;
class QPushButton;

class BranchWorkflowEditor : public QWidget {
    Q_OBJECT
public:
    BranchWorkflowEditor(Workflow* workflow, const QString& projectDirectory, const QString& title,
                         QWidget* parent = nullptr);

    void reload();
    void setWorkflow(Workflow* workflow);

signals:
    void changed();

private:
    void setupUi(const QString& title);
    void refreshList();
    int selectedRow() const;
    void addBlockOfType(BlockType type);
    void editSelectedBlock();
    void deleteSelectedBlock();
    void moveSelectedUp();
    void moveSelectedDown();

    Workflow* m_workflow = nullptr;
    QString m_projectDirectory;
    QListWidget* m_list = nullptr;
};
