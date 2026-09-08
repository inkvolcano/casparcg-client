#pragma once

#include "../Shared.h"

#include "Commands/TemplateCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QPointer>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorInvokeWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit InspectorInvokeWidget(QWidget* parent = 0);

    Q_SIGNALS:
        void contentChanged();

    private:
        // QPointer: auto-nulls when the command's rundown item is deleted or
        // rebuilt, so invoke edits afterwards can't dereference a dead command.
        QPointer<TemplateCommand> command;
        LibraryModel* model = nullptr;
        bool lock;

        QWidget* invokeHeaderWidget;
        QVBoxLayout* invokeRowsLayout;
        QButtonGroup* invokeRadioGroup;
        QStringList discoveredFunctions;          // dropdown choices from Discover Functions
        QList<QWidget*> invokeRowWidgets;         // parallel to the lists below
        QList<QWidget*> invokeDragHandles;        // grip that reorders its row
        QList<QComboBox*> invokeCombos;
        QList<QLineEdit*> invokeLabelEdits;
        QList<QPushButton*> invokePlayButtons;
        QList<QPushButton*> invokeRadios;

        void clearInvokeRows();
        void syncInvokesToCommand();
        void moveInvokeRow(int from, int to);
        void dragInvokeRowTo(QWidget* handle, const QPoint& globalPos);
        QStringList scanTemplateFunctions(QString* outFilePath = nullptr) const;

        Q_SLOT void addInvokeRow(const QString& text = "", const QString& label = "");
        Q_SLOT void removeInvokeRow();
        Q_SLOT void importInvokes();
        Q_SLOT void invokePlayClicked();
        Q_SLOT void invokeRadioChanged(int id);
        Q_SLOT void invokeTextChanged(const QString& text);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
};
