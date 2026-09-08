#pragma once

#include "Shared.h"

#include <QtCore/QString>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLineEdit>

// Icon picker for Simple Mode buttons: a grid of glyph/emoji icons plus a free
// text field. Icons are stored as text in the rundown XML, so they travel
// between clients with no image files involved.
class WIDGETS_EXPORT SimpleIconDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit SimpleIconDialog(const QString& currentIcon, QWidget* parent = nullptr);

        QString getIcon() const;   // empty = no icon

    private:
        QLineEdit* lineEditCustom;
};
