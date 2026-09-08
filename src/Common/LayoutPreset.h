#pragma once

// Named layouts: saving the arrangement of the panels under a name, and putting
// it back later.
//
// A "layout" is not one setting. It is a couple of dozen configuration rows —
// which columns exist and in what order, which panels sit in each, and for every
// panel its size mode, height, span, span direction, collapsed state and anchor.
// Rearranging all of that for a different job and then rearranging it back is the
// thing this exists to stop.
//
// There are two independent arrangements, because the client already keeps two:
// the normal one, and the one Simple Mode uses while it is active. They are told
// apart by scope, and a preset saved in one is never offered in the other.
//
// One honest limit, worth stating where the code is rather than only in a
// changelog: the per-panel keys (PanelSizeMode_, PanelSpan_, and so on) are not
// namespaced by mode in this client — Simple Mode reads the same ones. So they
// belong to the normal scope, and a Simple Mode preset carries the column
// arrangement and the button grid rather than panel sizing. Splitting them would
// be a schema change reaching a long way past this feature.
//
// Header-only, like PanelFit.h and OgrafManifest.h, so the key sets and the
// round trip can be compiled and tested without linking the widgets.

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace LayoutPreset
{
    // Stored in the Scope column, so these strings are on disk in the user's
    // database and cannot be renamed without a migration.
    inline QString scopePanel() { return QString("panel"); }
    inline QString scopeSimple() { return QString("simple"); }

    inline bool isKnownScope(const QString& scope)
    {
        return scope == scopePanel() || scope == scopeSimple();
    }

    // Every panel the layout editor can place. Kept here rather than only in the
    // settings dialog so a preset and the editor cannot drift apart: a panel
    // missing from this list would silently not be saved.
    inline QStringList panelIds()
    {
        return QStringList()
            << "AudioLevels" << "Preview" << "Library" << "Inspector" << "ServerStatus"
            << "Activity" << "TriggerBanks" << "Live" << "NDI" << "Performance"
            << "HttpLog" << "Sheets" << "SimpleInspector" << "Clock" << "StatusBar";
    }

    // The configuration keys a preset of this scope owns. Anything not in here is
    // left alone when a preset is applied, which is what keeps loading a layout
    // from disturbing servers, hotkeys or anything else.
    inline QStringList keysFor(const QString& scope)
    {
        QStringList keys;

        if (scope == scopeSimple())
        {
            keys << "SimpleLayoutColumnOrder"
                 << "SimpleLayoutPanel1" << "SimpleLayoutPanel2"
                 << "SimpleLayoutPanel3" << "SimpleLayoutPanel4"
                 << "SimpleModeColumns" << "SimpleModeRows"
                 << "SimpleModeShowPlayStop" << "SimpleModeShowPreview";

            return keys;
        }

        if (scope != scopePanel())
            return keys;

        keys << "LayoutColumnOrder"
             << "LayoutPanel1" << "LayoutPanel2" << "LayoutPanel3" << "LayoutPanel4"
             << "ShowEmptyPanels";

        const QStringList panels = panelIds();
        for (const QString& panel : panels)
        {
            keys << "PanelSizeMode_" + panel
                 << "PanelCollapsed_" + panel
                 << "PanelSpan_" + panel
                 << "PanelSpanDir_" + panel
                 << "PanelAnchor_" + panel
                 << panel + "PanelHeight";
        }

        return keys;
    }

    inline QString serialise(const QMap<QString, QString>& values)
    {
        QJsonObject object;

        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
            object.insert(it.key(), it.value());

        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }

    // Reads a stored preset back. Only keys the scope owns are returned, so a
    // preset written by a later version carrying keys this one does not know
    // about is applied as far as it can be rather than refused.
    inline QMap<QString, QString> deserialise(const QString& json, const QString& scope)
    {
        QMap<QString, QString> values;

        const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
        if (!document.isObject())
            return values;

        const QJsonObject object = document.object();
        const QStringList owned = keysFor(scope);

        for (const QString& key : owned)
        {
            if (!object.contains(key))
                continue;

            const QJsonValue value = object.value(key);

            // Everything in the Configuration table is text. A preset written by
            // hand might not be, so anything that is not a string is skipped
            // rather than turned into "true" or "1" and written back as a
            // setting nobody chose.
            if (value.isString())
                values.insert(key, value.toString());
        }

        return values;
    }

    // A name the operator typed, made safe to store and show. Empty means they
    // gave nothing usable and the caller should decline rather than invent one.
    inline QString sanitiseName(const QString& name)
    {
        QString cleaned = name.simplified();

        // Control characters would break the list widget and the combo box; the
        // cap is so a pasted paragraph cannot become a name.
        cleaned.remove(QRegularExpression("[\\x00-\\x1F\\x7F]"));

        if (cleaned.length() > 60)
            cleaned = cleaned.left(60).trimmed();

        return cleaned;
    }

    // Names are compared case-insensitively, because two presets called "Studio"
    // and "studio" in one list is a trap rather than a feature.
    inline bool sameName(const QString& left, const QString& right)
    {
        return QString::compare(left.trimmed(), right.trimmed(), Qt::CaseInsensitive) == 0;
    }
}
