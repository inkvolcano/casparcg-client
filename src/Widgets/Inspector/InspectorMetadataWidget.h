#pragma once

#include "../Shared.h"
#include "ui_InspectorMetadataWidget.h"

#include "Commands/AbstractCommand.h"
#include "Events/Library/LibraryItemSelectedEvent.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Events/Inspector/DeviceChangedEvent.h"
#include "Events/Inspector/TargetChangedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>

#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorMetadataWidget : public QWidget, Ui::InspectorMetadataWidget
{
    Q_OBJECT

    public:
        explicit InspectorMetadataWidget(QWidget* parent = 0);

    private:
        LibraryModel* model = nullptr;

        // The Media line: codec, resolution, rate and audio of a movie or still,
        // from the server's media scanner (MediaInfoClient). Rundown items only.
        QLabel* labelMediaTitle = nullptr;
        QLabel* labelMedia = nullptr;
        int mediaRequest = 0;
        void showMedia(const QString& deviceName, const QString& mediaName, const QString& type);
        void hideMedia();

        // The Reference row: the item's reference image (ReferenceImage.h), shown
        // when its rundown row is hovered. Choose... reads a picture file, Clear
        // removes it. Rundown items only.
        QPointer<AbstractCommand> command;

        // "From Preview": a reference picture from the server's preview channel.
        QPushButton* buttonReferenceCapture = nullptr;
        void captureReference();
        QMetaObject::Connection referenceConnection;

        // The Notes row: the item's operator note, edited here, shown when its
        // rundown row is hovered. Rundown items only.
        QLabel* labelNoteTitle = nullptr;
        class QPlainTextEdit* textNote = nullptr;
        QMetaObject::Connection noteConnection;
        void showNote();

        // Fire at: a time of day the item plays at, while time triggers are armed.
        QLabel* labelFireAtTitle = nullptr;
        QLineEdit* editFireAt = nullptr;
        void saveFireAt();
        QLabel* labelReferenceTitle = nullptr;
        QWidget* referenceRow = nullptr;
        QLabel* referencePreview = nullptr;
        QPushButton* buttonReferenceClear = nullptr;
        void showReference();
        void chooseReference();

        void blockAllSignals(bool block);

        Q_SLOT void labelChanged(QString);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void libraryItemSelected(const LibraryItemSelectedEvent&);
        Q_SLOT void emptyRundown(const EmptyRundownEvent&);
        Q_SLOT void targetChanged(const TargetChangedEvent&);
        Q_SLOT void deviceChanged(const DeviceChangedEvent&);
};
