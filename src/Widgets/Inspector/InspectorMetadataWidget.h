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
#include <QtCore/QSharedPointer>

#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorMetadataWidget : public QWidget, Ui::InspectorMetadataWidget
{
    Q_OBJECT

    public:
        explicit InspectorMetadataWidget(QWidget* parent = 0);

    private:
        LibraryModel* model;

        // The Media line: codec, resolution, rate and audio of a movie or still,
        // from the server's media scanner (MediaInfoClient). Rundown items only.
        QLabel* labelMediaTitle = nullptr;
        QLabel* labelMedia = nullptr;
        int mediaRequest = 0;
        void showMedia(const QString& deviceName, const QString& mediaName, const QString& type);
        void hideMedia();

        void blockAllSignals(bool block);

        Q_SLOT void labelChanged(QString);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void libraryItemSelected(const LibraryItemSelectedEvent&);
        Q_SLOT void emptyRundown(const EmptyRundownEvent&);
        Q_SLOT void targetChanged(const TargetChangedEvent&);
        Q_SLOT void deviceChanged(const DeviceChangedEvent&);
};
