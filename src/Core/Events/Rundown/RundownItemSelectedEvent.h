#pragma once

#include "../../Shared.h"
#include "../../Commands/AbstractCommand.h"
#include "../../Models/LibraryModel.h"

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <QtWidgets/QWidget>

class CORE_EXPORT RundownItemSelectedEvent
{
    public:
        explicit RundownItemSelectedEvent(AbstractCommand* command, LibraryModel* model, QWidget* source = NULL, QWidget* parent = NULL,
                                          const QList<AbstractCommand*>& allCommands = QList<AbstractCommand*>());

        AbstractCommand* getCommand() const;
        LibraryModel* getLibraryModel() const;
        QWidget* getSource() const;
        QWidget* getParent() const;
        const QList<AbstractCommand*>& getAllCommands() const;

    private:
        AbstractCommand* command;
        LibraryModel* model;
        QWidget* source;
        QWidget* parent;
        QList<AbstractCommand*> allCommands;
};
