#pragma once

#include "../Shared.h"
#include "AbstractCommand.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QString>

class QObject;
class QXmlStreamWriter;

// Runs a program on the machine the client is on, as a rundown item. The thing
// people reach for this for is the kit CasparCG does not talk to: a router, a
// projector shutter, a telnet box, a script that does the part no protocol
// covers.
//
// The command line is stored as typed and split at run time, so a rundown moved
// between machines carries what the operator wrote rather than one machine's
// idea of how to tokenise it.
class CORE_EXPORT ShellCommand : public AbstractCommand
{
    Q_OBJECT

    public:
        explicit ShellCommand(QObject* parent = 0);

        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

        const QString& getCommandLine() const;
        const QString& getWorkingDirectory() const;
        bool getTriggerOnNext() const;
        bool getWaitForFinish() const;
        int getTimeout() const;

        void setCommandLine(const QString& commandLine);
        void setWorkingDirectory(const QString& workingDirectory);
        void setTriggerOnNext(bool triggerOnNext);
        void setWaitForFinish(bool waitForFinish);
        void setTimeout(int timeout);

    private:
        QString commandLine;
        QString workingDirectory;
        bool triggerOnNext = false;

        // Off by default: a rundown should not stall on a program that decides to
        // take its time. On, with the timeout below, is for the cases where the
        // next item genuinely depends on this one having finished.
        bool waitForFinish = false;
        int timeout = 10;

    Q_SIGNALS:
        void commandLineChanged(const QString&);
        void workingDirectoryChanged(const QString&);
        void triggerOnNextChanged(bool);
        void waitForFinishChanged(bool);
        void timeoutChanged(int);
};
