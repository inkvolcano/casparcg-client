#pragma once

#include "Shared.h"

#include <QtCore/QPointer>
#include <QtCore/QString>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QTreeWidget;
QT_END_NAMESPACE

class RundownTreeWidget;

// File > Rundown Versions...: the last saves of the open rundown, kept beside it
// (Common/RundownVersions.h). Open one in a tab of its own, or put one back -
// the rundown as it is now is kept as a version first, so putting back can
// itself be undone the same way.
class WIDGETS_EXPORT RundownVersionsDialog : public QDialog
{
    Q_OBJECT

    public:
        static void openFor(RundownTreeWidget* rundown, QWidget* parent);

        explicit RundownVersionsDialog(RundownTreeWidget* rundown, QWidget* parent = nullptr);

        // Keep the file at `rundownPath` as a version before it is replaced, and
        // trim the folder to the newest ten. Nothing happens when there is no
        // file yet, or when `replacement` is what the file already holds.
        static void keepVersion(const QString& rundownPath, const QByteArray& replacement = QByteArray());

    private:
        QPointer<RundownTreeWidget> rundown;
        QString rundownPath;

        QLabel* labelInfo = nullptr;
        QTreeWidget* list = nullptr;
        QPushButton* buttonOpen = nullptr;
        QPushButton* buttonPutBack = nullptr;

        void fill();
        QString chosenPath() const;
        void putBack();
};
