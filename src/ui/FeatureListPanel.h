#pragma once

#include <QSet>
#include <QWidget>

#include <memory>

class QListWidgetItem;
class QMimeData;
class QPushButton;
class QListWidget;
class QToolButton;
class QSettings;
class QTimer;
class QVariantAnimation;
class Project;
class Feature;
class FeatureListWidget;
class FeatureLibraryListWidget;



struct FeatureListColumnLayout {

    int modeColumnWidth = 34;

    int hotkeyColumnWidth = 50;

    int rowHeight = 26;

};



class FeatureListPanel : public QWidget {

    Q_OBJECT

public:

    struct LibraryEntryUi {
        QString id;
        QString name;
        int templateCount = 0;
    };

    explicit FeatureListPanel(QWidget* parent = nullptr);
    ~FeatureListPanel() override;



    void setProject(Project* project);

    void setActiveProfileId(const QString& profileId);

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

    /// Replaces the library drawer contents with the given entries.
    void setLibraryEntries(const std::vector<LibraryEntryUi>& entries);



signals:

    void selectionChanged();

    void projectModified();

    void hotkeysChanged();

    void columnLayoutChanged();

    void featureRunRequested(const QString& featureId);

    void featureEnabledChanged(const QString& featureId, bool enabled);

    /// Save the selected feature into the global feature library.
    void saveFeatureToLibraryRequested(const QString& featureId);
    /// Open the library picker and import a feature into the current profile.
    void importFeatureFromLibraryRequested();
    /// Import a specific library entry (double-click / context menu on the drawer).
    void importLibraryEntryRequested(const QString& entryId);
    /// Delete a library entry from the drawer context menu.
    void deleteLibraryEntryRequested(const QString& entryId);
    /// Feature or library entry dropped onto the feature list.
    void featureDropped(const QMimeData* mime, int insertIndex);
    /// Active-profile feature dropped onto the library drawer.
    void featureDroppedOnLibrary(const QMimeData* mime);
    /// Library drawer entry order changed by drag.
    void libraryEntriesReordered(int fromRow, int toRow);



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
    void onLibraryRowsReordered(int fromRow, int toRow);



private:

    void setupUi();

    void configureListItem(QListWidgetItem* item, const Feature& feature);
    QList<int> selectedRows() const;

    bool editFeatureAt(int index);

    void applyColumnLayoutToList();

    void updateReorderEnabled();

    bool enableToggleHitTest(int row, const QPoint& viewportPos) const;

    bool runButtonHitTest(int row, const QPoint& viewportPos) const;

    void toggleFeatureEnabled(int row);

    void requestFeatureRun(int row);

    void setLibraryDrawerExpanded(bool expanded, bool persist);
    void updateLibraryToggleText();
    void onLibraryContextMenu(const QPoint& pos);
    int libraryDrawerContentHeight() const;
    void animateLibraryDrawerTo(bool expanded);
    void applyLibraryDrawerHeight(int height, bool expanded);
    void syncLibraryDrawerHeight(bool animate);



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

    QToolButton* m_libraryToggle = nullptr;

    QWidget* m_libraryDrawerHost = nullptr;

    FeatureLibraryListWidget* m_libraryList = nullptr;

    QVariantAnimation* m_libraryDrawerAnimation = nullptr;

    int m_libraryEntryCount = 0;

    bool m_libraryExpanded = false;

    std::unique_ptr<Feature> m_clipboardFeature;
};


