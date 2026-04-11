#include "ImageScrollerCommand.h"

#include <QtCore/QXmlStreamWriter>

ImageScrollerCommand::ImageScrollerCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

TransformData& ImageScrollerCommand::getTransform() { return this->m_transform; }
const TransformData& ImageScrollerCommand::getTransform() const { return this->m_transform; }

int ImageScrollerCommand::getBlur() const
{
    return this->blur;
}

int ImageScrollerCommand::getSpeed() const
{
    return this->speed;
}

bool ImageScrollerCommand::getPremultiply() const
{
    return this->premultiply;
}

bool ImageScrollerCommand::getProgressive() const
{
    return this->progressive;
}

const QString& ImageScrollerCommand::getImageScrollerName() const
{
    return this->imageScrollerName;
}

void ImageScrollerCommand::setImageScrollerName(const QString& imageScrollerName)
{
    this->imageScrollerName = imageScrollerName;
    emit imageScrollerNameChanged(this->imageScrollerName);
    emit propertyChanged();
}

void ImageScrollerCommand::setBlur(int blur)
{
    this->blur = blur;
    emit blurChanged(this->blur);
    emit propertyChanged();
}

void ImageScrollerCommand::setSpeed(int speed)
{
    this->speed = speed;
    emit speedChanged(this->speed);
    emit propertyChanged();
}

void ImageScrollerCommand::setPremultiply(bool premultiply)
{
    this->premultiply = premultiply;
    emit premultiplyChanged(this->premultiply);
    emit propertyChanged();
}

void ImageScrollerCommand::setProgressive(bool progressive)
{
    this->progressive = progressive;
    emit progressiveChanged(this->progressive);
    emit propertyChanged();
}

void ImageScrollerCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setBlur(pt.get(L"blur", ImageScroller::DEFAULT_BLUR));
    setSpeed(pt.get(L"speed", ImageScroller::DEFAULT_SPEED));
    setPremultiply(pt.get(L"premultiply", ImageScroller::DEFAULT_PREMULTIPLY));
    setProgressive(pt.get(L"progressive", ImageScroller::DEFAULT_PROGRESSIVE));

    if (pt.count(L"transform") > 0)
        m_transform.readProperties(pt.get_child(L"transform"));
}

void ImageScrollerCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("blur", QString::number(this->getBlur()));
    writer.writeTextElement("speed", QString::number(this->getSpeed()));
    writer.writeTextElement("premultiply", (getPremultiply() == true) ? "true" : "false");
    writer.writeTextElement("progressive", (getProgressive() == true) ? "true" : "false");

    m_transform.writeProperties(writer);
}
