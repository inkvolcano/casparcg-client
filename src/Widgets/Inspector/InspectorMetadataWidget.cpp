#include "InspectorMetadataWidget.h"

#include "Global.h"

#include "CasparDevice.h"

#include "EventManager.h"
#include "DeviceManager.h"
#include "DatabaseManager.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "MediaInfoClient.h"

#include <QtWidgets/QApplication>

InspectorMetadataWidget::InspectorMetadataWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL)
{
    setupUi(this);

    this->labelMediaTitle = new QLabel("Media", this);
    this->labelMediaTitle->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
    this->labelMedia = new QLabel(this);
    this->labelMedia->setWordWrap(true);
    this->labelMedia->setTextInteractionFlags(Qt::TextSelectableByMouse);
    this->labelMedia->setToolTip("What the server's media scanner found for this file.");
    this->gridLayout->addWidget(this->labelMediaTitle, 1, 0);
    this->gridLayout->addWidget(this->labelMedia, 1, 1);
    hideMedia();

    QObject::connect(&EventManager::getInstance(), SIGNAL(targetChanged(const TargetChangedEvent&)), this, SLOT(targetChanged(const TargetChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(deviceChanged(const DeviceChangedEvent&)), this, SLOT(deviceChanged(const DeviceChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent&)), this, SLOT(libraryItemSelected(const LibraryItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent&)), this, SLOT(emptyRundown(const EmptyRundownEvent&)));
}

void InspectorMetadataWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    this->lineEditLabel->setEnabled(true);
    this->lineEditLabel->setReadOnly(false);
    this->lineEditLabel->setText(this->model->getLabel().split('/').last());

    blockAllSignals(false);

    showMedia(this->model->getDeviceName(), this->model->getName(), this->model->getType());
}

void InspectorMetadataWidget::showMedia(const QString& deviceName, const QString& mediaName, const QString& type)
{
    if (type != Rundown::MOVIE && type != Rundown::STILL)
    {
        hideMedia();
        return;
    }

    // Answers can arrive after another item is selected; only the last asked is shown.
    const int asked = ++this->mediaRequest;

    this->labelMediaTitle->setVisible(true);
    this->labelMedia->setVisible(true);
    this->labelMedia->setText("Asking the server's scanner...");
    this->labelMedia->setStyleSheet("color: rgba(160, 160, 160, 200);");

    MediaInfoClient::getInstance().request(deviceName, mediaName, type == Rundown::STILL, this,
        [this, asked](const QString& text, bool known) {
            if (asked != this->mediaRequest)
                return;

            if (text.isEmpty())
            {
                hideMedia();
                return;
            }

            this->labelMedia->setText(text);
            this->labelMedia->setStyleSheet(known ? QString() : QString("color: rgba(160, 160, 160, 200);"));
        });
}

void InspectorMetadataWidget::hideMedia()
{
    ++this->mediaRequest;
    this->labelMediaTitle->setVisible(false);
    this->labelMedia->setVisible(false);
    this->labelMedia->clear();
}

void InspectorMetadataWidget::targetChanged(const TargetChangedEvent& event)
{
    if (this->model != nullptr && this->labelMedia->isVisible())
        showMedia(this->model->getDeviceName(), event.getTarget(), this->model->getType());
}

void InspectorMetadataWidget::deviceChanged(const DeviceChangedEvent& event)
{
    if (this->model != nullptr && this->labelMedia->isVisible() && !event.getDeviceName().isEmpty())
        showMedia(event.getDeviceName(), this->model->getName(), this->model->getType());
}

void InspectorMetadataWidget::libraryItemSelected(const LibraryItemSelectedEvent& event)
{
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    this->lineEditLabel->setEnabled(false);
    this->lineEditLabel->clear();

    blockAllSignals(false);

    hideMedia();
}

void InspectorMetadataWidget::emptyRundown(const EmptyRundownEvent& event)
{
    Q_UNUSED(event);

    hideMedia();

    blockAllSignals(true);

    this->lineEditLabel->setEnabled(false);
    this->lineEditLabel->clear();

    blockAllSignals(false);
}

void InspectorMetadataWidget::blockAllSignals(bool block)
{
    this->lineEditLabel->blockSignals(block);
}

void InspectorMetadataWidget::labelChanged(QString name)
{
    Q_UNUSED(name);

    EventManager::getInstance().fireLabelChangedEvent(LabelChangedEvent(this->lineEditLabel->text()));
}
