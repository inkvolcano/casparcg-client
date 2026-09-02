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

bool GroupCommand::getTreatAsDropdown() const
{
    return this->treatAsDropdown;
}

void GroupCommand::setTreatAsDropdown(bool treatAsDropdown)
{
    this->treatAsDropdown = treatAsDropdown;
    emit treatAsDropdownChanged(this->treatAsDropdown);
}

int GroupCommand::getDropdownIndex() const
{
    return this->dropdownIndex;
}

void GroupCommand::setDropdownIndex(int index)
{
    this->dropdownIndex = index;
    emit dropdownIndexChanged(this->dropdownIndex);
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
    setTreatAsDropdown(pt.get(L"treatasdropdown", false));
    setDropdownIndex(pt.get(L"dropdownindex", 0));
}

void GroupCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("notes", this->getNotes());
    writer.writeTextElement("autoplay", (getAutoPlay() == true) ? "true" : "false");
    writer.writeTextElement("loop", (getLoop() == true) ? "true" : "false");
    writer.writeTextElement("autoloop", (getAutoLoop() == true) ? "true" : "false");
    writer.writeTextElement("autoloopdelay", QString::number(getAutoLoopDelay()));
    if (getTreatAsDropdown())
    {
        writer.writeTextElement("treatasdropdown", "true");
        writer.writeTextElement("dropdownindex", QString::number(getDropdownIndex()));
    }
}
