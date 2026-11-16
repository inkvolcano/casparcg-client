// Find and replace in rundowns: what counts as a match, and what the text
// becomes.

#include "../src/Common/FindReplaceRules.h"

#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expect(const QString& actual, const QString& expected, const QString& what)
{
    checks++;
    if (actual == expected)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n        got      " << actual << "\n        expected " << expected << "\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

using namespace FindReplaceRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Find and replace\n";

    Options plain;
    Options matchCase;
    matchCase.matchCase = true;
    Options words;
    words.wholeWords = true;

    expect(replaced("Jan de Vries - Jan", "Jan", "Piet", plain), "Piet de Vries - Piet", "every match replaced");
    expect(replaced("jan JAN Jan", "jan", "Piet", plain), "Piet Piet Piet", "case ignored unless asked");
    expect(replaced("jan JAN Jan", "Jan", "Piet", matchCase), "jan JAN Piet", "match case: only the exact spelling");
    expect(replaced("Jan Janssen", "Jan", "Piet", words), "Piet Janssen", "whole words: not inside a longer word");
    expect(replaced("Jan_2 Jan", "Jan", "Piet", words), "Jan_2 Piet", "an underscore is part of a word");
    expect(replaced("(Jan)", "Jan", "Piet", words), "(Piet)", "brackets are not part of a word");
    expect(replaced("score: 2-1", "2-1", "3-1", words), "score: 3-1", "whole words with a dash inside the search");
    expect(replaced("a.b.c", ".", "/", plain), "a/b/c", "a dot is a dot, not a pattern");
    expect(replaced("x+y", "+", "\\1 $1", plain), "x\\1 $1y", "the replacement is literal: no back-references");
    expect(replaced("aaaa", "aa", "b", plain), "bb", "matches never overlap");
    expect(replaced("aaa", "aa", "b", plain), "ba", "the leftover stays");
    expect(replaced("Seizoen 2025", "2025", "", plain), "Seizoen ", "replacing with nothing removes it");
    expect(replaced("unchanged", "", "x", plain), "unchanged", "nothing to look for: nothing changes");
    expect(replaced("caf\xc3\xa9", "CAF\xc3\x89", "bar", plain), "bar", "case ignored beyond ASCII");
    expect(replaced("line one\nline two", "line", "Line", matchCase), "Line one\nLine two", "across lines");

    expectTrue(count("Jan Janssen jan", "jan", plain) == 3, "count: three, any case");
    expectTrue(count("Jan Janssen jan", "jan", words) == 2, "count: two whole words");
    expectTrue(count("JanJan Jan", "Jan", words) == 1, "a whole word next to itself is not two words");
    expectTrue(count("", "x", plain) == 0, "empty text: no match");

    expect(excerpt("short text", "text", plain), "short text", "short text shown whole");
    expect(excerpt("one\ntwo", "two", plain), QString::fromUtf8("one\xe2\x86\xb5 two"), "a line break shown as a mark");
    const QString longText = QString(100, 'a') + "needle" + QString(100, 'b');
    const QString cut = excerpt(longText, "needle", plain, 40);
    expectTrue(cut.contains("needle") && cut.startsWith(QString::fromUtf8("\xe2\x80\xa6")) && cut.endsWith(QString::fromUtf8("\xe2\x80\xa6")),
               "long text cut around its match, marked at both ends");

    expectTrue(kindText(Label) == "Label" && kindText(ShellLine) == "Shell command", "what each kind is called");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
