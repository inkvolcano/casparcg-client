#pragma once

#include "Shared.h"
#include "ui_PreviewWidget.h"

#include "PreviewContentWidget.h"

#include "Events/Inspector/TargetChangedEvent.h"
#include "Events/Library/LibraryItemSelectedEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtGui/QResizeEvent>
#include <QtMultimedia/QMediaPlayer>

#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class QVideoSink;
class PreviewAudioAnalyser;

class WIDGETS_EXPORT PreviewWidget : public QWidget, Ui::PreviewWidget
{
    Q_OBJECT

    public:
        explicit PreviewWidget(QWidget* parent = 0);

        // Everything below is off when this is on, and the panel behaves exactly
        // as it did before any of it existed: a thumbnail for stills, the local
        // file for movies, nothing for anything else.
        static bool legacyMode();

        // Whether a movie starts playing as soon as it is selected.
        static bool autoPlayVideo();

        // Whether the meters are drawn over the picture.
        static bool showAudioMeters();

        // Whether templates are rendered. Needs Qt WebEngine at build time; the
        // panel says so plainly when it was built without it.
        static bool showTemplates();

        // True when this build can render a template at all.
        static bool templateRenderingAvailable();

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        bool collapsed = false;
        QImage image;
        LibraryModel* model = nullptr;

        // A rundown item's template name lives on its command, not on the library
        // model: the operator can retype it in the Inspector, and then the two
        // disagree. The command's answer is the one the server would act on, so it
        // is the one previewed. Empty for a library selection, which has no
        // command and whose model name is correct.
        QString selectedTemplateName;

        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;

        // Content widget (replaces labelPreview).
        PreviewContentWidget* contentWidget = nullptr;

        // The template renderer. Held as a plain QWidget so this header never
        // needs a WebEngine include, and a build without WebEngine compiles
        // unchanged — it simply stays null.
        QWidget* templateView = nullptr;

        // Video playback.
        QMediaPlayer* player = nullptr;
        QVideoSink* videoSink = nullptr;

        // Transport bar (visible when video loaded).
        QWidget* transportBar = nullptr;
        QToolButton* playPauseButton = nullptr;
        QToolButton* stopButton = nullptr;
        QSlider* seekSlider = nullptr;
        QLabel* timeLabel = nullptr;
        bool sliderDragging = false;

        // Template transport: the four things a CasparCG template understands.
        QWidget* templateBar = nullptr;
        QToolButton* templatePlayButton = nullptr;
        QToolButton* templateNextButton = nullptr;
        QToolButton* templateUpdateButton = nullptr;
        QToolButton* templateStopButton = nullptr;

        // Meters are driven off a timer rather than off positionChanged, which
        // only fires a few times a second and would make them stutter.
        PreviewAudioAnalyser* audioAnalyser = nullptr;
        QTimer* meterTimer = nullptr;
        QVector<double> meterLevels;

        void setupMenus();
        void setupTransportBars();
        void setThumbnail();
        void updateThumbnailDisplay();
        QString resolveMediaFile(const QString& deviceName, const QString& mediaName);
        QString resolveImageFile(const QString& deviceName, const QString& mediaName);
        QString resolveTemplateFile(const QString& deviceName, const QString& templateName) const;
        bool loadImage(const QString& filePath);
        void loadVideo(const QString& filePath);
        void stopVideo();
        void loadTemplate(const QString& filePath);
        void clearTemplate();
        void runTemplateScript(const QString& script);
        QString templateNameForSelection() const;
        QString templateDataJson() const;
        QString formatTime(qint64 ms);

        Q_SLOT void toggleExpandCollapse();
        Q_SLOT void targetChanged(const TargetChangedEvent&);
        Q_SLOT void libraryItemSelected(const LibraryItemSelectedEvent&);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void playPause();
        Q_SLOT void stopPlayback();
        Q_SLOT void positionChanged(qint64 position);
        Q_SLOT void durationChanged(qint64 duration);
        Q_SLOT void sliderPressed();
        Q_SLOT void sliderReleased();
        Q_SLOT void sliderMoved(int value);
        Q_SLOT void updateMeters();
        Q_SLOT void templatePlay();
        Q_SLOT void templateNext();
        Q_SLOT void templateUpdate();
        Q_SLOT void templateStop();
};
