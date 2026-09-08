#include "PreviewWidget.h"

#include "Global.h"
#include "PanelHelper.h"
#include "PreviewAudioAnalyser.h"

#include "AudioLevelTrack.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Models/ConfigurationModel.h"
#include "Models/LibraryModel.h"
#include "Models/ThumbnailModel.h"
#include "Commands/TemplateCommand.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoSink>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QToolButton>

#ifdef CASPARCG_HAS_WEBENGINE
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineSettings>
#endif

namespace
{
    // Meters redrawn 25 times a second. Faster is wasted on a meter this size;
    // slower and the eye sees it stepping.
    const int METER_INTERVAL_MS = 40;

    // How fast a meter is allowed to fall, per redraw. Peaks are taken instantly.
    const double METER_FALL_DB = 1.6;

    bool configIsTrue(const QString& key, bool valueWhenUnset)
    {
        QString value = DatabaseManager::getInstance().getConfigurationByName(key).getValue();

        if (value.isEmpty())
            return valueWhenUnset;

        return value == "true";
    }
}

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);

    // Create content widget (replaces the old QLabel).
    this->contentWidget = new PreviewContentWidget(this);

    setupTransportBars();

    // Add content + transports to the tab's layout.
    this->verticalLayout->addWidget(this->contentWidget, 1);
    this->verticalLayout->addWidget(this->transportBar, 0);
    this->verticalLayout->addWidget(this->templateBar, 0);

    // Set up video player. An audio output is attached so a previewed clip can
    // actually be heard — without one QMediaPlayer decodes video only.
    this->player = new QMediaPlayer(this);
    this->player->setAudioOutput(new QAudioOutput(this));
    this->videoSink = new QVideoSink(this);
    this->player->setVideoSink(this->videoSink);

    QObject::connect(this->player, &QMediaPlayer::positionChanged, this, &PreviewWidget::positionChanged);
    QObject::connect(this->player, &QMediaPlayer::durationChanged, this, &PreviewWidget::durationChanged);

    QObject::connect(this->videoSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
        QVideoFrame f = frame;
        if (f.map(QVideoFrame::ReadOnly))
        {
            this->contentWidget->setImage(f.toImage());
            f.unmap();
        }
    });

    // Levels are decoded from the file itself, so the meters need no server and
    // no audio device — they work on a machine with neither.
    this->audioAnalyser = new PreviewAudioAnalyser(this);
    QObject::connect(this->audioAnalyser, &PreviewAudioAnalyser::noAudio, this, [this]() {
        this->meterLevels.clear();
        this->contentWidget->clearAudioLevels();
    });

    this->meterTimer = new QTimer(this);
    this->meterTimer->setInterval(METER_INTERVAL_MS);
    QObject::connect(this->meterTimer, SIGNAL(timeout()), this, SLOT(updateMeters()));

    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("Preview");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_PREVIEW_HEIGHT);

    QTimer::singleShot(0, this, [this]() {
        PanelHelper::applyExpandedHeight(this, "Preview", Panel::DEFAULT_PREVIEW_HEIGHT);
    });

    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent&)), this, SLOT(libraryItemSelected(const LibraryItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(targetChanged(const TargetChangedEvent&)), this, SLOT(targetChanged(const TargetChangedEvent&)));
}

bool PreviewWidget::legacyMode()
{
    return configIsTrue("PreviewLegacyMode", false);
}

bool PreviewWidget::autoPlayVideo()
{
    return configIsTrue("PreviewAutoPlayVideo", false);
}

bool PreviewWidget::showAudioMeters()
{
    return configIsTrue("PreviewAudioMeters", true);
}

bool PreviewWidget::showTemplates()
{
    return configIsTrue("PreviewTemplates", true);
}

bool PreviewWidget::templateRenderingAvailable()
{
#ifdef CASPARCG_HAS_WEBENGINE
    return true;
#else
    return false;
#endif
}

void PreviewWidget::setupTransportBars()
{
    // ---- video transport ----
    this->transportBar = new QWidget(this);
    this->transportBar->setFixedHeight(22);
    this->transportBar->setVisible(false);

    this->playPauseButton = new QToolButton(this->transportBar);
    this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // play
    this->playPauseButton->setFixedSize(22, 22);
    this->playPauseButton->setFocusPolicy(Qt::NoFocus);
    this->playPauseButton->setToolTip("Play / pause");
    QObject::connect(this->playPauseButton, &QToolButton::clicked, this, &PreviewWidget::playPause);

    this->stopButton = new QToolButton(this->transportBar);
    this->stopButton->setText(QString::fromUtf8("\xe2\x96\xa0")); // stop
    this->stopButton->setFixedSize(22, 22);
    this->stopButton->setFocusPolicy(Qt::NoFocus);
    this->stopButton->setToolTip("Stop and rewind");
    QObject::connect(this->stopButton, &QToolButton::clicked, this, &PreviewWidget::stopPlayback);

    this->seekSlider = new QSlider(Qt::Horizontal, this->transportBar);
    this->seekSlider->setRange(0, 0);
    this->seekSlider->setFocusPolicy(Qt::NoFocus);
    QObject::connect(this->seekSlider, &QSlider::sliderPressed, this, &PreviewWidget::sliderPressed);
    QObject::connect(this->seekSlider, &QSlider::sliderReleased, this, &PreviewWidget::sliderReleased);
    QObject::connect(this->seekSlider, &QSlider::sliderMoved, this, &PreviewWidget::sliderMoved);

    this->timeLabel = new QLabel("0:00 / 0:00", this->transportBar);
    this->timeLabel->setFixedWidth(90);
    this->timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont timeFont = this->timeLabel->font();
    timeFont.setPixelSize(10);
    this->timeLabel->setFont(timeFont);

    QHBoxLayout* transportLayout = new QHBoxLayout(this->transportBar);
    transportLayout->setContentsMargins(2, 0, 2, 0);
    transportLayout->setSpacing(4);
    transportLayout->addWidget(this->playPauseButton);
    transportLayout->addWidget(this->stopButton);
    transportLayout->addWidget(this->seekSlider, 1);
    transportLayout->addWidget(this->timeLabel);

    // ---- template transport ----
    // The four calls a CasparCG template understands, so the preview drives it
    // the same way the server would.
    this->templateBar = new QWidget(this);
    this->templateBar->setFixedHeight(22);
    this->templateBar->setVisible(false);

    auto makeButton = [this](const QString& text, const QString& tip) {
        QToolButton* button = new QToolButton(this->templateBar);
        button->setText(text);
        button->setFixedHeight(22);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(tip);
        return button;
    };

    this->templatePlayButton = makeButton("Play", "Call play() on the template");
    this->templateNextButton = makeButton("Next", "Call next() on the template");
    this->templateUpdateButton = makeButton("Update", "Call update() with the template's own debugData");
    this->templateStopButton = makeButton("Stop", "Call stop() on the template");

    QObject::connect(this->templatePlayButton, &QToolButton::clicked, this, &PreviewWidget::templatePlay);
    QObject::connect(this->templateNextButton, &QToolButton::clicked, this, &PreviewWidget::templateNext);
    QObject::connect(this->templateUpdateButton, &QToolButton::clicked, this, &PreviewWidget::templateUpdate);
    QObject::connect(this->templateStopButton, &QToolButton::clicked, this, &PreviewWidget::templateStop);

    QHBoxLayout* templateLayout = new QHBoxLayout(this->templateBar);
    templateLayout->setContentsMargins(2, 0, 2, 0);
    templateLayout->setSpacing(4);
    templateLayout->addWidget(this->templatePlayButton);
    templateLayout->addWidget(this->templateNextButton);
    templateLayout->addWidget(this->templateUpdateButton);
    templateLayout->addWidget(this->templateStopButton);
    templateLayout->addStretch(1);
}

void PreviewWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Preview", this);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &PreviewWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetPreview);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetPreview->setCornerWidget(this->menuButton);
}

void PreviewWidget::targetChanged(const TargetChangedEvent& event)
{
    if (this->model == nullptr)
        return;

    this->model->setName(event.getTarget());

    setThumbnail();
}

void PreviewWidget::libraryItemSelected(const LibraryItemSelectedEvent& event)
{
    this->model = event.getLibraryModel();
    this->selectedTemplateName.clear();

    setThumbnail();
}

void PreviewWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->model = event.getLibraryModel();

    // Prefer what the command says, so previewing follows the name the operator
    // actually typed rather than the one the item was dragged in with.
    TemplateCommand* templateCommand = dynamic_cast<TemplateCommand*>(event.getCommand());
    this->selectedTemplateName = (templateCommand != nullptr) ? templateCommand->getTemplateName() : QString();

    setThumbnail();
}

QString PreviewWidget::templateNameForSelection() const
{
    if (!this->selectedTemplateName.trimmed().isEmpty())
        return this->selectedTemplateName.trimmed();

    return this->model != nullptr ? this->model->getName() : QString();
}

void PreviewWidget::setThumbnail()
{
    // Whatever was showing stops first, so selecting a still never leaves the
    // previous clip's audio running underneath it.
    stopVideo();
    clearTemplate();
    this->contentWidget->setPlaceholder(QString());

    if (this->model == nullptr)
        return;

    const QString type = this->model->getType();
    const QString name = this->model->getName();
    const QString deviceName = this->model->getDeviceName();

    const bool legacy = legacyMode();

    // Legacy: stills and movies only, thumbnail from the database, nothing else.
    if (legacy && type != Rundown::STILL && type != Rundown::MOVIE)
    {
        this->image = QImage();
        this->contentWidget->clearContent();
        return;
    }

    if (!legacy && type == Rundown::TEMPLATE)
    {
        if (!showTemplates())
        {
            this->image = QImage();
            this->contentWidget->clearContent();
            return;
        }

        const QString templateName = templateNameForSelection();
        QString filePath = resolveTemplateFile(deviceName, templateName);

        if (filePath.isEmpty())
        {
            this->image = QImage();
            this->contentWidget->clearContent();
            this->contentWidget->setPlaceholder(
                templateName.isEmpty()
                    ? QString("This item has no template name yet.")
                    : QString("Template \"%1\" was not found.\n\nSet this server's Template path in Settings \xe2\x86\x92 Servers "
                              "to the folder holding the .html files.").arg(templateName));
            return;
        }

        loadTemplate(filePath);
        return;
    }

    if (type != Rundown::STILL && type != Rundown::MOVIE)
    {
        this->image = QImage();
        this->contentWidget->clearContent();
        return;
    }

    // Stills: the real file first. The database thumbnail is small, and it only
    // exists once a server has scanned the media — this panel is meant to work
    // with no server running at all.
    bool shown = false;
    if (!legacy && type == Rundown::STILL)
        shown = loadImage(resolveImageFile(deviceName, name));

    if (!shown)
    {
        QString data = DatabaseManager::getInstance().getThumbnailByNameAndDeviceName(name, deviceName).getData();

        if (!data.isEmpty())
        {
            this->image.loadFromData(QByteArray::fromBase64(data.toLatin1()), "PNG");
            updateThumbnailDisplay();
            shown = true;
        }
        else
        {
            this->image = QImage();
            this->contentWidget->clearContent();
        }
    }

    // For movies, try to load the local video file.
    if (type == Rundown::MOVIE)
    {
        QString filePath = resolveMediaFile(deviceName, name);
        if (!filePath.isEmpty())
        {
            loadVideo(filePath);
        }
        else if (!shown && !legacy)
        {
            this->contentWidget->setPlaceholder(
                QString("\"%1\" was not found on this machine.\n\nSet this server's Media path in Settings "
                        "\xe2\x86\x92 Servers to a folder this computer can reach.").arg(name));
        }
    }
}

void PreviewWidget::updateThumbnailDisplay()
{
    if (this->image.isNull())
        return;

    this->contentWidget->setImage(this->image);
}

QString PreviewWidget::resolveMediaFile(const QString& deviceName, const QString& mediaName)
{
    DeviceModel device = DatabaseManager::getInstance().getDeviceByName(deviceName);
    QString mediaPath = device.getMediaPath();

    if (mediaPath.isEmpty())
        return QString();

    // CasparCG media names use forward slashes for subdirs and no extension.
    QString baseName = mediaName;
    baseName.replace('\\', '/');

    QFileInfo baseInfo(mediaPath + "/" + baseName);
    QDir dir = baseInfo.dir();

    if (!dir.exists())
        return QString();

    // Search for the file with any common video extension.
    QStringList filters;
    QString nameOnly = baseInfo.fileName();
    for (const QString& ext : {".mov", ".mp4", ".mxf", ".avi", ".mkv", ".wmv", ".webm", ".mpg", ".mpeg", ".ts", ".m4v"})
        filters << (nameOnly + ext);

    QStringList matches = dir.entryList(filters, QDir::Files, QDir::Name);
    if (!matches.isEmpty())
        return dir.absoluteFilePath(matches.first());

    return QString();
}

QString PreviewWidget::resolveImageFile(const QString& deviceName, const QString& mediaName)
{
    DeviceModel device = DatabaseManager::getInstance().getDeviceByName(deviceName);
    QString mediaPath = device.getMediaPath();

    if (mediaPath.isEmpty())
        return QString();

    QString baseName = mediaName;
    baseName.replace('\\', '/');

    QFileInfo baseInfo(mediaPath + "/" + baseName);
    QDir dir = baseInfo.dir();

    if (!dir.exists())
        return QString();

    QStringList filters;
    QString nameOnly = baseInfo.fileName();
    for (const QString& ext : {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff", ".webp", ".tga"})
        filters << (nameOnly + ext);

    QStringList matches = dir.entryList(filters, QDir::Files, QDir::Name);
    if (!matches.isEmpty())
        return dir.absoluteFilePath(matches.first());

    return QString();
}

QString PreviewWidget::resolveTemplateFile(const QString& deviceName, const QString& templateName) const
{
    DeviceModel device = DatabaseManager::getInstance().getDeviceByName(deviceName);
    QString templatePath = device.getTemplatePath();

    if (templatePath.isEmpty() || templateName.isEmpty())
        return QString();

    QString baseName = templateName;
    baseName.replace('\\', '/');

    // Same rule the Inspector's function discovery uses, so the two agree about
    // which file a template name means.
    for (const QString& ext : {".html", ".htm"})
    {
        QString candidate = QDir(templatePath).filePath(baseName + ext);
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return QString();
}

bool PreviewWidget::loadImage(const QString& filePath)
{
    if (filePath.isEmpty())
        return false;

    QImage loaded;
    if (!loaded.load(filePath))
        return false;

    this->image = loaded;
    updateThumbnailDisplay();

    return true;
}

void PreviewWidget::loadVideo(const QString& filePath)
{
    this->player->setSource(QUrl::fromLocalFile(filePath));
    this->transportBar->setVisible(true);

    if (!legacyMode() && showAudioMeters())
    {
        this->audioAnalyser->analyse(filePath);
        this->meterTimer->start();
    }

    if (!legacyMode() && autoPlayVideo())
    {
        this->player->play();
        this->playPauseButton->setText(QString::fromUtf8("\xe2\x8f\xb8")); // pause
    }
    else
    {
        this->player->pause();
        this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // play
    }
}

void PreviewWidget::stopVideo()
{
    this->player->stop();
    this->player->setSource(QUrl());
    this->transportBar->setVisible(false);
    this->seekSlider->setRange(0, 0);
    this->timeLabel->setText("0:00 / 0:00");

    this->meterTimer->stop();
    this->audioAnalyser->stop();
    this->meterLevels.clear();
    this->contentWidget->clearAudioLevels();
}

void PreviewWidget::loadTemplate(const QString& filePath)
{
#ifdef CASPARCG_HAS_WEBENGINE
    if (this->templateView == nullptr)
    {
        QWebEngineView* view = new QWebEngineView(this);

        // Templates are keyed over black on air, and a white page behind one
        // would show every antialiased edge as a halo.
        view->page()->setBackgroundColor(Qt::black);
        view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);
        view->settings()->setAttribute(QWebEngineSettings::ShowScrollBars, false);

        this->templateView = view;
        this->verticalLayout->insertWidget(0, this->templateView, 1);
    }

    this->contentWidget->setVisible(false);
    this->templateView->setVisible(true);
    this->templateBar->setVisible(true);

    static_cast<QWebEngineView*>(this->templateView)->load(QUrl::fromLocalFile(filePath));
#else
    Q_UNUSED(filePath);

    this->image = QImage();
    this->contentWidget->clearContent();
    this->contentWidget->setPlaceholder(
        "Template preview needs Qt WebEngine, and this client was built without it.\n\n"
        "Everything else in this panel works as normal.");
#endif
}

void PreviewWidget::clearTemplate()
{
    this->templateBar->setVisible(false);

    if (this->templateView == nullptr)
    {
        this->contentWidget->setVisible(true);
        return;
    }

#ifdef CASPARCG_HAS_WEBENGINE
    // Loading a blank page rather than only hiding the view: a template left
    // loaded keeps running its timers and animations behind the panel.
    static_cast<QWebEngineView*>(this->templateView)->setHtml(QString());
#endif

    this->templateView->setVisible(false);
    this->contentWidget->setVisible(true);
}

void PreviewWidget::runTemplateScript(const QString& script)
{
#ifdef CASPARCG_HAS_WEBENGINE
    if (this->templateView == nullptr || !this->templateView->isVisible())
        return;

    static_cast<QWebEngineView*>(this->templateView)->page()->runJavaScript(script);
#else
    Q_UNUSED(script);
#endif
}

QString PreviewWidget::templateDataJson() const
{
    // A template carries its own sample values in window.debugData — the same
    // block the Inspector reads to build its typed fields. Feeding those back in
    // is what makes the preview show a populated graphic rather than an empty
    // one, and it needs nothing typed and no server.
    if (this->model == nullptr)
        return "{}";

    QString filePath = resolveTemplateFile(this->model->getDeviceName(), templateNameForSelection());
    if (filePath.isEmpty())
        return "{}";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return "{}";

    QString content = QTextStream(&file).readAll();
    file.close();

    QRegularExpression debugDataRegex("window\\.debugData\\s*=\\s*\\{([^}]*)\\}");
    QRegularExpressionMatch match = debugDataRegex.match(content);

    if (!match.hasMatch())
        return "{}";

    return "{" + match.captured(1) + "}";
}

QString PreviewWidget::formatTime(qint64 ms)
{
    int totalSec = static_cast<int>(ms / 1000);
    int min = totalSec / 60;
    int sec = totalSec % 60;
    return QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0'));
}

void PreviewWidget::playPause()
{
    if (this->player->playbackState() == QMediaPlayer::PlayingState)
    {
        this->player->pause();
        this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // play
    }
    else
    {
        this->player->play();
        this->playPauseButton->setText(QString::fromUtf8("\xe2\x8f\xb8")); // pause
    }
}

void PreviewWidget::stopPlayback()
{
    this->player->stop();
    this->player->setPosition(0);
    this->playPauseButton->setText(QString::fromUtf8("\xe2\x96\xb6")); // play

    this->meterLevels.clear();
    this->contentWidget->clearAudioLevels();
}

void PreviewWidget::updateMeters()
{
    if (legacyMode() || !showAudioMeters() || !this->audioAnalyser->hasLevels())
        return;

    QVector<double> target = this->audioAnalyser->dbfsAt(this->player->position());

    if (target.isEmpty())
    {
        this->contentWidget->clearAudioLevels();
        return;
    }

    if (this->meterLevels.size() != target.size())
        this->meterLevels = target;

    // A stopped or paused clip shows the level at that frame and holds it, which
    // is more useful than a meter that decays to nothing while the picture stays.
    const bool playing = this->player->playbackState() == QMediaPlayer::PlayingState;

    for (int i = 0; i < target.size(); i++)
    {
        this->meterLevels[i] = playing
            ? AudioLevelTrack::applyBallistics(this->meterLevels.at(i), target.at(i), METER_FALL_DB)
            : target.at(i);
    }

    this->contentWidget->setAudioLevels(this->meterLevels);
}

void PreviewWidget::templatePlay()
{
    // Data first, then play: a template that reads its fields on play would
    // otherwise animate on in empty and populate a frame later.
    runTemplateScript(QString("try { if (window.update) window.update(%1); } catch (e) {}").arg(templateDataJson()));
    runTemplateScript("try { if (window.play) window.play(); } catch (e) {}");
}

void PreviewWidget::templateNext()
{
    runTemplateScript("try { if (window.next) window.next(); } catch (e) {}");
}

void PreviewWidget::templateUpdate()
{
    runTemplateScript(QString("try { if (window.update) window.update(%1); } catch (e) {}").arg(templateDataJson()));
}

void PreviewWidget::templateStop()
{
    runTemplateScript("try { if (window.stop) window.stop(); } catch (e) {}");
}

void PreviewWidget::positionChanged(qint64 position)
{
    if (!this->sliderDragging)
        this->seekSlider->setValue(static_cast<int>(position));

    qint64 duration = this->player->duration();
    this->timeLabel->setText(formatTime(position) + " / " + formatTime(duration));
}

void PreviewWidget::durationChanged(qint64 duration)
{
    this->seekSlider->setRange(0, static_cast<int>(duration));
}

void PreviewWidget::sliderPressed()
{
    this->sliderDragging = true;
}

void PreviewWidget::sliderReleased()
{
    this->sliderDragging = false;
    this->player->setPosition(this->seekSlider->value());
}

void PreviewWidget::sliderMoved(int value)
{
    qint64 duration = this->player->duration();
    this->timeLabel->setText(formatTime(value) + " / " + formatTime(duration));

    // Scrubbing moves the meters too, because the levels are addressed by
    // position rather than by what is coming out of the player.
    if (!legacyMode() && showAudioMeters() && this->audioAnalyser->hasLevels())
    {
        this->meterLevels = this->audioAnalyser->dbfsAt(value);
        this->contentWidget->setAudioLevels(this->meterLevels);
    }
}

void PreviewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // Only update thumbnail display if no video is playing.
    if (this->player->playbackState() == QMediaPlayer::StoppedState &&
        this->player->source().isEmpty())
    {
        updateThumbnailDisplay();
    }
}

void PreviewWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Preview", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_PREVIEW_HEIGHT);
    else
        PanelHelper::applyExpandedHeight(this, "Preview", Panel::DEFAULT_PREVIEW_HEIGHT);
}
