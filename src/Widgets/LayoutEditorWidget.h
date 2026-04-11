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
        explicit LayoutEditorWidget(QWidget* parent = nullptr);

        void loadFromConfig();
        void saveToConfig();

    private:
        static const QStringList allWidgetIds;
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
