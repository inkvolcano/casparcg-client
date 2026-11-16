#include "RunSheet.h"

#include "Global.h"
#include "ReferenceImage.h"
#include "RundownTiming.h"
#include "RunSheetRules.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTimingBar.h"
#include "Rundown/RundownTreeBaseWidget.h"
#include "Rundown/RundownTreeWidget.h"

#include "Commands/AbstractCommand.h"
#include "Models/LibraryModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QHash>
#include <QtCore/QUrl>

#include <QtGui/QImage>
#include <QtGui/QPageLayout>
#include <QtGui/QPageSize>
#include <QtGui/QPdfWriter>
#include <QtGui/QTextDocument>

namespace
{
    struct Row
    {
        int depth = 0;
        QString label;
        QString type;
        QString where;
        QString note;
        QString picture;
        bool disabled = false;
        double length = -1;
    };
}

bool RunSheet::exportPdf(RundownTreeWidget* rundown, const QString& path, QString* error)
{
    if (rundown == nullptr || rundown->treeWidget() == nullptr)
    {
        if (error != nullptr)
            *error = "There is no rundown open.";
        return false;
    }

    RundownTreeBaseWidget* tree = rundown->treeWidget();
    RundownTimingBar* bar = rundown->getTimingBar();

    // The timing bar's own lengths, so the sheet and the bar always agree.
    QHash<QTreeWidgetItem*, double> timed;
    if (bar != nullptr)
    {
        for (const QPair<QTreeWidgetItem*, double>& entry : bar->timedItems())
            timed.insert(entry.first, entry.second);
    }

    QList<Row> rows;

    // Depth first, in rundown order.
    QList<QPair<QTreeWidgetItem*, int>> stack;
    for (int i = tree->topLevelItemCount() - 1; i >= 0; i--)
        stack << qMakePair(tree->topLevelItem(i), 0);
    while (!stack.isEmpty())
    {
        const QPair<QTreeWidgetItem*, int> next = stack.takeLast();
        QTreeWidgetItem* item = next.first;
        for (int c = item->childCount() - 1; c >= 0; c--)
            stack << qMakePair(item->child(c), next.second + 1);

        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(item, 0));
        if (widget == nullptr || widget->getCommand() == nullptr || widget->getLibraryModel() == nullptr)
            continue;

        Row row;
        row.depth = next.second;
        LibraryModel* model = widget->getLibraryModel();
        AbstractCommand* command = widget->getCommand();
        row.label = model->getLabel().isEmpty() ? model->getName() : model->getLabel();
        row.type = model->getType();
        if (!model->getDeviceName().isEmpty() && command->getChannel() > 0)
            row.where = QString("%1 %2-%3").arg(model->getDeviceName()).arg(command->getChannel()).arg(command->getVideolayer());
        row.note = command->getNote();
        row.picture = command->getReferenceImage();
        row.disabled = command->getDisabled();
        row.length = timed.value(item, -1);
        rows << row;
    }

    QList<double> lengths;
    for (const Row& row : rows)
        lengths << row.length;

    const QString hardOut = bar != nullptr ? bar->hardOut() : QString();
    const int hardOutSeconds = RundownTiming::parseHardOut(hardOut);
    const QStringList starts = RunSheetRules::starts(lengths, hardOutSeconds);

    const QString name = rundown->getActiveRundown() == Rundown::DEFAULT_NAME
        ? QString("Untitled rundown") : QFileInfo(rundown->getActiveRundown()).completeBaseName();

    QTextDocument document;
    QString html = "<html><body style=\"font-family: 'Segoe UI', Arial, sans-serif; font-size: 9pt;\">";
    html += QString("<h2 style=\"margin-bottom: 2px;\">%1</h2>").arg(name.toHtmlEscaped());
    QString summary = QString("Run sheet, %1. Running time %2.")
        .arg(QDateTime::currentDateTime().toString("ddd d MMM yyyy, HH:mm"), RundownTiming::formatLength(RunSheetRules::total(lengths)));
    if (hardOutSeconds >= 0)
        summary += QString(" Hard out %1; start times are back-timed to it.").arg(RundownTiming::formatTimeOfDay(hardOutSeconds));
    else
        summary += " Start times are from the top.";
    html += QString("<p style=\"color: #555;\">%1</p>").arg(summary.toHtmlEscaped());

    html += "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"4\" border=\"1\" style=\"border-collapse: collapse; border-color: #bbb;\">";
    // Widths so a name and a note have room and a number does not.
    html += "<tr style=\"background-color: #e8e8e8;\"><th width=\"4%\">#</th><th width=\"22%\">Item</th><th width=\"8%\">Type</th>"
            "<th width=\"12%\">Server ch-layer</th><th width=\"7%\">Length</th><th width=\"8%\">Starts</th>"
            "<th width=\"25%\">Note</th><th width=\"14%\">Picture</th></tr>";

    for (int i = 0; i < rows.count(); i++)
    {
        const Row& row = rows.at(i);
        QString label = row.label.toHtmlEscaped();
        if (row.disabled)
            label += " <i>(disabled)</i>";
        QString indent;
        for (int d = 0; d < row.depth; d++)
            indent += "&nbsp;&nbsp;&nbsp;&nbsp;";

        QString picture;
        const QImage image = ReferenceImage::decode(row.picture);
        if (!image.isNull())
        {
            const QString key = QString("runsheet-%1").arg(i);
            document.addResource(QTextDocument::ImageResource, QUrl(key), image.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            picture = QString("<img src=\"%1\"/>").arg(key);
        }

        QString note = row.note.toHtmlEscaped();
        note.replace("\n", "<br/>");

        html += QString("<tr%1><td>%2</td><td>%3%4</td><td>%5</td><td>%6</td><td>%7</td><td>%8</td><td>%9</td><td>%10</td></tr>")
            .arg(row.disabled ? QString(" style=\"color: #999;\"") : QString())
            .arg(i + 1)
            .arg(indent, label, row.type.toHtmlEscaped(), row.where.toHtmlEscaped(),
                 row.length >= 0 ? RundownTiming::formatLength(row.length) : QString(),
                 starts.value(i), note, picture);
    }
    html += "</table></body></html>";
    document.setHtml(html);

    QPdfWriter writer(path);
    writer.setTitle(name + " - run sheet");
    writer.setCreator("CasparCG Client");
    writer.setResolution(150);
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter));
    document.print(&writer);

    if (!QFileInfo(path).isFile() || QFileInfo(path).size() == 0)
    {
        if (error != nullptr)
            *error = QString("%1 could not be written.").arg(path);
        return false;
    }
    return true;
}
