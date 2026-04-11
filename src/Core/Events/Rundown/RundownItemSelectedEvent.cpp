#include "RundownItemSelectedEvent.h"

#include "Global.h"

RundownItemSelectedEvent::RundownItemSelectedEvent(AbstractCommand* command, LibraryModel* model, QWidget* source, QWidget* parent,
                                                     const QList<AbstractCommand*>& allCommands)
    : command(command), model(model), source(source), parent(parent), allCommands(allCommands)
{
}

AbstractCommand* RundownItemSelectedEvent::getCommand() const
{
    return this->command;
}

LibraryModel* RundownItemSelectedEvent::getLibraryModel() const
{
    return this->model;
}

QWidget* RundownItemSelectedEvent::getSource() const
{
    return this->source;
}

QWidget* RundownItemSelectedEvent::getParent() const
{
    return this->parent;
}

const QList<AbstractCommand*>& RundownItemSelectedEvent::getAllCommands() const
{
    return this->allCommands;
}
