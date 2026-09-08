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

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVector>

namespace Ograf
{
    // The one switch. Off by default, and every entry point asks first: the
    // Library walk, the Preview panel's lookup and the Inspector's field
    // discovery. A client that does not use OGraf does none of that work.
    //
    // Declared here rather than in a widget because all three ask, and three
    // copies of a default is how a feature ends up half-on.
    inline bool isEnabledIn(const QString& settingValue)
    {
        return settingValue.trimmed() == "true";
    }

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

    // ---- fields ----------------------------------------------------------
    //
    // The edit modes the Inspector's key/value table understands. They are ints
    // on the tree item rather than an enum on the wire, so the numbers matter:
    // this is the same mapping window.debugDataModes produces.
    namespace Mode
    {
        const int Text = 0;
        const int Integer = 1;
        const int Decimal = 2;
        const int Boolean = 3;
        const int Color = 4;
        const int Cycle = 5;
    }

    struct Field
    {
        QString key;          // dotted path, so a nested object flattens onto one row
        QString label;        // the schema's "title", or the key when it has none
        QString value;        // the default rendered for the table
        bool hasDefault = false;
        int mode = Mode::Text;
        QString cycleValues;  // "left|center|right" for a select
    };

    // A default value as the key/value table wants it: a string. The table sends
    // these to a template as text, so this is a rendering, not a serialisation —
    // a boolean has to arrive as "true", not as "1".
    inline QString renderDefault(const QJsonValue& value)
    {
        switch (value.type())
        {
            case QJsonValue::Bool:
                return value.toBool() ? "true" : "false";

            case QJsonValue::String:
                return value.toString();

            case QJsonValue::Double:
            {
                const double number = value.toDouble();

                // A whole number is written without a decimal point, because
                // "2.0" in a field the graphic reads as an integer looks wrong to
                // an operator and can read wrong to the template.
                if (number == static_cast<double>(static_cast<qlonglong>(number)))
                    return QString::number(static_cast<qlonglong>(number));

                return QString::number(number);
            }

            case QJsonValue::Array:
            case QJsonValue::Object:
                return QString::fromUtf8(
                    QJsonDocument::fromVariant(value.toVariant()).toJson(QJsonDocument::Compact)).trimmed();

            default:
                return QString();
        }
    }

    // Which editor a property wants, from JSON Schema plus GDD's gddType
    // extension. GDD is what OGraf uses to say "this string is a colour" or
    // "this number is one of these five" — without it every field would be a
    // text box, which is the thing typed fields exist to avoid.
    inline int modeFor(const QJsonObject& property)
    {
        const QString gddType = property.value("gddType").toString();
        const QString type = property.value("type").toString();

        if (gddType.startsWith("color-"))
            return Mode::Color;

        // An enum is a choice whether or not GDD named it one.
        if (property.value("enum").isArray() || gddType == "select")
            return Mode::Cycle;

        if (type == "boolean")
            return Mode::Boolean;

        if (type == "integer" || gddType == "integer" || gddType == "duration-ms")
            return Mode::Integer;

        if (type == "number" || gddType == "number" || gddType == "percentage")
            return Mode::Decimal;

        return Mode::Text;
    }

    // The choices for a select, in the order the schema lists them.
    inline QString cycleValuesFor(const QJsonObject& property)
    {
        const QJsonValue enumValue = property.value("enum");
        if (!enumValue.isArray())
            return QString();

        QStringList values;
        for (const QJsonValue& value : enumValue.toArray())
        {
            const QString rendered = renderDefault(value);
            if (!rendered.isEmpty())
                values.append(rendered);
        }

        // The table stores the choices as one pipe-separated string, so a choice
        // containing a pipe would silently become two. Those are dropped rather
        // than allowed to corrupt the row.
        values.removeIf([](const QString& value) { return value.contains('|'); });

        return values.join("|");
    }

    // Every editable field in a manifest's schema, flattened.
    //
    // A CasparCG template's data is a flat set of key/value pairs, and an OGraf
    // schema may nest objects, so a nested property becomes "parent.child". That
    // is a real mapping decision rather than a shortcut: it keeps one row per
    // editable thing, which is what the table is, and the dotted key is what a
    // template author would write anyway.
    inline QVector<Field> fieldsFor(const QJsonObject& schema, const QString& prefix = QString())
    {
        QVector<Field> fields;

        const QJsonValue propertiesValue = schema.value("properties");
        if (!propertiesValue.isObject())
            return fields;

        const QJsonObject properties = propertiesValue.toObject();

        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it)
        {
            if (!it.value().isObject())
                continue;

            const QJsonObject property = it.value().toObject();
            const QString key = prefix.isEmpty() ? it.key() : (prefix + "." + it.key());

            // An object with properties is a group of fields, not a field. One
            // without properties is treated as a value, because there is nothing
            // to descend into and a row is better than silently losing it.
            if (property.value("type").toString() == "object" && property.value("properties").isObject())
            {
                fields += fieldsFor(property, key);
                continue;
            }

            Field field;
            field.key = key;
            field.label = property.value("title").toString();
            if (field.label.isEmpty())
                field.label = it.key();

            field.mode = modeFor(property);
            field.cycleValues = cycleValuesFor(property);

            if (property.contains("default"))
            {
                field.hasDefault = true;
                field.value = renderDefault(property.value("default"));
            }

            fields.append(field);
        }

        return fields;
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

    // Where a graphic's manifest lives, given a template folder and the name an
    // item carries. Two shapes are accepted because both are how people store
    // them: the name pointing straight at "<name>.ograf.json", or the name being
    // a folder that holds exactly one manifest.
    //
    // A folder holding several is left alone on purpose. The spec allows it, and
    // it means several independent graphics sharing resources, so there is no
    // single right answer and guessing one would preview the wrong graphic.
    inline QString findManifest(const QString& templatePath, const QString& name)
    {
        if (templatePath.isEmpty() || name.isEmpty())
            return QString();

        QString baseName = name;
        baseName.replace(QChar(0x5C), QChar('/'));

        // The name comes off a rundown item, so it must not be able to reach out
        // of the template folder.
        if (baseName.contains("..") || baseName.startsWith('/') || baseName.contains(':'))
            return QString();

        const QString direct = QDir(templatePath).filePath(baseName + ".ograf.json");
        if (QFileInfo::exists(direct))
            return direct;

        QDir folder(QDir(templatePath).filePath(baseName));
        if (folder.exists())
        {
            QStringList manifests;
            const QStringList entries = folder.entryList(QDir::Files, QDir::Name);
            for (const QString& entry : entries)
            {
                if (isManifestFileName(entry))
                    manifests.append(entry);
            }

            if (manifests.size() == 1)
                return folder.filePath(manifests.first());
        }

        return QString();
    }

    // Reads and parses the manifest at this path.
    inline Manifest load(const QString& manifestPath)
    {
        Manifest manifest;

        QFile file(manifestPath);
        if (!file.open(QIODevice::ReadOnly))
        {
            manifest.error = "The manifest could not be opened.";
            return manifest;
        }

        const QByteArray content = file.readAll();
        file.close();

        return parse(content);
    }
}
