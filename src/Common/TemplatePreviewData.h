#pragma once

// What the Preview panel hands a CasparCG template, and how it fits the page to
// the panel.
//
// Template preview did not work properly, for reasons that are all here:
//
//   - The data never arrived. update() was called with a JavaScript object, but a
//     CasparCG template's update(data) is written for what the server sends: a
//     string, either <templateData> XML or JSON, which it checks and parses. An
//     object fails JSON.parse, the template catches it, and nothing changes.
//   - It was not the item's data. It was the template's own window.debugData
//     sample, not the fields the operator filled in on the rundown item.
//   - It was the wrong size. A template is laid out for the channel - 1920x1080
//     almost always - and was loaded at the size of the panel, so the preview
//     showed a corner of the graphic.
//
// The string is built the way TemplateCommand::getTemplateData builds it for the
// server - same JSON or XML choice, uppercase and newline handling - without the
// extra escaping AMCP needs on top. tools/test-templatepreview checks it.

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace TemplatePreviewData
{
    // The XML escaping Xml::encode applies to a value on air.
    inline QString xmlEncode(const QString& value)
    {
        QString out;
        out.reserve(value.size());
        for (const QChar c : value)
        {
            switch (c.unicode())
            {
                case '&':  out += "&amp;"; break;
                case '\'': out += "&apos;"; break;
                case '"':  out += "&quot;"; break;
                case '<':  out += "&lt;"; break;
                case '>':  out += "&gt;"; break;
                case '\n': out += "&#10;"; break;
                case '\r': out += "&#13;"; break;
                case '\t': out += "&#9;"; break;
                default:   out += c; break;
            }
        }
        return out;
    }

    // newlineBehavior as TemplateCommand has it: 0 strip, 1 keep (innerText),
    // 2 <br> (innerHTML).
    inline QString applyNewlines(QString value, int newlineBehavior)
    {
        if (newlineBehavior == 0)
        {
            value.replace("\r\n", "");
            value.replace("\n", "");
            value.replace("\r", "");
        }
        else if (newlineBehavior == 2)
        {
            value.replace("\r\n", "<br>");
            value.replace("\n", "<br>");
            value.replace("\r", "<br>");
        }
        return value;
    }

    // The data string for a template, from an item's fields.
    inline QString dataString(const QList<QPair<QString, QString>>& fields, bool asJson, bool uppercase, int newlineBehavior)
    {
        if (asJson)
        {
            QJsonObject object;
            for (const auto& field : fields)
                object[field.first] = uppercase ? field.second.toUpper() : field.second;

            return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
        }

        QString xml = "<templateData>";
        for (const auto& field : fields)
        {
            QString value = xmlEncode(applyNewlines(field.second, newlineBehavior));
            if (uppercase)
                value = value.toUpper();

            xml += QString("<componentData id=\"%1\"><data id=\"text\" value=\"%2\"/></componentData>").arg(field.first, value);
        }
        xml += "</templateData>";
        return xml;
    }

    // A template's window.debugData object body, as a JSON string the template can
    // parse. Written with double quotes it already is JSON; single quotes are the
    // other way templates write it. Empty when it is neither.
    inline QString debugDataAsJson(const QString& objectText)
    {
        QJsonParseError error;
        QJsonDocument document = QJsonDocument::fromJson(objectText.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            QString doubled = objectText;
            doubled.replace('\'', '"');
            document = QJsonDocument::fromJson(doubled.toUtf8(), &error);
            if (error.error != QJsonParseError::NoError || !document.isObject())
                return QString();
        }

        return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    }

    // A string as a JavaScript string literal, quotes and all, safe to put into a
    // script: every quote, backslash, line break and non-ASCII character escaped.
    inline QString jsString(const QString& value)
    {
        const QString array = QString::fromUtf8(QJsonDocument(QJsonArray{ value }).toJson(QJsonDocument::Compact));
        QString literal = array.mid(1, array.size() - 2);   // ["..."] -> "..."

        // JSON allows these two raw; a JavaScript string literal before ES2019 does not.
        literal.replace(QChar(0x2028), "\\u2028");
        literal.replace(QChar(0x2029), "\\u2029");
        return literal;
    }

    // How a page designed at designWidth x designHeight fits a panel: scaled to
    // fit whole, keeping its shape, centred.
    struct Fit
    {
        double scale = 1.0;
        double x = 0.0;
        double y = 0.0;
    };

    inline Fit fit(int panelWidth, int panelHeight, int designWidth, int designHeight)
    {
        Fit result;
        if (panelWidth <= 0 || panelHeight <= 0 || designWidth <= 0 || designHeight <= 0)
            return result;

        const double sx = double(panelWidth) / double(designWidth);
        const double sy = double(panelHeight) / double(designHeight);
        result.scale = (sx < sy) ? sx : sy;
        result.x = (panelWidth - designWidth * result.scale) / 2.0;
        result.y = (panelHeight - designHeight * result.scale) / 2.0;
        return result;
    }

    // The script that lays the page out at its design size and scales it into the
    // panel. A CSS transform rather than the view's zoom factor: that stops at 25 %,
    // which is still too large for a panel a few hundred pixels wide. The
    // template's own layout and window size are left as they are.
    inline QString fitScript(const Fit& f, int designWidth, int designHeight)
    {
        return QString(
            "(function () {"
            " var id = 'casparcg-preview-fit';"
            " var style = document.getElementById(id);"
            " if (!style) { style = document.createElement('style'); style.id = id;"
            "   (document.head || document.documentElement).appendChild(style); }"
            " style.textContent = 'html { width: %1px; height: %2px; overflow: hidden;"
            " transform-origin: 0 0; transform: translate(%3px, %4px) scale(%5); }"
            " body { overflow: hidden; }';"
            "})();")
            .arg(designWidth).arg(designHeight)
            .arg(f.x, 0, 'f', 2).arg(f.y, 0, 'f', 2).arg(f.scale, 0, 'f', 5);
    }
}
