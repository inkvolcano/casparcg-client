#include "TransformData.h"

#include "CasparDevice.h"

bool TransformData::hasAnyTransform() const
{
    return fill.has_value() || clip.has_value() || crop.has_value() ||
           anchor.has_value() || perspective.has_value() ||
           opacity.has_value() || rotation.has_value() ||
           brightness.has_value() || contrast.has_value() ||
           saturation.has_value() || volume.has_value() ||
           levels.has_value() || blendMode.has_value() ||
           keyer.has_value();
}

void TransformData::applyDeferred(CasparDevice* device, int channel, int videolayer)
{
    if (device == nullptr)
        return;

    // Each property is either applied from the embedded value or reset to its
    // CasparCG default.  This prevents stale MIXER state from a previous item
    // leaking into the next one on the same layer.

    if (fill.has_value())
        device->setFill(channel, videolayer, fill->x, fill->y, fill->scaleX, fill->scaleY, 0, "Linear", true, fill->mipmap);
    else
        device->setFill(channel, videolayer, 0, 0, 1, 1, 0, "Linear", true, false);

    if (clip.has_value())
        device->setClipping(channel, videolayer, clip->x, clip->y, clip->width, clip->height, true);
    else
        device->setClipping(channel, videolayer, 0, 0, 1, 1, true);

    if (crop.has_value())
        device->setCrop(channel, videolayer, crop->left, crop->top, crop->right, crop->bottom, true);
    else
        device->setCrop(channel, videolayer, 0, 0, 1, 1, true);

    // Anchor must be sent before rotation — it sets the pivot point.
    // If rotation is set but anchor isn't, default to center (0.5, 0.5).
    if (anchor.has_value())
        device->setAnchor(channel, videolayer, anchor->x, anchor->y, true);
    else if (rotation.has_value())
        device->setAnchor(channel, videolayer, 0.5f, 0.5f, true);
    else
        device->setAnchor(channel, videolayer, 0, 0, true);

    if (rotation.has_value())
        device->setRotation(channel, videolayer, *rotation, true);
    else
        device->setRotation(channel, videolayer, 0, true);

    if (perspective.has_value())
        device->setPerspective(channel, videolayer,
                               perspective->ulX, perspective->ulY,
                               perspective->urX, perspective->urY,
                               perspective->lrX, perspective->lrY,
                               perspective->llX, perspective->llY,
                               true, perspective->mipmap);
    else
        device->setPerspective(channel, videolayer, 0, 0, 1, 0, 1, 1, 0, 1, true, false);

    if (opacity.has_value())
        device->setOpacity(channel, videolayer, *opacity, true);
    else
        device->setOpacity(channel, videolayer, 1, true);

    if (brightness.has_value())
        device->setBrightness(channel, videolayer, *brightness, true);
    else
        device->setBrightness(channel, videolayer, 1, true);

    if (contrast.has_value())
        device->setContrast(channel, videolayer, *contrast, true);
    else
        device->setContrast(channel, videolayer, 1, true);

    if (saturation.has_value())
        device->setSaturation(channel, videolayer, *saturation, true);
    else
        device->setSaturation(channel, videolayer, 1, true);

    if (volume.has_value())
        device->setVolume(channel, videolayer, *volume, true);
    else
        device->setVolume(channel, videolayer, 1, true);

    if (levels.has_value())
        device->setLevels(channel, videolayer, levels->minIn, levels->maxIn, levels->gamma, levels->minOut, levels->maxOut, true);
    else
        device->setLevels(channel, videolayer, 0, 1, 1, 0, 1, true);

    if (blendMode.has_value())
    {
        static const QStringList blendNames = {
            "Normal", "Lighten", "Darken", "Multiply", "Average",
            "Add", "Subtract", "Difference", "Negation", "Exclusion",
            "Screen", "Overlay", "SoftLight", "HardLight", "ColorDodge",
            "ColorBurn", "LinearDodge", "LinearBurn", "LinearLight",
            "VividLight", "PinLight", "HardMix", "Reflect", "Glow",
            "Phoenix", "Contrast", "Saturation", "Color", "Luminosity"
        };
        int idx = *blendMode;
        if (idx >= 0 && idx < blendNames.size())
            device->setBlendMode(channel, videolayer, blendNames[idx]);
    }
    else
    {
        device->setBlendMode(channel, videolayer, "Normal");
    }

    if (keyer.has_value())
        device->setKeyer(channel, videolayer, *keyer ? 1 : 0, true);
    else
        device->setKeyer(channel, videolayer, 0, true);
}

void TransformData::commit(CasparDevice* device, int channel)
{
    if (device != nullptr)
        device->setCommit(channel);
}

void TransformData::applyEntrance(CasparDevice* device, int channel, int videolayer)
{
    if (device == nullptr || !entrance.has_value() || entrance->duration <= 0)
        return;

    const Entrance& e = *entrance;

    if (e.property == "opacity")
        device->setOpacity(channel, videolayer, e.to, e.duration, e.tween, false);
    else if (e.property == "rotation")
        device->setRotation(channel, videolayer, e.to, e.duration, e.tween, false);
    else if (e.property == "brightness")
        device->setBrightness(channel, videolayer, e.to, e.duration, e.tween, false);
    else if (e.property == "contrast")
        device->setContrast(channel, videolayer, e.to, e.duration, e.tween, false);
    else if (e.property == "saturation")
        device->setSaturation(channel, videolayer, e.to, e.duration, e.tween, false);
    else if (e.property == "volume")
        device->setVolume(channel, videolayer, e.to, e.duration, e.tween, false);
}

void TransformData::readProperties(boost::property_tree::wptree& pt)
{
    if (pt.count(L"fill") > 0)
    {
        auto& f = pt.get_child(L"fill");
        Fill v;
        v.x = f.get(L"x", 0.0f);
        v.y = f.get(L"y", 0.0f);
        v.scaleX = f.get(L"scalex", 1.0f);
        v.scaleY = f.get(L"scaley", 1.0f);
        v.mipmap = f.get(L"mipmap", false);
        fill = v;
    }

    if (pt.count(L"clip") > 0)
    {
        auto& c = pt.get_child(L"clip");
        Clip v;
        v.x = c.get(L"x", 0.0f);
        v.y = c.get(L"y", 0.0f);
        v.width = c.get(L"width", 1.0f);
        v.height = c.get(L"height", 1.0f);
        clip = v;
    }

    if (pt.count(L"crop") > 0)
    {
        auto& c = pt.get_child(L"crop");
        Crop v;
        v.left = c.get(L"left", 0.0f);
        v.top = c.get(L"top", 0.0f);
        v.right = c.get(L"right", 1.0f);
        v.bottom = c.get(L"bottom", 1.0f);
        crop = v;
    }

    if (pt.count(L"anchor") > 0)
    {
        auto& a = pt.get_child(L"anchor");
        Anchor v;
        v.x = a.get(L"x", 0.5f);
        v.y = a.get(L"y", 0.5f);
        anchor = v;
    }

    if (pt.count(L"perspective") > 0)
    {
        auto& p = pt.get_child(L"perspective");
        Perspective v;
        v.ulX = p.get(L"ulx", 0.0f); v.ulY = p.get(L"uly", 0.0f);
        v.urX = p.get(L"urx", 1.0f); v.urY = p.get(L"ury", 0.0f);
        v.lrX = p.get(L"lrx", 1.0f); v.lrY = p.get(L"lry", 1.0f);
        v.llX = p.get(L"llx", 0.0f); v.llY = p.get(L"lly", 1.0f);
        v.mipmap = p.get(L"mipmap", false);
        perspective = v;
    }

    if (pt.count(L"opacity") > 0)
        opacity = pt.get(L"opacity", 1.0f);

    if (pt.count(L"rotation") > 0)
        rotation = pt.get(L"rotation", 0.0f);

    if (pt.count(L"brightness") > 0)
        brightness = pt.get(L"brightness", 1.0f);

    if (pt.count(L"contrast") > 0)
        contrast = pt.get(L"contrast", 1.0f);

    if (pt.count(L"saturation") > 0)
        saturation = pt.get(L"saturation", 1.0f);

    if (pt.count(L"volume") > 0)
        volume = pt.get(L"volume", 1.0f);

    if (pt.count(L"levels") > 0)
    {
        auto& l = pt.get_child(L"levels");
        Levels v;
        v.minIn = l.get(L"minin", 0.0f);
        v.maxIn = l.get(L"maxin", 1.0f);
        v.gamma = l.get(L"gamma", 1.0f);
        v.minOut = l.get(L"minout", 0.0f);
        v.maxOut = l.get(L"maxout", 1.0f);
        levels = v;
    }

    if (pt.count(L"blendmode") > 0)
        blendMode = pt.get(L"blendmode", 0);

    if (pt.count(L"keyer") > 0)
        keyer = pt.get(L"keyer", false);

    if (pt.count(L"entrance") > 0)
    {
        auto& e = pt.get_child(L"entrance");
        Entrance v;
        v.property = QString::fromStdWString(e.get(L"property", L"opacity"));
        v.from = e.get(L"from", 0.0f);
        v.to = e.get(L"to", 1.0f);
        v.duration = e.get(L"duration", 0);
        v.tween = QString::fromStdWString(e.get(L"tween", L"Linear"));
        entrance = v;
    }
}

void TransformData::writeProperties(QXmlStreamWriter& writer) const
{
    if (!hasAnyTransform() && !entrance.has_value())
        return;

    writer.writeStartElement("transform");

    if (fill.has_value())
    {
        writer.writeStartElement("fill");
        writer.writeTextElement("x", QString::number(fill->x));
        writer.writeTextElement("y", QString::number(fill->y));
        writer.writeTextElement("scalex", QString::number(fill->scaleX));
        writer.writeTextElement("scaley", QString::number(fill->scaleY));
        if (fill->mipmap)
            writer.writeTextElement("mipmap", "true");
        writer.writeEndElement();
    }

    if (clip.has_value())
    {
        writer.writeStartElement("clip");
        writer.writeTextElement("x", QString::number(clip->x));
        writer.writeTextElement("y", QString::number(clip->y));
        writer.writeTextElement("width", QString::number(clip->width));
        writer.writeTextElement("height", QString::number(clip->height));
        writer.writeEndElement();
    }

    if (crop.has_value())
    {
        writer.writeStartElement("crop");
        writer.writeTextElement("left", QString::number(crop->left));
        writer.writeTextElement("top", QString::number(crop->top));
        writer.writeTextElement("right", QString::number(crop->right));
        writer.writeTextElement("bottom", QString::number(crop->bottom));
        writer.writeEndElement();
    }

    if (anchor.has_value())
    {
        writer.writeStartElement("anchor");
        writer.writeTextElement("x", QString::number(anchor->x));
        writer.writeTextElement("y", QString::number(anchor->y));
        writer.writeEndElement();
    }

    if (perspective.has_value())
    {
        writer.writeStartElement("perspective");
        writer.writeTextElement("ulx", QString::number(perspective->ulX));
        writer.writeTextElement("uly", QString::number(perspective->ulY));
        writer.writeTextElement("urx", QString::number(perspective->urX));
        writer.writeTextElement("ury", QString::number(perspective->urY));
        writer.writeTextElement("lrx", QString::number(perspective->lrX));
        writer.writeTextElement("lry", QString::number(perspective->lrY));
        writer.writeTextElement("llx", QString::number(perspective->llX));
        writer.writeTextElement("lly", QString::number(perspective->llY));
        if (perspective->mipmap)
            writer.writeTextElement("mipmap", "true");
        writer.writeEndElement();
    }

    if (opacity.has_value())
        writer.writeTextElement("opacity", QString::number(*opacity));

    if (rotation.has_value())
        writer.writeTextElement("rotation", QString::number(*rotation));

    if (brightness.has_value())
        writer.writeTextElement("brightness", QString::number(*brightness));

    if (contrast.has_value())
        writer.writeTextElement("contrast", QString::number(*contrast));

    if (saturation.has_value())
        writer.writeTextElement("saturation", QString::number(*saturation));

    if (volume.has_value())
        writer.writeTextElement("volume", QString::number(*volume));

    if (levels.has_value())
    {
        writer.writeStartElement("levels");
        writer.writeTextElement("minin", QString::number(levels->minIn));
        writer.writeTextElement("maxin", QString::number(levels->maxIn));
        writer.writeTextElement("gamma", QString::number(levels->gamma));
        writer.writeTextElement("minout", QString::number(levels->minOut));
        writer.writeTextElement("maxout", QString::number(levels->maxOut));
        writer.writeEndElement();
    }

    if (blendMode.has_value())
        writer.writeTextElement("blendmode", QString::number(*blendMode));

    if (keyer.has_value())
        writer.writeTextElement("keyer", *keyer ? "true" : "false");

    if (entrance.has_value())
    {
        writer.writeStartElement("entrance");
        writer.writeTextElement("property", entrance->property);
        writer.writeTextElement("from", QString::number(entrance->from));
        writer.writeTextElement("to", QString::number(entrance->to));
        writer.writeTextElement("duration", QString::number(entrance->duration));
        writer.writeTextElement("tween", entrance->tween);
        writer.writeEndElement();
    }

    writer.writeEndElement(); // transform
}

void TransformData::clear()
{
    fill.reset();
    clip.reset();
    crop.reset();
    anchor.reset();
    perspective.reset();
    opacity.reset();
    rotation.reset();
    brightness.reset();
    contrast.reset();
    saturation.reset();
    volume.reset();
    levels.reset();
    blendMode.reset();
    keyer.reset();
    entrance.reset();
}
