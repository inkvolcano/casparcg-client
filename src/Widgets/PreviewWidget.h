#pragma once

#include "Shared.h"
#include "ui_PreviewWidget.h"

#include "PreviewContentWidget.h"

#include "Events/Inspector/TargetChangedEvent.h"
#include "Events/Library/LibraryItemSelectedEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QString>
#include <QtGui/QImage>
#include <QtGui/QResizeEvent>
#include <QtMultimedia/QMediaPlayer>

#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QSlider>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class QVideoSink;

class WIDGETS_EXPORT PreviewWidget : public QWidget, Ui::PreviewWidget
{
    Q_OBJECT

    public:
        explicit PreviewWidget(QWidget* parent = 0);

    protected:
        void resizeEvent(QResizeEvent* event) override;

    private:
        bool collapsed = false;
        QImage image;
        LibraryModel* model;

        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;

        // Content widget (replaces labelPreview).
        PreviewContentWidget* contentWidget = nullptr;

        // Video playback.
        QMediaPlayer* player = nullptr;
        QVideoSink* videoSink = nullptr;

        // Transport bar (visible when video loaded).
        QWidget* transportBar = nullptr;
        QToolButton* playPauseButton = nullptr;
        QSlider* seekSlider = nullptr;
        QLabel* timeLabel = nullptr;
        bool sliderDragging = false;

        void setupMenus();
        void setThumbnail();
        void updateThumbnailDisplay();
        QString resolveMediaFile(const QString& deviceName, const QString& mediaName);
        void loadVideo(const QString& filePath);
        void stopVideo();
        QString formatTime(qint64 ms);

        Q_SLOT void toggleExpandCollapse();
        Q_SLOT void targetChanged(const TargetChangedEvent&);
        Q_SLOT void libraryItemSelected(const LibraryItemSelectedEvent&);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void playPause();
        Q_SLOT void positionChanged(qint64 position);
        Q_SLOT void durationChanged(qint64 duration);
        Q_SLOT void sliderPressed();
        Q_SLOT void sliderReleased();
        Q_SLOT void sliderMoved(int value);
};
