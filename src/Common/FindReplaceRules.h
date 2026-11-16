#pragma once

// Find and replace in rundowns (build 310): the text rules, kept apart from the
// widgets so tools/test-findreplace can check them.
//
// The replacement is literal: no pattern characters in what is looked for, and
// no back-references in what replaces it - an operator types "\1" or "$1" and
// gets exactly that. Whole words means the match does not touch a letter, digit
// or underscore on either side.

#include <QtCore/QList>
#include <QtCore/QString>

namespace FindReplaceRules
{
    // The kinds of text that can be searched, each its own tick box.
    enum Kind
    {
        Label = 1,          // the item's label in the rundown
        FieldValue = 2,     // a template field's value
        Note = 4,           // the operator note (299)
        Url = 8,            // an HTML item's page address
        AmcpCommand = 16,   // a custom command item's AMCP lines
        ShellLine = 32      // a shell command item's command line
    };

    inline QString kindText(Kind kind)
    {
        switch (kind)
        {
            case Label: return "Label";
            case FieldValue: return "Field value";
            case Note: return "Note";
            case Url: return "Page address";
            case AmcpCommand: return "AMCP command";
            default: return "Shell command";
        }
    }

    struct Options
    {
        bool matchCase = false;
        bool wholeWords = false;
    };

    inline bool isWordCharacter(QChar c)
    {
        return c.isLetterOrNumber() || c == '_';
    }

    // Where each match starts, left to right, never overlapping.
    inline QList<int> matches(const QString& text, const QString& find, const Options& options)
    {
        QList<int> found;
        if (find.isEmpty())
            return found;

        const Qt::CaseSensitivity cs = options.matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
        int from = 0;
        while (true)
        {
            const int at = text.indexOf(find, from, cs);
            if (at < 0)
                break;

            const int end = at + find.length();
            const bool clearLeft = at == 0 || !isWordCharacter(text.at(at - 1)) || !isWordCharacter(find.at(0));
            const bool clearRight = end >= text.length() || !isWordCharacter(text.at(end)) || !isWordCharacter(find.at(find.length() - 1));
            if (!options.wholeWords || (clearLeft && clearRight))
            {
                found.append(at);
                from = end;
            }
            else
                from = at + 1;
        }
        return found;
    }

    inline int count(const QString& text, const QString& find, const Options& options)
    {
        return matches(text, find, options).count();
    }

    // The text with every match replaced, as typed.
    inline QString replaced(const QString& text, const QString& find, const QString& with, const Options& options)
    {
        const QList<int> at = matches(text, find, options);
        if (at.isEmpty())
            return text;

        QString out;
        int from = 0;
        for (int start : at)
        {
            out += text.mid(from, start - from);
            out += with;
            from = start + find.length();
        }
        out += text.mid(from);
        return out;
    }

    // One line of text shown in the list: long text cut around its first match,
    // line breaks shown as a mark, so every match fits on its row.
    inline QString excerpt(const QString& text, const QString& find, const Options& options, int room = 80)
    {
        QString flat = text;
        flat.replace("\r\n", QString::fromUtf8("\xe2\x86\xb5 ")).replace('\n', QString::fromUtf8("\xe2\x86\xb5 "));
        if (flat.length() <= room)
            return flat;

        const QList<int> at = matches(flat, find, options);
        const int first = at.isEmpty() ? 0 : at.first();
        int start = qMax(0, first - room / 3);
        if (start + room > flat.length())
            start = qMax(0, flat.length() - room);
        QString cut = flat.mid(start, room);
        if (start > 0)
            cut = QString::fromUtf8("\xe2\x80\xa6") + cut;
        if (start + room < flat.length())
            cut += QString::fromUtf8("\xe2\x80\xa6");
        return cut;
    }
}
