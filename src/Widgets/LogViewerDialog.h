#pragma once

#include "Shared.h"

#include "LogLines.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
QT_END_NAMESPACE

// Help > Show Log: the client's own daily log, in the client - so nobody has
// to find %USERPROFILE%\.CasparCG\Client\Logs to see what it said.
//
// Pick the day, filter to warnings and errors or errors only, search; today's
// log is followed as it is written. The newest 5,000 matching entries are
// shown. Reading and filtering are in Common/LogLines.h (tools/test-loglines).
class WIDGETS_EXPORT LogViewerDialog : public QDialog
{
    Q_OBJECT

    public:
        static void openFor(QWidget* parent);

        explicit LogViewerDialog(QWidget* parent = nullptr);

        static QString logFolder();

    private:
        QComboBox* comboDay = nullptr;
        QComboBox* comboFilter = nullptr;
        QLineEdit* editSearch = nullptr;
        QCheckBox* checkFollow = nullptr;
        QPlainTextEdit* text = nullptr;
        QLabel* labelCount = nullptr;
        QTimer followTimer;
        QTimer searchTimer;

        QList<LogLines::Entry> entries;
        LogLines::Reader reader;
        QString path;
        qint64 readTo = 0;

        void fillDays();
        void load();
        void follow();
        void refreshView();
        LogLines::Filter filter() const;
};
