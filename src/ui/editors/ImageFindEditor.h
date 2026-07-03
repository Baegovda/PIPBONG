#pragma once



#include "core/workflow/blocks/ImageFindBlock.h"

#include "core/input/HotkeyBinding.h"
#include "ui/editors/RoiPreviewOverlay.h"



#include <QDialog>



class DragAdjustDoubleSpinBox;

class DragAdjustSpinBox;

class QLabel;

class QListWidget;

class QPushButton;

class QShowEvent;

class QHideEvent;

class QResizeEvent;

class QKeyEvent;

class QCheckBox;

class QComboBox;

class HintLabel;

class AnimatedTwoWaySwitch;



class ImageFindEditor : public QDialog {

    Q_OBJECT

public:

    explicit ImageFindEditor(ImageFindBlock* block, const QString& projectDirectory, QWidget* parent = nullptr,

                             bool embedded = false);

    ~ImageFindEditor() override;



    void setBlock(ImageFindBlock* block);

    void reload();

    void apply();

    void setRoiCorrectionUiPolicy(bool featureGlobalEnabled, bool sessionEligible);

    bool isCapturingTemplateHotkey() const { return m_capturingTemplateHotkey; }

    bool isTemplateCaptureHotkeyHookActive() const;

    void refreshTemplateCaptureHotkeyHook();



protected:

    void showEvent(QShowEvent* event) override;

    void hideEvent(QHideEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;

    bool eventFilter(QObject* watched, QEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;



private slots:

    void onCaptureTemplate();

    void onPickRoi();

    void onRoiPreview();

    void onMatchTest();

    void onRemoveTemplate();

    void onRemoveRoi();

    void updatePreview();

    void triggerTemplateCaptureFromHotkey();



private:

    void setupUi();

    void syncUiToBlockValues();

    QString selectedTemplateAbsolutePath() const;

    QString selectedTemplateRelativePath() const;

    void addTemplatePath(const QString& relativePath);

    void refreshTemplateList();

    void syncBlockTemplatePathsFromList();

    void refreshRoiList();

    void syncBlockCustomRegionsFromList();

    void updateRoiPreviewButton(bool overlayVisible);

    bool hasConfiguredRoiForPreview() const;

    void showRoiPreviewOverlay(bool silentErrors = false);

    void dismissRoiPreviewOverlay();

    void confirmFromRoiOverlay();

    void updatePickRoiButton(bool pickActive);

    void startRoiPick();

    RoiPreviewOverlay::EditableOptions makeRoiPreviewEditableOptions();

    void updateMatchTestButton(bool overlayVisible);

    void updateTemplateHotkeyDisplay();

    void updateTemplateHotkeyCaptureUi();

    void startTemplateHotkeyCapture();

    void stopTemplateHotkeyCapture();

    void applyTemplateHotkeyCapture(int virtualKey, Qt::KeyboardModifiers modifiers);

    void syncTemplateCaptureHotkeyHook();

    void updateRoiCorrectionUi();

    ImageFindBlock* m_block = nullptr;

    QString m_projectDirectory;

    bool m_embedded = false;

    bool m_featureRoiCorrectionGlobal = false;

    bool m_roiCorrectionSessionEligible = false;

    bool m_capturingTemplateHotkey = false;

    bool m_roiPickActive = false;

    DragAdjustDoubleSpinBox* m_thresholdSpin = nullptr;

    DragAdjustSpinBox* m_pollIntervalSpin = nullptr;

    QComboBox* m_templateColorModeCombo = nullptr;

    QCheckBox* m_roiCorrectionCheck = nullptr;

    QWidget* m_roiCorrectionExpandRow = nullptr;

    DragAdjustSpinBox* m_roiCorrectionExpandSpin = nullptr;

    QCheckBox* m_returnToPreviousImageFindCheck = nullptr;

    QCheckBox* m_retryAfterNextActionCheck = nullptr;

    HintLabel* m_roiCorrectionGlobalHint = nullptr;

    AnimatedTwoWaySwitch* m_matchModeSwitch = nullptr;

    QListWidget* m_templateList = nullptr;

    QPushButton* m_removeTemplateButton = nullptr;

    QPushButton* m_pickRoiButton = nullptr;

    QListWidget* m_roiList = nullptr;

    QPushButton* m_removeRoiButton = nullptr;

    QLabel* m_templatePreviewLabel = nullptr;

    QLabel* m_templateSizeLabel = nullptr;

    QPushButton* m_roiPreviewButton = nullptr;

    QPushButton* m_matchTestButton = nullptr;

    QLabel* m_templateCaptureHotkeyLabel = nullptr;

};


