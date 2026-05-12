#include "TemplateCommand.h"

#include "Xml.h"

#include <QtCore/QXmlStreamWriter>

TemplateCommand::TemplateCommand(QObject* parent)
    : AbstractCommand(parent)
{
    this->videolayer = Output::DEFAULT_FLASH_VIDEOLAYER;
}

TransformData& TemplateCommand::getTransform() { return this->m_transform; }
const TransformData& TemplateCommand::getTransform() const { return this->m_transform; }

int TemplateCommand::getFlashlayer() const
{
    return this->flashlayer;
}

const QString& TemplateCommand::getInvoke() const
{
    if (this->invokeHotkeyIndex >= 0 && this->invokeHotkeyIndex < this->invokes.size())
        return this->invokes[this->invokeHotkeyIndex];

    static const QString empty;
    return empty;
}

const QStringList& TemplateCommand::getInvokes() const
{
    return this->invokes;
}

QString TemplateCommand::getInvokeAt(int index) const
{
    if (index >= 0 && index < this->invokes.size())
        return this->invokes[index];
    return QString();
}

int TemplateCommand::getInvokeCount() const
{
    return this->invokes.size();
}

int TemplateCommand::getInvokeHotkeyIndex() const
{
    return this->invokeHotkeyIndex;
}

bool TemplateCommand::getUseStoredData() const
{
    return this->useStoredData;
}

bool TemplateCommand::getSendAsJson() const
{
    return this->sendAsJson;
}

bool TemplateCommand::getUseUppercaseData() const
{
    return this->useUppercaseData;
}

const QString& TemplateCommand::getTemplateName() const
{
    return this->templateName;
}

const QString TemplateCommand::getTemplateData() const
{
    QString templateData;
    if (this->useStoredData)
    {
        if (this->models.count() == 0)
            return "";

        templateData.append(this->models.at(0).getValue());
    }
    else
    {
        if (this->sendAsJson)
        {
            QJsonObject jsonObject;
            foreach (KeyValueModel model, this->models)
            {
                jsonObject[model.getKey()] = (this->useUppercaseData == true) ? model.getValue().toUpper() : model.getValue();
            }
            QJsonDocument jsonDocument(jsonObject);
            QString strJson(jsonDocument.toJson(QJsonDocument::Compact));
            strJson.replace("\"", "\\\"");
            templateData.append(strJson);
        }
        else
        {
            templateData.append("<templateData>");
            foreach (KeyValueModel model, this->models)
            {
                QString componentData = TemplateData::DEFAULT_COMPONENT_DATA_XML;
                componentData.replace("#KEY", model.getKey());

                QString value = model.getValue();
                if (this->newlineBehavior == 0) // Ignore: strip newlines
                {
                    value.replace("\r\n", "");
                    value.replace("\n", "");
                    value.replace("\r", "");
                }
                else if (this->newlineBehavior == 2) // innerHTML: replace newlines with <br>
                {
                    value.replace("\r\n", "<br>");
                    value.replace("\n", "<br>");
                    value.replace("\r", "<br>");
                }
                // innerText (1): leave newlines for Xml::encode → &#10;
                value = Xml::encode(value).replace("\\", "\\\\");
                componentData.replace("#VALUE", (this->useUppercaseData == true) ? value.toUpper() : value);

                templateData.append(componentData);
            }
            templateData.append("</templateData>");
        }
    }

    return templateData;
}

const QList<KeyValueModel>& TemplateCommand::getTemplateDataModels() const
{
    return this->models;
}

bool TemplateCommand::getTriggerOnNext() const
{
    return this->triggerOnNext;
}

void TemplateCommand::setFlashlayer(int flashlayer)
{
    this->flashlayer = flashlayer;
    emit flashlayerChanged(this->flashlayer);
    emit propertyChanged();
}

void TemplateCommand::setInvoke(const QString& invoke)
{
    if (this->invokes.isEmpty())
        this->invokes.append(invoke);
    else
        this->invokes[0] = invoke;
    this->invokeHotkeyIndex = 0;
    emit invokesChanged(this->invokes);
    emit propertyChanged();
}

void TemplateCommand::setInvokes(const QStringList& invokes)
{
    this->invokes = invokes;
    if (this->invokes.isEmpty())
        this->invokes.append(Template::DEFAULT_INVOKE);
    if (this->invokeHotkeyIndex >= this->invokes.size())
        this->invokeHotkeyIndex = 0;
    emit invokesChanged(this->invokes);
    emit propertyChanged();
}

void TemplateCommand::setInvokeHotkeyIndex(int index)
{
    if (index >= 0 && index < this->invokes.size())
        this->invokeHotkeyIndex = index;
    emit propertyChanged();
}

void TemplateCommand::setPendingInvokeOverride(const QString& override)
{
    this->pendingInvokeOverride = override;
}

QString TemplateCommand::takePendingInvokeOverride()
{
    QString result = this->pendingInvokeOverride;
    this->pendingInvokeOverride.clear();
    return result;
}

void TemplateCommand::setUseStoredData(bool useStoredData)
{
    this->useStoredData = useStoredData;
    emit useStoredDataChanged(this->useStoredData);
    emit propertyChanged();
}

void TemplateCommand::setSendAsJson(bool sendAsJson)
{
    this->sendAsJson = sendAsJson;
    emit sendAsJsonChanged(this->sendAsJson);
    emit propertyChanged();
}

void TemplateCommand::setUseUppercaseData(bool useUppercaseData)
{
    this->useUppercaseData = useUppercaseData;
    emit useUppercaseDataChanged(this->useUppercaseData);
    emit propertyChanged();
}

void TemplateCommand::setTemplateName(const QString& templateName)
{
    this->templateName = templateName;
    emit templateNameChanged(this->templateName);
    emit propertyChanged();
}

void TemplateCommand::setTemplateDataModels(const QList<KeyValueModel>& models)
{
    this->models = models;
    emit templateDataChanged(this->models);
    emit propertyChanged();
}

void TemplateCommand::setTriggerOnNext(bool triggerOnNext)
{
    this->triggerOnNext = triggerOnNext;
    emit triggerOnNextChanged(this->triggerOnNext);
    emit propertyChanged();
}

bool TemplateCommand::getAutoPlay() const
{
    return this->autoPlay;
}

void TemplateCommand::setAutoPlay(bool autoPlay)
{
    this->autoPlay = autoPlay;
    emit autoPlayChanged(this->autoPlay);
    emit propertyChanged();
}

int TemplateCommand::getNewlineBehavior() const
{
    return this->newlineBehavior;
}

void TemplateCommand::setNewlineBehavior(int newlineBehavior)
{
    this->newlineBehavior = newlineBehavior;
    emit newlineBehaviorChanged(this->newlineBehavior);
    emit propertyChanged();
}

void TemplateCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setFlashlayer(pt.get(L"flashlayer", Template::DEFAULT_FLASHLAYER));

    // Read invokes: new format (invokes list) or old format (single invoke string).
    if (pt.count(L"invokes") > 0)
    {
        QStringList invokeList;
        for (const auto& value : pt.get_child(L"invokes"))
            invokeList.append(QString::fromStdWString(value.second.data()));
        setInvokes(invokeList);
        setInvokeHotkeyIndex(pt.get(L"invokehotkeyindex", 0));
    }
    else
    {
        QString singleInvoke = QString::fromStdWString(pt.get(L"invoke", Template::DEFAULT_INVOKE.toStdWString()));
        setInvokes(QStringList() << singleInvoke);
        setInvokeHotkeyIndex(0);
    }

    setUseStoredData(pt.get(L"usestoreddata", Template::DEFAULT_USE_STORED_DATA));
    setUseUppercaseData(pt.get(L"useuppercasedata", Template::DEFAULT_USE_UPPERCASE_DATA));
    setTriggerOnNext(pt.get(L"triggeronnext", Template::DEFAULT_TRIGGER_ON_NEXT));
    this->autoPlay = pt.get(L"autoplay", false);
    setSendAsJson(pt.get(L"sendasjson", Template::DEFAULT_SEND_AS_JSON));
    setNewlineBehavior(pt.get(L"newlinebehavior", Template::DEFAULT_NEWLINE_BEHAVIOR));

    if (pt.count(L"templatedata") > 0)
    {
        for (const boost::property_tree::wptree::value_type &value : pt.get_child(L"templatedata"))
        {
            this->models.push_back(KeyValueModel(QString::fromStdWString(value.second.get(L"id", L"")),
                                                 QString::fromStdWString(value.second.get(L"value", L"")),
                                                 value.second.get(L"mode", 0),
                                                 QString::fromStdWString(value.second.get(L"cyclevalues", L""))));
        }
    }

    if (pt.count(L"transform") > 0)
        m_transform.readProperties(pt.get_child(L"transform"));
}

void TemplateCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("flashlayer", QString::number(this->getFlashlayer()));

    // Write invokes list (new format).
    writer.writeStartElement("invokes");
    for (const QString& inv : this->invokes)
        writer.writeTextElement("invoke", inv);
    writer.writeEndElement();
    writer.writeTextElement("invokehotkeyindex", QString::number(this->invokeHotkeyIndex));

    writer.writeTextElement("usestoreddata", (getUseStoredData() == true) ? "true" : "false");
    writer.writeTextElement("useuppercasedata", (getUseUppercaseData() == true) ? "true" : "false");
    writer.writeTextElement("triggeronnext", (getTriggerOnNext() == true) ? "true" : "false");
    writer.writeTextElement("autoplay", (this->autoPlay == true) ? "true" : "false");
    writer.writeTextElement("sendasjson", (getSendAsJson() == true) ? "true" : "false");
    writer.writeTextElement("newlinebehavior", QString::number(this->getNewlineBehavior()));

    if (this->models.count() > 0)
    {
        writer.writeStartElement("templatedata");
        foreach (KeyValueModel model, this->models)
        {
            writer.writeStartElement("componentdata");
            writer.writeTextElement("id", model.getKey());
            writer.writeTextElement("value", model.getValue());
            if (model.getMode() != 0)
                writer.writeTextElement("mode", QString::number(model.getMode()));
            if (!model.getCycleValues().isEmpty())
                writer.writeTextElement("cyclevalues", model.getCycleValues());
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    m_transform.writeProperties(writer);
}
