#include "ShellCommand.h"

#include <QtCore/QXmlStreamWriter>

ShellCommand::ShellCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

const QString& ShellCommand::getCommandLine() const
{
    return this->commandLine;
}

const QString& ShellCommand::getWorkingDirectory() const
{
    return this->workingDirectory;
}

bool ShellCommand::getTriggerOnNext() const
{
    return this->triggerOnNext;
}

bool ShellCommand::getWaitForFinish() const
{
    return this->waitForFinish;
}

int ShellCommand::getTimeout() const
{
    return this->timeout;
}

void ShellCommand::setCommandLine(const QString& commandLine)
{
    this->commandLine = commandLine;
    emit commandLineChanged(this->commandLine);
    emit propertyChanged();
}

void ShellCommand::setWorkingDirectory(const QString& workingDirectory)
{
    this->workingDirectory = workingDirectory;
    emit workingDirectoryChanged(this->workingDirectory);
    emit propertyChanged();
}

void ShellCommand::setTriggerOnNext(bool triggerOnNext)
{
    this->triggerOnNext = triggerOnNext;
    emit triggerOnNextChanged(this->triggerOnNext);
    emit propertyChanged();
}

void ShellCommand::setWaitForFinish(bool waitForFinish)
{
    this->waitForFinish = waitForFinish;
    emit waitForFinishChanged(this->waitForFinish);
    emit propertyChanged();
}

void ShellCommand::setTimeout(int timeout)
{
    // A wait with no ceiling is a frozen client, so there is always one.
    if (timeout < 1)
        timeout = 1;
    if (timeout > 300)
        timeout = 300;

    this->timeout = timeout;
    emit timeoutChanged(this->timeout);
    emit propertyChanged();
}

void ShellCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setCommandLine(QString::fromStdWString(pt.get(L"commandline", L"")));
    setWorkingDirectory(QString::fromStdWString(pt.get(L"workingdirectory", L"")));
    setTriggerOnNext(pt.get(L"triggeronnext", false));
    setWaitForFinish(pt.get(L"waitforfinish", false));
    setTimeout(pt.get(L"timeout", 10));
}

void ShellCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("commandline", getCommandLine());
    writer.writeTextElement("workingdirectory", getWorkingDirectory());
    writer.writeTextElement("triggeronnext", (getTriggerOnNext() == true) ? "true" : "false");
    writer.writeTextElement("waitforfinish", (getWaitForFinish() == true) ? "true" : "false");
    writer.writeTextElement("timeout", QString("%1").arg(getTimeout()));
}
