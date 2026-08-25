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

        void setNotes(const QString& notes);
        void setAutoPlay(bool autoPlay);
        void setLoop(bool loop);
        void setAutoLoop(bool autoLoop);
        void setAutoLoopDelay(int autoLoopDelay);

    private:
        QString notes = Group::DEFAULT_NOTE;
        bool autoPlay = Group::DEFAULT_AUTO_PLAY;
        bool loop = Group::DEFAULT_LOOP;
        bool autoLoop = Group::DEFAULT_AUTO_LOOP;
        int autoLoopDelay = Group::DEFAULT_AUTO_LOOP_DELAY;

        Q_SIGNAL void notesChanged(const QString&);
        Q_SIGNAL void autoPlayChanged(bool);
        Q_SIGNAL void loopChanged(bool);
        Q_SIGNAL void autoLoopChanged(bool);
        Q_SIGNAL void autoLoopDelayChanged(int);
};
