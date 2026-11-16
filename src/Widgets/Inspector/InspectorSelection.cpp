#include "InspectorSelection.h"

#include "EventManager.h"

#include "Commands/AbstractCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"

InspectorSelection& InspectorSelection::getInstance()
{
    static InspectorSelection* instance = new InspectorSelection();
    return *instance;
}

InspectorSelection::InspectorSelection()
    : QObject()
{
    QObject::connect(&EventManager::getInstance(), &EventManager::rundownItemSelected, this,
                     [this](const RundownItemSelectedEvent& event) { rundownItemSelected(event); });
}

void InspectorSelection::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->commands.clear();
    if (event.getAllCommands().count() > 1)
    {
        for (AbstractCommand* command : event.getAllCommands())
            this->commands.append(command);
    }
    emit selectionChanged(this->commands.count());
}

QList<AbstractCommand*> InspectorSelection::selected()
{
    QList<AbstractCommand*> live;
    for (const QPointer<AbstractCommand>& command : getInstance().commands)
    {
        if (!command.isNull())
            live.append(command.data());
    }
    return live;
}
