#pragma once

#include "../Shared.h"
#include "AbstractCommand.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QString>

class QObject;
class QXmlStreamWriter;

class CORE_EXPORT GroupCommand : public AbstractCommand
{
    Q_OBJECT

    public:
        explicit GroupCommand(QObject* parent = 0);

        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

        const QString& getNotes() const;
        bool getAutoPlay() const;
        bool getLoop() const;
        bool getAutoLoop() const;
        int getAutoLoopDelay() const;
        bool getTreatAsDropdown() const;
        int getDropdownIndex() const;
        bool getTreatAsShotbox() const;

        void setNotes(const QString& notes);
        void setAutoPlay(bool autoPlay);
        void setLoop(bool loop);
        void setAutoLoop(bool autoLoop);
        void setAutoLoopDelay(int autoLoopDelay);
        void setTreatAsDropdown(bool treatAsDropdown);
        void setDropdownIndex(int index);
        void setTreatAsShotbox(bool treatAsShotbox);

    private:
        QString notes = Group::DEFAULT_NOTE;
        bool autoPlay = Group::DEFAULT_AUTO_PLAY;
        bool loop = Group::DEFAULT_LOOP;
        bool autoLoop = Group::DEFAULT_AUTO_LOOP;
        int autoLoopDelay = Group::DEFAULT_AUTO_LOOP_DELAY;
        // Dropdown group: the group acts as a chooser over its children — the
        // selected child is what actually fires, instead of the whole group.
        bool treatAsDropdown = false;
        int dropdownIndex = 0;

        // Shotbox group: one grid key holding a row per child - label on the
        // left, the child's own controls on the right. The group itself never
        // fires; each row fires its own child on that child's channel.
        //
        // Mutually exclusive with the dropdown above, because both are answers to
        // "what does this group look like on the grid" and a group cannot be a
        // chooser and a rack at the same time.
        bool treatAsShotbox = false;

        Q_SIGNAL void notesChanged(const QString&);
        Q_SIGNAL void autoPlayChanged(bool);
        Q_SIGNAL void loopChanged(bool);
        Q_SIGNAL void autoLoopChanged(bool);
        Q_SIGNAL void autoLoopDelayChanged(int);
        Q_SIGNAL void treatAsDropdownChanged(bool);
        Q_SIGNAL void dropdownIndexChanged(int);
        Q_SIGNAL void treatAsShotboxChanged(bool);
};
