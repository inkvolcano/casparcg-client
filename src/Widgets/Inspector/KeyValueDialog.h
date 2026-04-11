#pragma once

#include "../Shared.h"
#include "ui_KeyValueDialog.h"

#include <QtCore/QEvent>

#include <QtWidgets/QDialog>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT KeyValueDialog : public QDialog, Ui::KeyValueDialog
{
    Q_OBJECT

    public:
        explicit KeyValueDialog(QWidget* parent = 0);

        const QString getKey() const;
        const QString getValue() const;
        int getMode() const;
        const QString getCycleValues() const;

        void setKey(const QString& key);
        void setValue(const QString& value);
        void setMode(int mode);
        void setCycleValues(const QString& cycleValues);
        void setTitle(const QString& title);

    private:
        Q_SLOT void modeChanged(int index);

    protected:
        virtual bool eventFilter(QObject* target, QEvent* event);
        void accept();
};
