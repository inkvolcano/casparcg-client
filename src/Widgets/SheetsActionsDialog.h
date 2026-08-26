#pragma once

#include "Shared.h"
#include "SheetsPanelWidget.h"

#include <QtCore/QStringList>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTreeWidget>

// Editor for a tab's operator buttons: row-scoped (placeholder-resolved per data row)
// and standalone. Saved by the panel to <project>/extensions.json.
class WIDGETS_EXPORT SheetsActionsDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit SheetsActionsDialog(const QString& tabName, const QStringList& headers,
                                     const QStringList& templateNames,
                                     const SheetsTabActions& actions, QWidget* parent = nullptr);

        SheetsTabActions getActions() const;

    private:
        QStringList headers;
        QStringList templateNames;
        SheetsTabActions actions;

        QTreeWidget* treeWidget;

        void rebuildTree();
        bool editDefinition(SheetsActionDef& def);

        Q_SLOT void addRowButton();
        Q_SLOT void addStandaloneButton();
        Q_SLOT void editSelected();
        Q_SLOT void removeSelected();
};
