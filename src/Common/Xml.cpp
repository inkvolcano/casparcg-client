#include "Xml.h"

#include <QtCore/QString>

QString Xml::encode(const QString& data)
{
    QString temp;

    for (int index = 0; index < data.size(); index++)
    {
        QChar character(data.at(index));
        switch (character.unicode())
        {
            case '&':
                temp += "&amp;";
                break;
            case '\'':
                temp += "&apos;";
                break;
            case '"':
                temp += "&quot;";
                break;
            case '<':
                temp += "&lt;";
                break;
            case '>':
                temp += "&gt;";
                break;
            case '\n':
                temp += "&#10;";
                break;
            case '\r':
                temp += "&#13;";
                break;
            case '\t':
                temp += "&#9;";
                break;
            default:
                temp += character;
                break;
        }
    }

    return temp;
}

QString Xml::decode(const QString& data)
{
    QString temp(data);

    temp.replace("&amp;", "&");
    temp.replace("&apos;", "'");
    temp.replace("&quot;", "\"");
    temp.replace("&lt;", "<");
    temp.replace("&gt;", ">");
    temp.replace("&#10;", "\n");
    temp.replace("&#13;", "\r");
    temp.replace("&#9;", "\t");

    return temp;
}
