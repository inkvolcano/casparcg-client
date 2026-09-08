#include "AbstractCommand.h"

#include "CloneGroupRegistry.h"

#include <QtCore/QXmlStreamWriter>

AbstractCommand::AbstractCommand(QObject* parent)
    : QObject(parent)
{
}

AbstractCommand::~AbstractCommand()
{
}

int AbstractCommand::getChannel() const
{
    if (this->channelOverrideValue > 0)
        return this->channelOverrideValue;
    return this->channel;
}

void AbstractCommand::setChannelOverride(int channel)
{
    this->channelOverrideValue = channel;
}

void AbstractCommand::clearChannelOverride()
{
    this->channelOverrideValue = 0;
}

int AbstractCommand::getBaseChannel() const
{
    return this->channel;
}

int AbstractCommand::getVideolayer() const
{
    return this->videolayer;
}

int AbstractCommand::getDelay() const
{
    return this->delay;
}

int AbstractCommand::getDuration() const
{
    return this->duration;
}

bool AbstractCommand::getAllowGpi() const
{
    return this->allowGpi;
}

bool AbstractCommand::getAllowRemoteTriggering() const
{
    return this->allowRemoteTriggering;
}

QString AbstractCommand::getRemoteTriggerId() const
{
    return this->remoteTriggerId;
}

QString AbstractCommand::getStoryId() const
{
    return this->storyId;
}

void AbstractCommand::setChannel(int channel)
{
    this->channel = channel;
    this->channelOverrideValue = 0; // Clear preview override when channel is explicitly set.
    emit channelChanged(this->channel);
    emit propertyChanged();
}

void AbstractCommand::setVideolayer(int videolayer)
{
    this->videolayer = videolayer;
    emit videolayerChanged(this->videolayer);
    emit propertyChanged();
}

void AbstractCommand::setDelay(int delay)
{
    this->delay = delay;
    emit delayChanged(this->delay);
    emit propertyChanged();
}

void AbstractCommand::setDuration(int duration)
{
    this->duration = duration;
    emit durationChanged(this->duration);
    emit propertyChanged();
}

void AbstractCommand::setAllowGpi(bool allowGpi)
{
    this->allowGpi = allowGpi;
    emit allowGpiChanged(this->allowGpi);
    emit propertyChanged();
}

void AbstractCommand::setAllowRemoteTriggering(bool allowRemoteTriggering)
{
    this->allowRemoteTriggering = allowRemoteTriggering;
    emit allowRemoteTriggeringChanged(this->allowRemoteTriggering);
    emit propertyChanged();
}

void AbstractCommand::setRemoteTriggerId(const QString& remoteTriggerId)
{
    this->remoteTriggerId = remoteTriggerId;
    emit remoteTriggerIdChanged(this->remoteTriggerId);
    emit propertyChanged();
}

void AbstractCommand::setStoryId(const QString& storyId)
{
    this->storyId = storyId;
    emit storyIdChanged(this->storyId);
    emit propertyChanged();
}

int AbstractCommand::getTriggerBank() const
{
    return this->triggerBank;
}

void AbstractCommand::setTriggerBank(int triggerBank)
{
    this->triggerBank = triggerBank;
    emit triggerBankChanged(this->triggerBank);
    emit propertyChanged();
}

QString AbstractCommand::getCloneGroupId() const
{
    return this->cloneGroupId;
}

bool AbstractCommand::getDisabled() const
{
    return this->disabled;
}

void AbstractCommand::setDisabled(bool disabled)
{
    this->disabled = disabled;
    emit disabledChanged(this->disabled);
    emit propertyChanged();
}

bool AbstractCommand::getShowInSimpleMode() const
{
    return this->showInSimpleMode;
}

int AbstractCommand::getSimpleModeSlot() const
{
    return this->simpleModeSlot;
}

void AbstractCommand::setSimpleModeSlot(int slot)
{
    this->simpleModeSlot = slot;
    emit propertyChanged();
}

bool AbstractCommand::getSimpleModeNextButton() const
{
    return this->simpleModeNextButton;
}

void AbstractCommand::setSimpleModeNextButton(bool enabled)
{
    this->simpleModeNextButton = enabled;
    emit propertyChanged();
}

bool AbstractCommand::getSimpleModeGroupInvokes() const
{
    return this->simpleModeGroupInvokes;
}

void AbstractCommand::setSimpleModeGroupInvokes(bool enabled)
{
    this->simpleModeGroupInvokes = enabled;
}

int AbstractCommand::getSimpleModeWidth() const
{
    return this->simpleModeWidth;
}

void AbstractCommand::setSimpleModeWidth(int columns)
{
    this->simpleModeWidth = columns;
}

int AbstractCommand::getSimpleModeHeight() const
{
    return this->simpleModeHeight;
}

void AbstractCommand::setSimpleModeHeight(int rows)
{
    this->simpleModeHeight = rows;
}

int AbstractCommand::getSimpleModeLabelSize() const
{
    return this->simpleModeLabelSize;
}

void AbstractCommand::setSimpleModeLabelSize(int size)
{
    this->simpleModeLabelSize = size;
    emit propertyChanged();
}

QString AbstractCommand::getSimpleModeIcon() const
{
    return this->simpleModeIcon;
}

void AbstractCommand::setSimpleModeIcon(const QString& icon)
{
    this->simpleModeIcon = icon;
    emit propertyChanged();
}

void AbstractCommand::setShowInSimpleMode(bool showInSimpleMode)
{
    this->showInSimpleMode = showInSimpleMode;
    emit showInSimpleModeChanged(this->showInSimpleMode);
    emit propertyChanged();
}

void AbstractCommand::setCloneGroupId(const QString& cloneGroupId)
{
    if (!this->cloneGroupId.isEmpty() && this->cloneGroupId != cloneGroupId)
        CloneGroupRegistry::getInstance().unregisterCommand(this);

    this->cloneGroupId = cloneGroupId;

    if (!cloneGroupId.isEmpty())
        CloneGroupRegistry::getInstance().registerCommand(cloneGroupId, this);

    emit cloneGroupIdChanged(this->cloneGroupId);
}

void AbstractCommand::readProperties(boost::property_tree::wptree& pt)
{
    setChannel(pt.get(L"channel", Output::DEFAULT_CHANNEL));
    setVideolayer(pt.get(L"videolayer", Output::DEFAULT_VIDEOLAYER));
    setDelay(pt.get(L"delay", Output::DEFAULT_DELAY));
    setDuration(pt.get(L"duration", Output::DEFAULT_DURATION));
    setAllowGpi(pt.get(L"allowgpi", Output::DEFAULT_ALLOW_GPI));
    setAllowRemoteTriggering(pt.get(L"allowremotetriggering", Output::DEFAULT_ALLOW_REMOTE_TRIGGERING));
    setRemoteTriggerId(QString::fromStdWString(pt.get(L"remotetriggerid", Output::DEFAULT_REMOTE_TRIGGER_ID.toStdWString())));
    setStoryId(QString::fromStdWString(pt.get(L"storyid", QString("").toStdWString())));
    setTriggerBank(pt.get(L"triggerbank", 0));
    setCloneGroupId(QString::fromStdWString(pt.get(L"clonegroupid", QString("").toStdWString())));
    setDisabled(pt.get(L"disabled", false));
    setShowInSimpleMode(pt.get(L"showinsimplemode", false));
    setSimpleModeSlot(pt.get(L"simplemodeslot", -1));
    setSimpleModeNextButton(pt.get(L"simplemodenextbutton", false));
    setSimpleModeGroupInvokes(pt.get(L"simplemodegroupinvokes", false));
    setSimpleModeWidth(pt.get(L"simplemodewidth", 1));
    setSimpleModeHeight(pt.get(L"simplemodeheight", 0));
    setSimpleModeLabelSize(pt.get(L"simplemodelabelsize", 0));
    setSimpleModeIcon(QString::fromStdWString(pt.get(L"simplemodeicon", std::wstring())));
}

void AbstractCommand::writeProperties(QXmlStreamWriter& writer)
{
    writer.writeTextElement("channel", QString::number(getBaseChannel()));
    writer.writeTextElement("videolayer", QString::number(getVideolayer()));
    writer.writeTextElement("delay", QString::number(getDelay()));
    writer.writeTextElement("duration", QString::number(getDuration()));
    writer.writeTextElement("allowgpi", (getAllowGpi() == true) ? "true" : "false");
    writer.writeTextElement("allowremotetriggering", (getAllowRemoteTriggering() == true) ? "true" : "false");
    writer.writeTextElement("remotetriggerid", getRemoteTriggerId());
    writer.writeTextElement("storyid", getStoryId());
    writer.writeTextElement("triggerbank", QString::number(getTriggerBank()));
    if (!getCloneGroupId().isEmpty())
        writer.writeTextElement("clonegroupid", getCloneGroupId());
    if (getDisabled())
        writer.writeTextElement("disabled", "true");
    if (getShowInSimpleMode())
        writer.writeTextElement("showinsimplemode", "true");
    if (getSimpleModeSlot() >= 0)
        writer.writeTextElement("simplemodeslot", QString::number(getSimpleModeSlot()));
    if (getSimpleModeNextButton())
        writer.writeTextElement("simplemodenextbutton", "true");
    if (getSimpleModeGroupInvokes())
        writer.writeTextElement("simplemodegroupinvokes", "true");
    if (getSimpleModeWidth() > 1)
        writer.writeTextElement("simplemodewidth", QString::number(getSimpleModeWidth()));
    if (getSimpleModeHeight() > 0)
        writer.writeTextElement("simplemodeheight", QString::number(getSimpleModeHeight()));
    if (getSimpleModeLabelSize() > 0)
        writer.writeTextElement("simplemodelabelsize", QString::number(getSimpleModeLabelSize()));
    if (!getSimpleModeIcon().isEmpty())
        writer.writeTextElement("simplemodeicon", getSimpleModeIcon());
}
