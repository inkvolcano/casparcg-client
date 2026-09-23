#include "RundownTimingBar.h"

#include "RundownTiming.h"
#include "AbstractRundownWidget.h"
#include "RundownGroupWidget.h"
#include "RundownTreeBaseWidget.h"
#include "RundownWidgetHelper.h"
#include "Commands/AbstractCommand.h"
#include "Commands/MovieCommand.h"
#include "Models/LibraryModel.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QTime>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>

namespace
{
    // Labels are only touched when their text changes: this runs every second.
    void setTextIfChanged(QLabel* label, const QString& text)
    {
        if (label->text() != text)
            label->setText(text);
    }
}

RundownTimingBar::RundownTimingBar(RundownTreeBaseWidget* tree, QWidget* parent)
    : QFrame(parent),
      tree(tree)
{
    setObjectName("rundownTimingBar");
    setFixedHeight(24);
    setStyleSheet("QFrame#rundownTimingBar { background-color: rgba(30, 30, 30, 200); border-top: 1px solid rgba(70, 70, 70, 200); }"
                  "QLabel { color: rgba(200, 200, 200, 220); font-size: 11px; }");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(14);

    this->labelTotal = new QLabel(this);
    this->labelTotal->setToolTip("The whole rundown: clip lengths, Duration settings, and a group's own Duration where one is typed.\n"
                                 "Disabled items count nothing.");
    layout->addWidget(this->labelTotal);

    this->labelFromSelected = new QLabel(this);
    this->labelFromSelected->setToolTip("From the selected item to the end of the rundown.");
    layout->addWidget(this->labelFromSelected);

    layout->addStretch();

    QLabel* hardOutLabel = new QLabel("Hard out", this);
    layout->addWidget(hardOutLabel);

    this->editHardOut = new QLineEdit(this);
    this->editHardOut->setPlaceholderText("none");
    this->editHardOut->setFixedWidth(72);
    this->editHardOut->setToolTip("The time this rundown must be off air, such as 21:00 or 21:00:00.\n"
                                  "Saved with the rundown. Clear it to turn back-timing off.");
    this->editHardOut->setValidator(new QRegularExpressionValidator(QRegularExpression("^\\d{0,2}(:\\d{0,2}(:\\d{0,2})?)?$"), this->editHardOut));
    this->editHardOut->setStyleSheet("QLineEdit { font-size: 11px; padding: 0px 4px; }");
    QObject::connect(this->editHardOut, &QLineEdit::editingFinished, this, &RundownTimingBar::hardOutEdited);
    layout->addWidget(this->editHardOut);

    this->labelEnds = new QLabel(this);
    this->labelEnds->setToolTip("When the rundown ends if it starts from the selected item now.");
    layout->addWidget(this->labelEnds);

    this->labelOverUnder = new QLabel(this);
    layout->addWidget(this->labelOverUnder);

    this->tick.setInterval(1000);
    QObject::connect(&this->tick, &QTimer::timeout, this, [this]() {
        // The totals every other second - an Inspector edit to a Duration has no
        // signal of its own to hang on - and the clock every second.
        if (++this->ticks % 2 == 0)
            recompute();
        else
            refreshClock();
    });

    QObject::connect(this->tree, &QTreeWidget::currentItemChanged, this, [this]() { recompute(); });

    refreshClock();
}

QString RundownTimingBar::hardOut() const
{
    return this->hardOutText;
}

void RundownTimingBar::setHardOut(const QString& hardOut)
{
    const int seconds = RundownTiming::parseHardOut(hardOut);
    this->hardOutText = seconds < 0 ? QString() : RundownTiming::formatTimeOfDay(seconds);
    this->editHardOut->setText(this->hardOutText);
    refreshClock();
}

void RundownTimingBar::hardOutEdited()
{
    const QString typed = this->editHardOut->text().trimmed();
    if (typed.isEmpty())
    {
        setHardOut(QString());
        return;
    }

    if (RundownTiming::parseHardOut(typed) < 0)
    {
        // Not a time of day: back to what it was, rather than a hard out nobody meant.
        this->editHardOut->setText(this->hardOutText);
        return;
    }

    setHardOut(typed);
}

void RundownTimingBar::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    recompute();
    this->tick.start();
}

void RundownTimingBar::hideEvent(QHideEvent* event)
{
    QFrame::hideEvent(event);
    this->tick.stop();
}

double RundownTimingBar::itemSeconds(QTreeWidgetItem* item) const
{
    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->tree->itemWidget(item, 0));
    if (widget == nullptr || widget->getCommand() == nullptr)
        return 0;

    AbstractCommand* command = widget->getCommand();
    const LibraryModel* model = widget->getLibraryModel();
    const double fps = RundownWidgetHelper::getChannelFps(model->getDeviceName(), command->getBaseChannel());

    if (model->getType() == Rundown::MOVIE || model->getType() == Rundown::AUDIO)
    {
        const double clip = RundownWidgetHelper::timecodeToSeconds(model->getTimecode());
        if (MovieCommand* movie = dynamic_cast<MovieCommand*>(command))
            return RundownTiming::clipSeconds(clip, movie->getSeek(), movie->getLength(), fps);
        return clip;
    }

    return RundownTiming::durationSeconds(command->getDuration(),
        RundownWidgetHelper::getDurationUnit() == Output::DEFAULT_DELAY_IN_MILLISECONDS, fps);
}

// Adds the item's leaves in play order and returns its length. countAsLeaves is
// false inside a group whose own Duration stands for the whole group: its items
// are still added up, for the group's own row, but not counted twice.
double RundownTimingBar::walk(QTreeWidgetItem* item, QList<Leaf>& leaves, bool countAsLeaves)
{
    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->tree->itemWidget(item, 0));
    if (widget == nullptr || widget->getCommand() == nullptr || widget->getCommand()->getDisabled())
        return 0;

    if (!widget->isGroup())
    {
        const double seconds = itemSeconds(item);
        if (countAsLeaves)
            leaves.append({ item, seconds });
        return seconds;
    }

    // A group's typed Duration is in milliseconds, whatever the item setting.
    const double planned = widget->getCommand()->getDuration() / 1000.0;
    if (planned > 0 && countAsLeaves)
        leaves.append({ item, planned });

    double sum = 0;
    for (int i = 0; i < item->childCount(); i++)
        sum += walk(item->child(i), leaves, countAsLeaves && planned <= 0);

    if (RundownGroupWidget* group = dynamic_cast<RundownGroupWidget*>(widget))
        group->setComputedLength(sum);

    return planned > 0 ? planned : sum;
}

void RundownTimingBar::recompute()
{
    if (!isVisible())
        return;

    // Runs every two seconds while the rundown is on screen, so its cost on a big
    // rundown is logged if it ever becomes noticeable.
    QElapsedTimer clock;
    clock.start();

    QList<Leaf> leaves;
    QTreeWidgetItem* root = this->tree->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
        walk(root->child(i), leaves, true);

    this->totalSeconds = 0;
    for (const Leaf& leaf : leaves)
        this->totalSeconds += leaf.seconds;

    // From the selected item: the first leaf that is it, is inside it (a group was
    // selected), or holds it (it is inside a group counted by its own Duration).
    this->remainingSeconds = this->totalSeconds;
    QTreeWidgetItem* current = this->tree->currentItem();
    if (current != nullptr)
    {
        auto related = [](QTreeWidgetItem* a, QTreeWidgetItem* b) {
            for (QTreeWidgetItem* p = a; p != nullptr; p = p->parent())
                if (p == b)
                    return true;
            return false;
        };

        double before = 0;
        for (const Leaf& leaf : leaves)
        {
            if (leaf.item == current || related(leaf.item, current) || related(current, leaf.item))
                break;
            before += leaf.seconds;
        }
        this->remainingSeconds = qMax(0.0, this->totalSeconds - before);
    }

    const qint64 took = clock.elapsed();
    if (took >= 30)
        qDebug("Rundown timing took %lld ms for %d items", took, int(leaves.count()));

    setTextIfChanged(this->labelTotal, QString("Total %1").arg(RundownTiming::formatLength(this->totalSeconds)));
    setTextIfChanged(this->labelFromSelected, QString("From selected %1").arg(RundownTiming::formatLength(this->remainingSeconds)));

    refreshClock();
}

void RundownTimingBar::refreshClock()
{
    const int hardOut = RundownTiming::parseHardOut(this->hardOutText);
    this->labelEnds->setVisible(hardOut >= 0);
    this->labelOverUnder->setVisible(hardOut >= 0);
    if (hardOut < 0)
        return;

    const int now = QTime::currentTime().msecsSinceStartOfDay() / 1000;
    const double over = RundownTiming::overBy(now, this->remainingSeconds, hardOut);

    setTextIfChanged(this->labelEnds,
        QString("Ends %1").arg(RundownTiming::formatTimeOfDay(now + qRound(this->remainingSeconds))));

    QString text;
    QString colour;
    if (qAbs(over) < 1)
    {
        text = "On time";
        colour = "rgb(200, 200, 200)";
    }
    else if (over > 0)
    {
        text = QString("%1 over").arg(RundownTiming::formatLength(over));
        colour = "rgb(239, 83, 80)";
    }
    else
    {
        text = QString("%1 under").arg(RundownTiming::formatLength(-over));
        colour = "rgb(102, 187, 106)";
    }

    setTextIfChanged(this->labelOverUnder, text);
    RundownWidgetHelper::setStyleSheetIfChanged(this->labelOverUnder,
        QString("QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(colour));
}
