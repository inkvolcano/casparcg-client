#pragma once

#include "../Shared.h"

#include <QtCore/QList>
#include <QtCore/QTimer>
#include <QtWidgets/QFrame>

class QLabel;
class QLineEdit;
class QTreeWidgetItem;
class RundownTreeBaseWidget;

// The bar under a rundown: how long it runs, how much is left from the selected
// item, and - with a hard out typed in - when it will end and whether that is
// over or under. What each item counts is in src/Common/RundownTiming.h. Group
// rows show their worked-out length as well.
//
// It only works while its rundown is on screen: the totals are worked out every
// two seconds and when the selection changes, the clock every second, and a tab
// in the background does neither.
class WIDGETS_EXPORT RundownTimingBar : public QFrame
{
    Q_OBJECT

    public:
        RundownTimingBar(RundownTreeBaseWidget* tree, QWidget* parent = nullptr);

        // "HH:MM:SS", or empty for none. Saved with the rundown by its owner.
        QString hardOut() const;
        void setHardOut(const QString& hardOut);

        Q_SLOT void recompute();

    protected:
        void showEvent(QShowEvent* event) override;
        void hideEvent(QHideEvent* event) override;

    private:
        struct Leaf
        {
            QTreeWidgetItem* item = nullptr;
            double seconds = 0;
        };

        RundownTreeBaseWidget* tree;
        QLabel* labelTotal = nullptr;
        QLabel* labelFromSelected = nullptr;
        QLineEdit* editHardOut = nullptr;
        QLabel* labelEnds = nullptr;
        QLabel* labelOverUnder = nullptr;

        QTimer tick;
        int ticks = 0;
        double totalSeconds = 0;
        double remainingSeconds = 0;
        QString hardOutText;

        double walk(QTreeWidgetItem* item, QList<Leaf>& leaves, bool countAsLeaves);
        double itemSeconds(QTreeWidgetItem* item) const;
        void refreshClock();
        void hardOutEdited();
};
