#pragma once

#include "Shared.h"

#include <QtCore/QMap>
#include <QtCore/QStringList>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT LayoutEditorWidget : public QWidget
{
    Q_OBJECT

    public:
        // keyPrefix prepends every config key ("Simple" -> SimpleLayoutColumnOrder,
        // SimpleLayoutPanel1, ...) so a second instance can edit an independent layout.
        explicit LayoutEditorWidget(QWidget* parent = nullptr, const QString& keyPrefix = QString());

        void loadFromConfig();
        void saveToConfig();

    private:
        QString keyPrefix;

        static QString widgetDisplayName(const QString& id);
        static QString columnDisplayName(const QString& id);

        // Column order strip.
        QListWidget* columnOrderList;

        // Widget assignment lists.
        QHBoxLayout* panelListsLayout;
        QMap<QString, QListWidget*> panelLists;
        QListWidget* availableList;

        QStringList gatherColumnOrder();
        QMap<QString, QStringList> gatherPanelWidgets();

        void rebuildPanelLists();
        QListWidget* createWidgetList();

        Q_SLOT void resetDefaults();
};
