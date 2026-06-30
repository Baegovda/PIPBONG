#pragma once

#include <QSet>
#include <QWidget>

#include <memory>

class QListWidgetItem;
class QPushButton;
class QSettings;
class QTimer;
class Project;
class Feature;
class FeatureListWidget;



struct FeatureListColumnLayout {

    int modeColumnWidth = 34;

    int hotkeyColumnWidth = 50;

    int rowHeight = 26;

};



class FeatureListPanel : public QWidget {

    Q_OBJECT

public:

    explicit FeatureListPanel(QWidget* parent = nullptr);
    ~FeatureListPanel() override;



    void setProject(Project* project);

    void refresh();

    Feature* selectedFeature() const;

    int selectedIndex() const;

    QString selectedFeatureId() const;

    void selectFeatureById(const QString& featureId);



    QString runningFeatureId() const;

    bool isFeatureRunning(const QString& featureId) const;

    int animationPhase() const { return m_animPhase; }

    void setRunningFeatureIds(const QSet<QString>& featureIds);

    void setEditControlsEnabled(bool enabled);

    const Feature* projectFeatureById(const QString& featureId) const;



    const FeatureListColumnLayout& columnLayout() const { return m_columnLayout; }

    void setColumnLayout(const FeatureListColumnLayout& layout, bool persist = true);



    void saveColumnLayout(QSettings& settings, const QString& settingsKey) const;

    void restoreColumnLayout(const QSettings& settings, const QString& settingsKey);



signals:

    void selectionChanged();

    void projectModified();

    void hotkeysChanged();

    void columnLayoutChanged();

    void featureRunRequested(const QString& featureId);



protected:

    bool eventFilter(QObject* watched, QEvent* event) override;



private slots:

    void onAddFeature();

    void onRemoveFeature();

    void onCopyFeature();

    void onPasteFeature();

    void onEditFeature();

    void onSelectionChanged();

    void onContextMenu(const QPoint& pos);

    void onAnimationTick();

    void onFeatureRowsReordered(int fromRow, int toRow);



private:

    void setupUi();

    void configureListItem(QListWidgetItem* item, const Feature& feature);

    bool editFeatureAt(int index);

    void applyColumnLayoutToList();

    void updateReorderEnabled();

    bool runButtonHitTest(int row, const QPoint& viewportPos) const;

    void requestFeatureRun(int row);



    Project* m_project = nullptr;

    FeatureListWidget* m_list = nullptr;

    QWidget* m_headerRow = nullptr;

    QPushButton* m_addButton = nullptr;

    QPushButton* m_removeButton = nullptr;

    QPushButton* m_editButton = nullptr;

    QTimer* m_animTimer = nullptr;

    QSet<QString> m_runningFeatureIds;

    QString m_lastSelectedFeatureId;

    FeatureListColumnLayout m_columnLayout;

    int m_animPhase = 0;

    bool m_editControlsEnabled = true;

    bool m_restoringColumnLayout = false;

    std::unique_ptr<Feature> m_clipboardFeature;
};


