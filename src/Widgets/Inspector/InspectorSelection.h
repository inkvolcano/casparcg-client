#pragma once

#include "../Shared.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>

class AbstractCommand;
class RundownItemSelectedEvent;

// The items selected in the rundown, for Inspector sections that change all of
// them at once (build 311). The Output section already did so for channel,
// layer, delay, duration and its two ticks; the clip, still, audio, HTML,
// colour, template and image scroller sections now do the same for their
// transition and their flags, on every selected item of the same kind.
//
// A single selection holds nothing here, so a section changes only its own
// item. Items that go away drop out on their own (QPointer). Undo needs nothing
// extra: InspectorUndo snapshots the whole rundown around a field's edit.
class WIDGETS_EXPORT InspectorSelection : public QObject
{
    Q_OBJECT

    public:
        static InspectorSelection& getInstance();

        // Every selected item, the one the Inspector shows among them; empty
        // when only one is selected.
        static QList<AbstractCommand*> selected();

        // Makes the change to the item the Inspector shows, then to every other
        // selected item of the same kind.
        template <typename T, typename F>
        static void apply(T* primary, F change)
        {
            if (primary == nullptr)
                return;

            change(primary);
            for (AbstractCommand* other : selected())
            {
                if (other == static_cast<AbstractCommand*>(primary))
                    continue;
                if (T* same = dynamic_cast<T*>(other))
                    change(same);
            }
        }

        Q_SIGNAL void selectionChanged(int count);

    private:
        explicit InspectorSelection();

        QList<QPointer<AbstractCommand>> commands;

        void rundownItemSelected(const RundownItemSelectedEvent& event);
};
