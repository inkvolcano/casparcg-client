#include "GroupCommand.h"

#include <QtCore/QXmlStreamWriter>

GroupCommand::GroupCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

const QString& GroupCommand::getNotes() const
{
    return this->notes;
}

bool GroupCommand::getAutoPlay() const
{
    return this->autoPlay;
}

bool GroupCommand::getLoop() const
{
    return this->loop;
}

void GroupCommand::setAutoPlay(bool autoPlay)
{
    this->autoPlay = autoPlay;
    emit autoPlayChanged(this->autoPlay);
    emit propertyChanged();
}

void GroupCommand::setLoop(bool loop)
{
    this->loop = loop;
    emit loopChanged(this->loop);
    emit propertyChanged();
}

bool GroupCommand::getAutoLoop() const
{
    return this->autoLoop;
}

int GroupCommand::getAutoLoopDelay() const
{
    return this->autoLoopDelay;
}

void GroupCommand::setAutoLoop(bool autoLoop)
{
    this->autoLoop = autoLoop;
    emit autoLoopChanged(this->autoLoop);
    emit propertyChanged();
}

void GroupCommand::setAutoLoopDelay(int autoLoopDelay)
{
    this->autoLoopDelay = autoLoopDelay;
    emit autoLoopDelayChanged(this->autoLoopDelay);
    emit propertyChanged();
}

void GroupCommand::setNotes(const QString& notes)
{
    this->notes = notes;
    emit notesChanged(this->notes);
    emit propertyChanged();
}

void GroupCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setNotes(QString::fromStdWString(pt.get(L"notes", Group::DEFAULT_NOTE.toStdWString())));
    setAutoPlay(pt.get(L"autoplay", Group::DEFAULT_AUTO_PLAY));
    setLoop(pt.get(L"loop", Group::DEFAULT_LOOP));
    setAutoLoopDelay(pt.get(L"autoloopdelay", Group::DEFAULT_AUTO_LOOP_DELAY));
    setAutoLoop(pt.get(L"autoloop", Group::DEFAULT_AUTO_LOOP));
}

void GroupCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("notes", this->getNotes());
    writer.writeTextElement("autoplay", (getAutoPlay() == true) ? "true" : "false");
    writer.writeTextElement("loop", (getLoop() == true) ? "true" : "false");
    writer.writeTextElement("autoloop", (getAutoLoop() == true) ? "true" : "false");
    writer.writeTextElement("autoloopdelay", QString::number(getAutoLoopDelay()));
}
