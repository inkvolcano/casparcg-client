#include "InspectorMetadataWidget.h"

#include "Global.h"

#include "CasparDevice.h"

#include "EventManager.h"
#include "DeviceManager.h"
#include "DatabaseManager.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "MediaInfoClient.h"
#include "ReferenceImage.h"
#include "DialogPosition.h"

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

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

    this->labelReferenceTitle = new QLabel("Reference", this);
    this->labelReferenceTitle->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignTop);
    this->labelReferenceTitle->setToolTip("A picture of what this item puts on air, shown when you hover its row in the rundown.");
    this->referenceRow = new QWidget(this);
    QHBoxLayout* referenceLayout = new QHBoxLayout(this->referenceRow);
    referenceLayout->setContentsMargins(0, 0, 0, 0);
    referenceLayout->setSpacing(6);
    this->referencePreview = new QLabel(this->referenceRow);
    this->referencePreview->setFixedSize(128, 72);
    this->referencePreview->setAlignment(Qt::AlignCenter);
    this->referencePreview->setStyleSheet("QLabel { background-color: rgba(0, 0, 0, 60); border: 1px solid rgba(90, 90, 90, 160); "
                                          "color: rgba(150, 150, 150, 200); font-size: 10px; }");
    referenceLayout->addWidget(this->referencePreview);
    QPushButton* choose = new QPushButton("Choose...", this->referenceRow);
    choose->setToolTip("Pick a picture file. You can also drop a picture file onto the row in the rundown.");
    this->buttonReferenceClear = new QPushButton("Clear", this->referenceRow);
    referenceLayout->addWidget(choose, 0, Qt::AlignTop);
    referenceLayout->addWidget(this->buttonReferenceClear, 0, Qt::AlignTop);
    referenceLayout->addStretch();
    this->gridLayout->addWidget(this->labelReferenceTitle, 2, 0);
    this->gridLayout->addWidget(this->referenceRow, 2, 1);
    this->labelReferenceTitle->setVisible(false);
    this->referenceRow->setVisible(false);

    QObject::connect(choose, &QPushButton::clicked, this, [this]() { chooseReference(); });
    QObject::connect(this->buttonReferenceClear, &QPushButton::clicked, this, [this]() {
        if (!this->command.isNull())
            this->command->setReferenceImage(QString());
    });

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

    // Follows the selected item's picture, whichever way it changes - this row, a
    // drop on the rundown row, an undo - and nothing else's.
    QObject::disconnect(this->referenceConnection);
    this->command = event.getCommand();
    if (!this->command.isNull())
        this->referenceConnection = QObject::connect(this->command.data(), &AbstractCommand::referenceImageChanged,
                                                     this, [this]() { showReference(); });
    showReference();
}

void InspectorMetadataWidget::showReference()
{
    const bool shown = !this->command.isNull();
    this->labelReferenceTitle->setVisible(shown);
    this->referenceRow->setVisible(shown);
    if (!shown)
        return;

    const QString picture = this->command->getReferenceImage();
    const QImage image = ReferenceImage::decode(picture);
    if (image.isNull())
    {
        this->referencePreview->setPixmap(QPixmap());
        this->referencePreview->setText("No picture");
        this->referencePreview->setToolTip("Choose a picture, or drop a picture file onto the row in the rundown.");
        this->buttonReferenceClear->setEnabled(false);
        return;
    }

    this->referencePreview->setText(QString());
    this->referencePreview->setPixmap(QPixmap::fromImage(image.scaled(this->referencePreview->size() - QSize(2, 2),
                                                                      Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    this->referencePreview->setToolTip(ReferenceImage::tooltipHtml(picture));
    this->buttonReferenceClear->setEnabled(true);
}

void InspectorMetadataWidget::chooseReference()
{
    if (this->command.isNull())
        return;

    // Remembered for the session: the next lower third is usually in the same folder.
    static QString lastFolder;

    QStringList patterns;
    for (const QString& suffix : ReferenceImage::fileSuffixes())
        patterns.append("*." + suffix);

    const QString path = QFileDialog::getOpenFileName(this, "Reference Image", lastFolder,
                                                      QString("Pictures (%1)").arg(patterns.join(' ')));
    if (path.isEmpty())
        return;

    lastFolder = QFileInfo(path).absolutePath();

    const QString picture = ReferenceImage::fromFile(path);
    if (picture.isEmpty())
    {
        QMessageBox::warning(this, "Reference Image", QString("%1 could not be read as a picture.").arg(QFileInfo(path).fileName()));
        return;
    }

    if (!this->command.isNull())
        this->command->setReferenceImage(picture);
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
    QObject::disconnect(this->referenceConnection);
    this->command = nullptr;
    showReference();

    this->model = event.getLibraryModel();

    blockAllSignals(true);

    this->lineEditLabel->setEnabled(false);
    this->lineEditLabel->clear();

    blockAllSignals(false);

    hideMedia();
}

void InspectorMetadataWidget::emptyRundown(const EmptyRundownEvent& event)
{
    QObject::disconnect(this->referenceConnection);
    this->command = nullptr;
    showReference();

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
