#include "LoudnessReadout.h"

#include "LoudnessMeter.h"
#include "LoudnessMonitor.h"

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

LoudnessReadout::LoudnessReadout(const QString& server, int channel, QWidget* parent)
    : QWidget(parent), monitor(LoudnessMonitor::forChannel(server, channel))
{
    setFixedWidth(118);
    setCursor(Qt::ArrowCursor);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 2, 4, 2);
    layout->setSpacing(1);

    QLabel* title = new QLabel("LOUDNESS", this);
    title->setStyleSheet("color: rgba(200, 200, 200, 150); font-size: 9px; font-weight: bold; letter-spacing: 1px;");
    layout->addWidget(title);

    this->labelShort = new QLabel(this);
    this->labelShort->setStyleSheet("font-size: 22px; font-weight: bold; font-family: Consolas, monospace;");
    layout->addWidget(this->labelShort);

    this->labelDetail = new QLabel(this);
    this->labelDetail->setStyleSheet("font-size: 10px; font-family: Consolas, monospace; color: rgba(220, 220, 220, 200);");
    layout->addWidget(this->labelDetail);

    this->labelSource = new QLabel(this);
    this->labelSource->setStyleSheet("font-size: 9px; color: rgba(200, 200, 200, 150);");
    layout->addWidget(this->labelSource);
    layout->addStretch(1);

    QObject::connect(this->monitor.data(), &LoudnessMonitor::changed, this, [this]() {
        if (isVisible())
            refresh();
    });
    refresh();
}

void LoudnessReadout::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!this->monitor.isNull())
        this->monitor->start();
    refresh();
}

void LoudnessReadout::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    if (!this->monitor.isNull())
        this->monitor->reset();
}

void LoudnessReadout::refresh()
{
    if (this->monitor.isNull())
        return;

    const bool measured = this->monitor->measured();
    const QString approx = measured ? QString() : QString::fromUtf8("~");
    const double shortTerm = this->monitor->shortTerm();

    this->labelShort->setText(approx + Loudness::text(shortTerm));
    this->labelDetail->setText(measured ? QString("I %1  M %2").arg(Loudness::text(this->monitor->integrated()),
                                                                   Loudness::text(this->monitor->momentary()))
                                        : QString("I %1%2").arg(approx, Loudness::text(this->monitor->integrated())));
    this->labelSource->setText(measured ? "LUFS S \xc2\xb7 R128, NDI" : "LUFS S \xc2\xb7 estimate, OSC");

    // Far from the -23 target (EBU R128): amber beyond 2 LU, red beyond 5.
    QString colour = "rgba(230, 230, 230, 230)";
    if (Loudness::isReading(shortTerm))
    {
        const double off = qAbs(shortTerm + 23.0);
        if (off > 5.0)
            colour = "rgb(229, 72, 77)";
        else if (off > 2.0)
            colour = "rgb(214, 160, 53)";
    }
    // Restyled only when the colour changes: this runs four times a second.
    if (colour != this->shownColour)
    {
        this->shownColour = colour;
        this->labelShort->setStyleSheet(QString("font-size: 22px; font-weight: bold; font-family: Consolas, monospace; color: %1;").arg(colour));
    }

    setToolTip(QString("%1\nS short-term (3 s), I integrated since the start, M momentary (400 ms). Target -23 LUFS (EBU R128).\n"
                       "Double-click to start the integrated reading again.").arg(this->monitor->sourceText()));
}
