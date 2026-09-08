#pragma once

#include "Shared.h"
#include "ui_SheetsPanelWidget.h"
#include "SheetsProjectRegistry.h"

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>

#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

// One operator button definition, stored in <project>/extensions.json.
struct SheetsActionDef
{
    QString label;          // may contain {COLUMN} placeholders (row scope)
    QString color;          // "#rrggbb" or empty for default
    QString templateName;   // e.g. "SEVILLE/card"
    int channel = 1;
    int videolayer = 20;
    QString data;           // "key=value,key2={COLUMN}" pairs
    QString action;         // play | stop | update | toggle

    QJsonObject toJson() const;
    static SheetsActionDef fromJson(const QJsonObject& obj);
};

struct SheetsTabActions
{
    QList<SheetsActionDef> rowButtons;   // rendered per data row, placeholders resolved per row
    QList<SheetsActionDef> standalone;   // rendered once above the table
};

class WIDGETS_EXPORT SheetsPanelWidget : public QWidget, Ui::SheetsPanelWidget
{
    Q_OBJECT

    public:
        explicit SheetsPanelWidget(QWidget* parent = nullptr);

    private:
        QNetworkAccessManager* networkManager;
        QList<SheetsProject> projects;
        QMap<QString, SheetsTabActions> tabActions;   // key = tab name, from extensions.json
        QStringList headers;                          // current tab's header row
        QList<QStringList> rows;                      // current tab's data rows
        QSet<QString> litToggles;                     // toggle-state keys currently "on air"
        QMap<QString, QString> litToggleTargets;      // toggle key -> "channel/videolayer" it plays on
        QTimer pollTimer;
        bool collapsed = false;

        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QMenu* columnsMenu = nullptr;                 // per-tab column show/hide
        QAction* expandCollapseAction = nullptr;

        void setupMenus();
        void discoverProjects();
        SheetsProject* currentProject();
        QString currentTab() const;

        void loadExtensions();
        void saveExtensions();

        void fetchTabs();
        void fetchValues();
        void renderData();
        void renderStandaloneButtons();
        QWidget* buildRowButtons(int rowIndex);

        QString resolvePlaceholders(const QString& text, int rowIndex) const;
        QString buildTemplateDataXml(const QString& resolvedData) const;
        void executeAction(const SheetsActionDef& def, int rowIndex, const QString& toggleKey, QPushButton* button);
        void styleActionButton(QPushButton* button, const SheetsActionDef& def, bool lit) const;

        QString hiddenColumnsKey() const;
        QSet<QString> hiddenColumns() const;
        void setColumnHiddenState(const QString& columnName, bool hidden);
        void buildColumnsMenu();

        void setStatus(const QString& text);
        void applyPollInterval();
        void clearLitTogglesForTarget(const QString& target, const QString& exceptKey);
        void rebuildButtonsDeferred();

        Q_SLOT void projectChanged(int index);
        Q_SLOT void tabChanged(int index);
        Q_SLOT void refresh();
        Q_SLOT void editActions();
        Q_SLOT void toggleExpandCollapse();
};
