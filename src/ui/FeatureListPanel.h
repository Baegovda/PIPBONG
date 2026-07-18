#pragma once

#include <QHash>
#include <QSet>
#include <QWidget>

#include <memory>
#include <vector>

class QListWidgetItem;
class QMimeData;
class QPushButton;
class QListWidget;
class QToolButton;
class QSettings;
class QTimer;
class QSplitter;
class Project;
class Feature;
class FeatureListWidget;
class FeatureLibraryListWidget;
class FeatureHotkeyGateScope;
class ListColumnHeaderWidget;



struct FeatureListColumnLayout {

    int enableColumnWidth = 26;

    int runColumnWidth = 24;

    int modeColumnWidth = 34;

    int hotkeyColumnWidth = 50;

    int rowHeight = 26;

};



enum class FeatureRunVisualKind {
    ActiveRun,
    TriggerWatch,
    TriggerCooldown
};

struct FeatureTriggerCooldownState {
    qint64 endsAtEpochMs = 0;
    int totalMs = 0;
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



    void setProject(Project* project, bool refreshList = true);

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

    void setActiveWorkflowFeatureIds(const QSet<QString>& featureIds);

    void setFeatureRunVisualKinds(const QHash<QString, FeatureRunVisualKind>& kinds);

    void setTriggerCooldownStates(const QHash<QString, FeatureTriggerCooldownState>& states);

    FeatureRunVisualKind featureRunVisualKind(const QString& featureId) const;

    qint64 triggerCooldownRemainingMs(const QString& featureId) const;

    int triggerCooldownTotalMs(const QString& featureId) const;

    const Feature* projectFeatureById(const QString& featureId) const;



    const FeatureListColumnLayout& columnLayout() const { return m_columnLayout; }

    void setColumnLayout(const FeatureListColumnLayout& layout, bool persist = true);



    void saveColumnLayout(QSettings& settings, const QString& settingsKey) const;

    void restoreColumnLayout(const QSettings& settings, const QString& settingsKey);

    static void wireListColumnHeader(ListColumnHeaderWidget* header, FeatureListPanel* panel);

    QSplitter* featureLibrarySplitter() const { return m_featureLibrarySplitter; }

    void clampFeatureLibrarySplitterSizes();

    /// Replaces the library drawer contents with the given entries.
    void setLibraryEntries(const std::vector<LibraryEntryUi>& entries);



signals:

    void selectionChanged();

    void mutationAboutToCommit(const QString& reason);

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
    void importLibraryEntriesRequested(const QStringList& entryIds);
    /// Delete library entries from the drawer context menu or Delete key.
    void deleteLibraryEntriesRequested(const QStringList& entryIds);
    /// Feature or library entry dropped onto the feature list.
    void featureDropped(const QMimeData* mime, int insertIndex);
    /// Active-profile feature dropped onto the library drawer.
    void featureDroppedOnLibrary(const QMimeData* mime);
    /// Library drawer entry order changed by drag.
    void libraryEntriesReordered(int fromRow, int toRow);
    void libraryEntriesMultiReordered(const QList<int>& selectedRows, int insertIndex);
    /// Library drawer selection changed for workflow preview.
    void libraryEntrySelected(const QString& entryId);



protected:

    bool eventFilter(QObject* watched, QEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;



private slots:

    void onAddFeature();

    void onRemoveFeature();

    void onCopyFeature();

    void onPasteFeature();

    void onEditFeature();

    void onInlineRenameRequested();

    void onSelectionChanged();
    void onLibrarySelectionChanged();

    void onContextMenu(const QPoint& pos);

    void onAnimationTick();

    void onFeatureRowsReordered(int fromRow, int toRow);
    void onFeatureMultiRowsReordered(const QList<int>& selectedRows, int insertIndex);
    void onLibraryRowsReordered(int fromRow, int toRow);
    void onLibraryMultiRowsReordered(const QList<int>& selectedRows, int insertIndex);



private:

    void setupUi();

    void configureListItem(QListWidgetItem* item, const Feature& feature);
    QList<int> selectedRows() const;

    bool editFeatureAt(int index);

    bool isFeatureInActiveWorkflowRun(const QString& featureId) const;

    bool hasAnyActiveWorkflowRun() const;

    bool isFeatureEditableAt(int index) const;

    void refreshListMutationPolicy();

    void updateFeatureEditButtonState();

    void updateRemoveButtonState();

    void applyInlineRename(int row, const QString& name);

    void applyColumnLayoutToList();

    void updateReorderEnabled();

    void updateListItemEditableFlags();

    bool enableToggleHitTest(int row, const QPoint& viewportPos) const;

    bool runButtonHitTest(int row, const QPoint& viewportPos) const;

    void toggleFeatureEnabled(int row);

    void requestFeatureRun(int row);

    void setLibraryDrawerExpanded(bool expanded, bool persist);
    void updateLibraryToggleText();
    void onLibraryContextMenu(const QPoint& pos);
    void onRemoveLibraryEntries();
    QStringList selectedLibraryEntryIds() const;
    void applyLibraryDrawerVisibility(bool expanded);
    void ensureLibraryPaneSizeOnExpand();
    void updateFeatureLibrarySplitterHandle();

    Project* m_project = nullptr;

    FeatureListWidget* m_list = nullptr;

    ListColumnHeaderWidget* m_headerRow = nullptr;

    QPushButton* m_addButton = nullptr;

    QPushButton* m_removeButton = nullptr;

    QPushButton* m_editButton = nullptr;

    QTimer* m_animTimer = nullptr;

    QSet<QString> m_runningFeatureIds;

    QSet<QString> m_activeWorkflowFeatureIds;

    QHash<QString, FeatureRunVisualKind> m_featureRunVisualKinds;

    QHash<QString, FeatureTriggerCooldownState> m_triggerCooldownStates;

    QString m_lastSelectedFeatureId;

    FeatureListColumnLayout m_columnLayout;
    FeatureListColumnLayout m_headerDragStartLayout;

    int m_animPhase = 0;

    int m_inlineRenameRow = -1;

    std::unique_ptr<FeatureHotkeyGateScope> m_renameHotkeyGate;

    bool m_restoringColumnLayout = false;

    QToolButton* m_libraryToggle = nullptr;

    QWidget* m_libraryPane = nullptr;

    QSplitter* m_featureLibrarySplitter = nullptr;

    QWidget* m_libraryDrawerHost = nullptr;

    FeatureLibraryListWidget* m_libraryList = nullptr;

    int m_libraryEntryCount = 0;

    bool m_libraryExpanded = false;

    std::vector<std::unique_ptr<Feature>> m_clipboardFeatures;
};


