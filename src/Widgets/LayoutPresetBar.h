#pragma once

#include "Shared.h"

#include <QtCore/QString>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

// The saved-layouts row that sits in the Layout tab and again in the Simple Mode
// tab. One widget rather than two, because the only difference between them is
// which scope they read and write — and two copies would drift.
//
// It does not know what a layout is. It reads and writes the configuration keys
// the scope owns (see Common/LayoutPreset.h) and tells whoever owns it that
// something changed, so the editor above can redraw and the layout can rebuild.
class WIDGETS_EXPORT LayoutPresetBar : public QGroupBox
{
    Q_OBJECT

    public:
        explicit LayoutPresetBar(const QString& scope, QWidget* parent = nullptr);

        // Re-reads the preset list. Called after something outside this widget
        // has changed them.
        void refresh();

    Q_SIGNALS:
        // A preset was applied, so every setting this scope owns has just been
        // rewritten and anything showing them is now stale.
        void presetApplied();

    private:
        QString scope;

        QComboBox* presetCombo = nullptr;
        QPushButton* saveButton = nullptr;
        QPushButton* applyButton = nullptr;
        QPushButton* renameButton = nullptr;
        QPushButton* deleteButton = nullptr;

        int selectedPresetId() const;
        QString selectedPresetName() const;
        void updateButtonState();

        Q_SLOT void saveClicked();
        Q_SLOT void applyClicked();
        Q_SLOT void renameClicked();
        Q_SLOT void deleteClicked();
};
