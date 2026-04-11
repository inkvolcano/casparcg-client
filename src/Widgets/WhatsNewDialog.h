#pragma once

#include "Shared.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT WhatsNewDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit WhatsNewDialog(QWidget* parent = 0);

        static void showOnStartupIfEnabled(QWidget* parent);

    private:
        QCheckBox* checkBoxHideOnStartup;
};
