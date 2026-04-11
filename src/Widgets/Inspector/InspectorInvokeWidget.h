#pragma once

#include "../Shared.h"

#include "Commands/TemplateCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"

#include <QtWidgets/QButtonGroup>
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
        TemplateCommand* command;
        bool lock;

        QWidget* invokeHeaderWidget;
        QVBoxLayout* invokeRowsLayout;
        QButtonGroup* invokeRadioGroup;
        QList<QLineEdit*> invokeLineEdits;
        QList<QPushButton*> invokePlayButtons;
        QList<QPushButton*> invokeRadios;

        void clearInvokeRows();
        void syncInvokesToCommand();

        Q_SLOT void addInvokeRow(const QString& text = "");
        Q_SLOT void removeInvokeRow();
        Q_SLOT void invokePlayClicked();
        Q_SLOT void invokeRadioChanged(int id);
        Q_SLOT void invokeTextChanged(const QString& text);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
};
