#pragma once

#include "../Shared.h"
#include "ui_InspectorTemplateWidget.h"

#include "Commands/TemplateCommand.h"
#include "Events/Inspector/AddTemplateDataEvent.h"
#include "Events/Inspector/ShowAddTemplateDataDialogEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"
#include "Models/LibraryModel.h"
#include "../SheetDataResolver.h"

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <QtCore/QSet>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QWidget>

class NumericValueDelegate;

class WIDGETS_EXPORT InspectorTemplateWidget : public QWidget, Ui::InspectorTemplateWidget
{
    Q_OBJECT

    public:
        explicit InspectorTemplateWidget(QWidget* parent = 0);

        // The option rows, laid out on their own so the inspector can mount them as a
        // separate, collapsed "Template Settings" section. Still owned and wired here.
        QWidget* templateSettingsPanel() const { return this->settingsPanel; }

        // Raised whenever this widget's height changes: the table grew, the result
        // box was shown, hidden or re-rendered. The inspector row that holds this
        // widget re-reads sizeHint() on it, the way the Invoke section already does;
        // without that the row keeps its first height and the grid spreads the
        // difference out as dead space.
        Q_SIGNAL void contentChanged();

    protected:
        virtual bool eventFilter(QObject* target, QEvent* event);

    private:
        int fieldCounter;
        LibraryModel* model;
        // QPointer: the command dies whenever its rundown item is deleted or
        // rebuilt (move, undo, reload) — a raw pointer here caused crashes when
        // the key/value tree was touched afterwards.
        QPointer<TemplateCommand> command;
        bool lock;
        NumericValueDelegate* numericDelegate;
        // Expected result: what the row the operator has typed actually contains.
        // An index is not an answer until you can see who is at that index.
        QWidget* settingsPanel = nullptr;
        QWidget* expectedBox = nullptr;
        QLabel* expectedHeading = nullptr;
        QLabel* expectedStatus = nullptr;
        QTreeWidget* treeExpected = nullptr;
        QPushButton* buttonRefreshExpected = nullptr;
        TemplateSheetConnection sheetConnection;
        QList<SheetRow> expectedRows;   // the tab as last read
        QString expectedRequestId;
        SheetRowsOrigin expectedOrigin;   // where the held rows came from, and when
        QLabel* expectedFreshness = nullptr;

        QCheckBox* checkBoxAutoPlay = nullptr;
        QCheckBox* checkBoxAutoLoop = nullptr;
        QSpinBox* spinBoxAutoLoopDelay = nullptr;

        void updateTemplateDataModels();
        // The key/value table is as tall as the keys in it plus one free row, so the
        // section takes the space it needs and no more.
        void resizeDataTreeToContents();

        void buildExpectedBox();
        void refreshExpectedBinding(bool forceReload = false);   // show or hide it for the selected item
        void requestExpectedRows(bool forceReload);
        void renderExpectedRow();
        int expectedBaseRow(const QString& rawStart, int lines, QString* how) const;
        void renderExpectedFreshness();
        // Height exactly its content, width the full row. SetFixedSize would give the
        // first but take the second, so the height is pinned by hand instead.
        void pinExpectedBoxHeight();          // resolve the current key against what we hold
        QString currentTemplateFieldValue(const QString& key) const;

        void blockAllSignals(bool block);

        Q_SLOT bool addRow();
        Q_SLOT bool editRow();
        Q_SLOT bool removeRow();
        Q_SLOT bool duplicateSelectedItem();
        Q_SLOT bool copySelectedItem();
        Q_SLOT bool pasteSelectedItem();
        Q_SLOT void flashlayerChanged(int);
        Q_SLOT void useStoredDataChanged(int);
        Q_SLOT void sendAsJsonChanged(int);
        Q_SLOT void newlineBehaviorChanged(int);
        Q_SLOT void useUppercaseDataChanged(int);
        Q_SLOT void currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
        Q_SLOT void itemDoubleClicked(QTreeWidgetItem*, int);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void addTemplateData(const AddTemplateDataEvent&);
        Q_SLOT void showAddTemplateDataDialog(const ShowAddTemplateDataDialogEvent&);
        Q_SLOT void triggerOnNextChanged(int);
        Q_SLOT void autoPlayChanged(int);
        Q_SLOT void autoLoopChanged(int);
        Q_SLOT void autoLoopDelayChanged(int);
        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
        // Builds the key/value rows from an EBU OGraf manifest's JSON Schema,
        // the standard equivalent of this fork's window.debugData convention.
        void loadOgrafFields(const QString& manifestPath);

        Q_SLOT void loadDebugData();
        Q_SLOT void sheetRowsReady(const QString& requestId, const QList<SheetRow>& rows,
                                   const SheetRowsOrigin& origin);
        Q_SLOT void sheetRowsFailed(const QString& requestId, const QString& reason);
};
