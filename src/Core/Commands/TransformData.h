#pragma once

#include "../Shared.h"

#include <optional>

#include <boost/property_tree/ptree.hpp>

#include <QtCore/QString>
#include <QtCore/QXmlStreamWriter>

class CasparDevice;

class CORE_EXPORT TransformData
{
public:
    struct Fill { float x = 0; float y = 0; float scaleX = 1; float scaleY = 1; bool mipmap = false; };
    struct Clip { float x = 0; float y = 0; float width = 1; float height = 1; };
    struct Crop { float left = 0; float top = 0; float right = 1; float bottom = 1; };
    struct Anchor { float x = 0.5f; float y = 0.5f; };
    struct Perspective {
        float ulX = 0; float ulY = 0; float urX = 1; float urY = 0;
        float lrX = 1; float lrY = 1; float llX = 0; float llY = 1;
        bool mipmap = false;
    };
    struct Levels { float minIn = 0; float maxIn = 1; float gamma = 1; float minOut = 0; float maxOut = 1; };

    struct Entrance {
        QString property;   // "opacity", "fill", "rotation", etc.
        float from = 0;
        float to = 1;
        int duration = 0;   // frames
        QString tween = "Linear";
    };

    // All optional — only set values generate AMCP commands.
    std::optional<Fill> fill;
    std::optional<Clip> clip;
    std::optional<Crop> crop;
    std::optional<Anchor> anchor;
    std::optional<Perspective> perspective;
    std::optional<float> opacity;
    std::optional<float> rotation;
    std::optional<float> brightness;
    std::optional<float> contrast;
    std::optional<float> saturation;
    std::optional<float> volume;
    std::optional<Levels> levels;
    std::optional<int> blendMode;
    std::optional<bool> keyer;
    std::optional<Entrance> entrance;

    bool hasAnyTransform() const;
    void applyDeferred(CasparDevice* device, int channel, int videolayer);
    void commit(CasparDevice* device, int channel);
    void applyEntrance(CasparDevice* device, int channel, int videolayer);

    void readProperties(boost::property_tree::wptree& pt);
    void writeProperties(QXmlStreamWriter& writer) const;
    void clear();
};
