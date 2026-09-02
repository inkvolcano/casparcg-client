#pragma once

#include "Shared.h"
#include "ui_SettingsDialog.h"

#include "Global.h"
#include "Playout.h"

#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QKeySequenceEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

class LayoutEditorWidget;

class WIDGETS_EXPORT SettingsDialog : public QDialog, Ui::SettingsDialog
{
    Q_OBJECT

    public:
        explicit SettingsDialog(QWidget* parent = 0);

        Q_SIGNAL void gpiBindingChanged(int, Playout::PlayoutType);
        Q_SIGNAL void hotkeyChanged();

    private:
        QString stylesheet;

        // Debounce timers for expensive operations.
        QTimer* fontSizeDebounceTimer = nullptr;
        int pendingFontSize = 0;
        QTimer* sliderDbWriteTimer = nullptr;
        QMap<QString, QString> pendingSliderDbWrites;
        void flushPendingWrites();

        // General tab widgets (moved from .ui to programmatic).
        QCheckBox* checkBoxFullscreen;
        QComboBox* comboBoxTheme;
        QSpinBox* spinBoxFontSize;
        QCheckBox* checkBoxUseDropFrameNotation;
        QCheckBox* checkBoxAutoRefresh;
        QLabel* labelInterval;
        QSpinBox* spinBoxRefreshInterval;
        QLabel* labelSeconds;
        QCheckBox* checkBoxShowThumbnailTooltip;
        QCheckBox* checkBoxReverseOscTime;
        QCheckBox* checkBoxDisableInAndOutPoints;
        QSpinBox* spinBoxOscRefreshRate;
        QSpinBox* spinBoxUndoHistoryLimit;
        QCheckBox* checkBoxMarkUsedItems;
        QLineEdit* lineEditRundownRepository;
        QLineEdit* lineEditRepositoryPort;
        QCheckBox* checkBoxUseFreezeOnLoad;
        QComboBox* comboBoxDurationFormat;
        QCheckBox* checkBoxStoreThumbnailsInDatabase;
        QPushButton* pushButtonDeleteThumbnails;

        QMap<QString, QKeySequenceEdit*> hotkeyEdits;
        QComboBox* previewModifierCombo;
        QCheckBox* previewFreezeTemplateCheck;
        QCheckBox* checkBoxShowPreviewBorder;
        QCheckBox* checkBoxShowSTEPButton;
        QCheckBox* checkBoxShowPVWButton;
        QCheckBox* checkBoxShowServers;
        QCheckBox* checkBoxShowChannelLocks;
        QCheckBox* checkBoxShowChannelHeaders;
        QCheckBox* checkBoxShowBankIcons;
        QCheckBox* checkBoxHttpLogLastOnly;
        QCheckBox* checkBoxShowLastAction;
        QCheckBox* checkBoxActiveIndicatorPerChannel;
        QComboBox* comboBoxDisconnectMode;
        QCheckBox* checkBoxActivityGrow;
        QSpinBox* spinBoxNdiOutputs;
        QComboBox* comboBoxNdiBandwidth;
        QComboBox* comboBoxNdiFpsLimit;
        QComboBox* comboBoxNdiScaling;
        QCheckBox* checkBoxDualClock;
        QComboBox* comboBoxTimezone1;
        QComboBox* comboBoxTimezone2;
        LayoutEditorWidget* layoutEditor;
        LayoutEditorWidget* simpleLayoutEditor = nullptr;
        QSpinBox* spinBoxSimpleColumns = nullptr;
        QCheckBox* checkBoxSimplePlayStop = nullptr;
        QCheckBox* checkBoxSimplePreview = nullptr;

    // Sheets: where the cache lives, what the per-minute budget is, and where the
    // strain figures are published so something outside can total them up.
    QLineEdit* lineEditSheetsCacheUrl = nullptr;
    QSpinBox* spinBoxSheetsQuota = nullptr;
    QLineEdit* lineEditSheetsStrainUrl = nullptr;
    QCheckBox* checkBoxHostSheetCache = nullptr;
    QSpinBox* spinBoxSheetCachePort = nullptr;
    QLineEdit* lineEditSheetCacheDir = nullptr;
    QCheckBox* checkBoxSheetCacheBypass = nullptr;
    QPushButton* buttonClearSheetCache = nullptr;
    void refreshSheetCacheSize();
    QSpinBox* spinBoxWarmStale = nullptr;
    QSpinBox* spinBoxWarmSpacing = nullptr;

    // One checkbox per discovered template project, writing the `local` flag straight
    // into that project's project.js.
    QWidget* sheetProjectsBox = nullptr;
    void buildSheetProjectsGroup();

        // Panel sizing combos (Layout tab).
        struct PanelSizingEntry {
            QString panelId;
            QString label;
            QComboBox* combo;
        };
        QList<PanelSizingEntry> panelSizingEntries;
        QCheckBox* checkBoxShowEmptyPanels;

        // Channel color sliders + preview.
        QSlider* sliderAngle;
        QSlider* sliderOffset;
        QSlider* sliderSaturation;
        QSlider* sliderLightness;
        QLabel* labelAngleValue;
        QLabel* labelOffsetValue;
        QLabel* labelSaturationValue;
        QLabel* labelLightnessValue;
        QLabel* channelPreview[5];

        // Interface color swatches.
        QLabel* swatchPVW;
        QLabel* swatchSTEP;
        QLabel* swatchPreviewBorder;
        QLabel* swatchAutostepHighlight;
        QLabel* swatchActiveIndicator;
        QLabel* swatchLibrarySectionLine;

        // Header color swatches (Line / Block / Text per row).
        QLabel* swatchMasterLine;
        QLabel* swatchMasterBlock;
        QLabel* swatchMasterText;
        QLabel* swatchRundownLine;
        QLabel* swatchRundownBlock;
        QLabel* swatchRundownText;
        QCheckBox* checkBoxCustomizeHeaders;
        static const int HEADER_PANEL_COUNT = 11;
        QLabel* swatchLine[HEADER_PANEL_COUNT];
        QLabel* swatchBlock[HEADER_PANEL_COUNT];
        QLabel* swatchText[HEADER_PANEL_COUNT];
        QWidget* panelRowWidgets[HEADER_PANEL_COUNT]; // for show/hide per-widget rows

        // Clock color swatches + options.
        QLabel* swatchClock1;
        QLabel* swatchClock2;
        QLabel* swatchClockShadow;
        QCheckBox* checkBoxClockShowLabels;
        QCheckBox* checkBoxClockStacked;

        void setupGeneralTab();
        void updateChannelPreview();
        void openColorPicker(QLabel* swatch, const QString& dbKey, bool alpha);
        bool eventFilter(QObject* obj, QEvent* event) override;

        void loadGpi();
        void loadDevice();
        void loadOscOutput();
        void loadHotkeys();
        void setupHotkeyTab();
        void checkEmptyDeviceList();
        void checkEmptyOscOutputList();
        void updateGpi(int gpi, const QComboBox* voltage, const QComboBox* action);
        void updateGpo(int gpo, const QComboBox* voltage, const QSpinBox* pulseLength);
        void updateGpiDevice();
        void blockAllSignals(bool block);

        Q_SLOT void removeDevice();
        Q_SLOT void showAddDeviceDialog();
        Q_SLOT void startFullscreenChanged(int);
        Q_SLOT void fontSizeChanged(int);
        Q_SLOT void autoSynchronizeChanged(int);
        Q_SLOT void synchronizeIntervalChanged(int);
        Q_SLOT void showThumbnailTooltipChanged(int);
        Q_SLOT void enableOscInputControlChanged(int);
        Q_SLOT void enableOscInputMonitorChanged(int);
        Q_SLOT void enableOscInputWebSocketChanged(int);
        Q_SLOT void disableInAndOutPointsChanged(int);
        Q_SLOT void reverseOscTimeChanged(int);
        Q_SLOT void deviceItemDoubleClicked(QTreeWidgetItem*, int);
        Q_SLOT void gpi1Changed();
        Q_SLOT void gpi2Changed();
        Q_SLOT void gpi3Changed();
        Q_SLOT void gpi4Changed();
        Q_SLOT void gpi5Changed();
        Q_SLOT void gpi6Changed();
        Q_SLOT void gpi7Changed();
        Q_SLOT void gpi8Changed();
        Q_SLOT void gpo1Changed();
        Q_SLOT void gpo2Changed();
        Q_SLOT void gpo3Changed();
        Q_SLOT void gpo4Changed();
        Q_SLOT void gpo5Changed();
        Q_SLOT void gpo6Changed();
        Q_SLOT void gpo7Changed();
        Q_SLOT void gpo8Changed();
        Q_SLOT void serialPortChanged();
        Q_SLOT void streamPortChanged();
        Q_SLOT void baudRateChanged(QString);
        Q_SLOT void oscMonitorPortChanged();
        Q_SLOT void oscControlPortChanged();
        Q_SLOT void oscWebSocketPortChanged();
        Q_SLOT void repositoryPortChanged();
        Q_SLOT void showImportDeviceDialog();
        Q_SLOT void showAddOscOutputDialog();
        Q_SLOT void removeOscOutput();
        Q_SLOT void oscOutputItemDoubleClicked(QTreeWidgetItem*, int);
        Q_SLOT void durationFormatChanged(QString);
        Q_SLOT void logLevelChanged(int);
        Q_SLOT void themeChanged(QString);
        Q_SLOT void rundownRepositoryChanged();
        Q_SLOT void deleteThumbnails();
        Q_SLOT void storeThumbnailsInDatabaseChanged(int);
        Q_SLOT void markUsedItemsChanged(int);
        Q_SLOT void disableAudioInStreamChanged(int);
        Q_SLOT void networkCacheChanged(int);
        Q_SLOT void streamQualityChanged(int);
        Q_SLOT void useFreezeOnLoadChanged(int);
        Q_SLOT void useDropFrameNotationChanged(int);
        Q_SLOT void hotkeyEditChanged(const QKeySequence&);
        Q_SLOT void restoreDefaultHotkeysClicked();
};
