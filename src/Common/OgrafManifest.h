#pragma once

// Reading an EBU OGraf graphic's manifest.
//
// OGraf is an open specification for HTML broadcast graphics, published by the
// EBU. A graphic is a folder holding a "<something>.ograf.json" manifest, a
// Javascript module whose default export is a class extending HTMLElement, and
// whatever resources it needs. A renderer loads the module, puts the element in
// the DOM, and drives it with load(), playAction(), updateAction(), stopAction()
// and customAction().
//
// This header does the part that is pure data: turning the manifest into
// something the client can act on, and working out the data a graphic should be
// given when nobody has typed anything. That second part matters more than it
// looks. A CasparCG template in this fork carries sample values in a private
// window.debugData block; OGraf carries the same idea as a standard JSON Schema
// in the manifest, where each property may declare a "default". Reading those
// defaults is what lets a graphic preview populated rather than empty, using a
// convention every OGraf tool already speaks rather than one only this fork does.
//
// Header-only, like PanelFit.h and AudioLevelTrack.h, so it can be compiled and
// tested without linking the widgets.
//
// Spec: https://ograf.ebu.io/  (v1 specification, Manifest Model)

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace Ograf
{
    // A manifest file is identified by its name, not by its content: the spec
    // says the name MUST end with ".ograf.json", and a folder may hold several,
    // each a separate graphic sharing the folder's resources.
    inline bool isManifestFileName(const QString& fileName)
    {
        return fileName.endsWith(".ograf.json", Qt::CaseInsensitive)
            && fileName.length() > QString(".ograf.json").length();
    }

    struct CustomAction
    {
        QString id;
        QString name;
    };

    struct Manifest
    {
        bool valid = false;
        QString error;

        QString id;
        QString name;
        QString description;
        QString version;
        QString main;

        // Defaults from the spec: a graphic triggered by one play and one stop is
        // a stepCount of 1, and -1 means dynamic or unknown.
        int stepCount = 1;

        bool supportsRealTime = false;
        bool supportsNonRealTime = false;

        QJsonObject schema;
        QVector<CustomAction> customActions;
        QStringList thumbnails;
    };

    // Builds the data object to hand a graphic when nobody has typed anything,
    // by walking the manifest's schema and taking each property's "default".
    //
    // A property with no default is left out rather than guessed at: the spec
    // treats the data as the graphic's own state model, and inventing an empty
    // string for a field the author deliberately left undefaulted would put the
    // graphic into a state its author never described.
    inline QJsonObject defaultDataFor(const QJsonObject& schema)
    {
        QJsonObject data;

        const QJsonValue propertiesValue = schema.value("properties");
        if (!propertiesValue.isObject())
            return data;

        const QJsonObject properties = propertiesValue.toObject();

        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it)
        {
            if (!it.value().isObject())
                continue;

            const QJsonObject property = it.value().toObject();

            if (property.contains("default"))
            {
                data.insert(it.key(), property.value("default"));
                continue;
            }

            // A nested object can carry defaults of its own. It is only included
            // when something inside it actually had one, so an object of entirely
            // undefaulted fields does not arrive as a meaningless empty object.
            if (property.value("type").toString() == "object")
            {
                const QJsonObject nested = defaultDataFor(property);
                if (!nested.isEmpty())
                    data.insert(it.key(), nested);
            }
        }

        return data;
    }

    inline Manifest parse(const QByteArray& json)
    {
        Manifest manifest;

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            manifest.error = QString("The manifest is not valid JSON: %1").arg(parseError.errorString());
            return manifest;
        }

        if (!document.isObject())
        {
            manifest.error = "The manifest is not a JSON object.";
            return manifest;
        }

        const QJsonObject root = document.object();

        manifest.id = root.value("id").toString();
        manifest.name = root.value("name").toString();
        manifest.description = root.value("description").toString();
        manifest.version = root.value("version").toString();
        manifest.main = root.value("main").toString();

        // id, name and main are the three the spec marks required, and each is
        // needed to do anything at all: without main there is nothing to load,
        // and without an id two graphics cannot be told apart.
        if (manifest.id.isEmpty())
        {
            manifest.error = "The manifest has no \"id\".";
            return manifest;
        }

        if (manifest.main.isEmpty())
        {
            manifest.error = "The manifest has no \"main\", so there is no code to load.";
            return manifest;
        }

        // An id may hold any character except a forward slash, and main is a path
        // that is resolved against the graphic's own folder. Neither may climb out
        // of it, whatever a manifest from elsewhere asks for.
        if (manifest.id.contains('/'))
        {
            manifest.error = "The manifest \"id\" may not contain a forward slash.";
            return manifest;
        }

        if (manifest.main.contains("..") || manifest.main.startsWith('/') || manifest.main.startsWith('\\')
            || manifest.main.contains(':'))
        {
            manifest.error = "The manifest \"main\" must stay inside the graphic's own folder.";
            return manifest;
        }

        if (manifest.name.isEmpty())
            manifest.name = manifest.id;

        if (root.contains("stepCount"))
        {
            const QJsonValue stepCount = root.value("stepCount");
            if (stepCount.isDouble())
                manifest.stepCount = stepCount.toInt(1);
        }

        manifest.supportsRealTime = root.value("supportsRealTime").toBool(false);
        manifest.supportsNonRealTime = root.value("supportsNonRealTime").toBool(false);

        if (root.value("schema").isObject())
            manifest.schema = root.value("schema").toObject();

        const QJsonValue actions = root.value("customActions");
        if (actions.isArray())
        {
            for (const QJsonValue& value : actions.toArray())
            {
                if (!value.isObject())
                    continue;

                const QJsonObject action = value.toObject();
                const QString actionId = action.value("id").toString();

                if (actionId.isEmpty())
                    continue;

                CustomAction custom;
                custom.id = actionId;
                custom.name = action.value("name").toString();
                if (custom.name.isEmpty())
                    custom.name = actionId;

                manifest.customActions.append(custom);
            }
        }

        const QJsonValue thumbnails = root.value("thumbnails");
        if (thumbnails.isArray())
        {
            for (const QJsonValue& value : thumbnails.toArray())
            {
                if (!value.isObject())
                    continue;

                const QString file = value.toObject().value("file").toString();

                // Same rule as main: a thumbnail path is joined to the graphic's
                // folder and must not be able to leave it.
                if (file.isEmpty() || file.contains("..") || file.startsWith('/') || file.contains(':'))
                    continue;

                manifest.thumbnails.append(file);
            }
        }

        manifest.valid = true;

        return manifest;
    }
}
