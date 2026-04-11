#include "NumericValueDelegate.h"

#include <QtCore/QEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QColorDialog>

NumericValueDelegate::NumericValueDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

NumericValueDelegate::Mode NumericValueDelegate::rowMode(const QModelIndex& index) const
{
    QModelIndex keyIndex = index.sibling(index.row(), 0);
    return static_cast<Mode>(keyIndex.data(Qt::UserRole).toInt());
}

bool NumericValueDelegate::isNumeric(const QString& text) const
{
    if (text.trimmed().isEmpty())
        return false;

    bool ok = false;
    text.trimmed().toDouble(&ok);
    return ok;
}

QRect NumericValueDelegate::upArrowRect(const QStyleOptionViewItem& option) const
{
    int btnH = option.rect.height() / 2;
    return QRect(option.rect.left(), option.rect.top(), option.rect.width(), btnH);
}

QRect NumericValueDelegate::downArrowRect(const QStyleOptionViewItem& option) const
{
    int btnH = option.rect.height() / 2;
    return QRect(option.rect.left(), option.rect.top() + btnH, option.rect.width(), option.rect.height() - btnH);
}

void NumericValueDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    // Column 2 (interaction column): render depends on mode.
    if (index.column() == 2)
    {
        Mode mode = rowMode(index);
        if (mode == Text)
            return;

        QModelIndex valueIndex = index.sibling(index.row(), 1);
        QString text = valueIndex.data(Qt::DisplayRole).toString();

        painter->save();

        if (mode == Boolean)
        {
            // Draw a toggle indicator: green when "true", dark when "false".
            QColor btnBg(60, 60, 60);
            painter->fillRect(option.rect, btnBg);

            QString val = text.trimmed().toLower();
            bool isTrue = (val == "true" || val == "1" || val == "yes");

            QColor indicatorColor = isTrue ? QColor(80, 200, 120) : QColor(90, 90, 90);
            int size = qMin(option.rect.width() - 4, option.rect.height() - 4);
            size = qMin(size, 12);
            QRect indicator(option.rect.center().x() - size / 2,
                            option.rect.center().y() - size / 2,
                            size, size);

            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(indicatorColor);
            painter->drawRoundedRect(indicator, 2, 2);
        }
        else if (mode == Color)
        {
            // Draw a color swatch.
            QColor btnBg(60, 60, 60);
            painter->fillRect(option.rect, btnBg);

            QColor color(text.trimmed());
            int size = qMin(option.rect.width() - 4, option.rect.height() - 4);
            size = qMin(size, 14);
            QRect swatch(option.rect.center().x() - size / 2,
                         option.rect.center().y() - size / 2,
                         size, size);

            if (color.isValid())
            {
                painter->setPen(QColor(120, 120, 120));
                painter->setBrush(color);
                painter->drawRect(swatch);
            }
            else
            {
                // Grey placeholder with diagonal line.
                painter->setPen(QColor(120, 120, 120));
                painter->setBrush(QColor(80, 80, 80));
                painter->drawRect(swatch);
                painter->setPen(QColor(120, 120, 120));
                painter->drawLine(swatch.topLeft(), swatch.bottomRight());
            }
        }
        else
        {
            // Integer, Decimal, Cycle: draw up/down arrows.
            if ((mode == Integer || mode == Decimal) && !isNumeric(text))
            {
                painter->restore();
                return;
            }

            QRect upRect = upArrowRect(option);
            QRect downRect = downArrowRect(option);

            // Button background.
            QColor btnBg(60, 60, 60);
            painter->fillRect(upRect, btnBg);
            painter->fillRect(downRect, btnBg);

            // Separator line.
            painter->setPen(QColor(80, 80, 80));
            painter->drawLine(upRect.bottomLeft(), upRect.bottomRight());

            // Draw arrows.
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(180, 180, 180));

            // Up triangle.
            {
                QPointF center = upRect.center();
                QPolygonF tri;
                tri << QPointF(center.x(), center.y() - 3)
                    << QPointF(center.x() - 4, center.y() + 2)
                    << QPointF(center.x() + 4, center.y() + 2);
                painter->drawPolygon(tri);
            }

            // Down triangle.
            {
                QPointF center = downRect.center();
                QPolygonF tri;
                tri << QPointF(center.x(), center.y() + 3)
                    << QPointF(center.x() - 4, center.y() - 2)
                    << QPointF(center.x() + 4, center.y() - 2);
                painter->drawPolygon(tri);
            }
        }

        painter->restore();
        return;
    }

    // Column 1 (value) and column 0 (key): normal text rendering.
    QStyledItemDelegate::paint(painter, option, index);
}

bool NumericValueDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index)
{
    // Only handle clicks on column 2.
    if (index.column() != 2)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    Mode mode = rowMode(index);
    if (mode == Text)
        return true;  // Consume event, no action in text mode.

    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonDblClick)
        return true;  // Consume other events on the interaction column.

    QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() != Qt::LeftButton)
        return true;

    QModelIndex valueIndex = index.sibling(index.row(), 1);
    QString text = valueIndex.data(Qt::DisplayRole).toString();

    if (mode == Boolean)
    {
        // Toggle between "true" and "false".
        QString val = text.trimmed().toLower();
        bool isTrue = (val == "true" || val == "1" || val == "yes");
        model->setData(valueIndex, isTrue ? "false" : "true");
    }
    else if (mode == Color)
    {
        // Open color dialog.
        QColor current(text.trimmed());
        if (!current.isValid())
            current = Qt::white;

        QWidget* parentWidget = qobject_cast<QWidget*>(parent());
        QColor color = QColorDialog::getColor(current, parentWidget, "Select Color");
        if (color.isValid())
            model->setData(valueIndex, color.name());
    }
    else if (mode == Cycle)
    {
        // Read cycle values from UserRole+1.
        QModelIndex keyIndex = index.sibling(index.row(), 0);
        QString cycleStr = keyIndex.data(Qt::UserRole + 1).toString();
        QStringList values = cycleStr.split("|", Qt::SkipEmptyParts);
        if (values.isEmpty())
            return true;

        int currentIdx = values.indexOf(text.trimmed());
        QPoint pos = mouseEvent->pos();
        QRect upRect = upArrowRect(option);
        QRect downRect = downArrowRect(option);

        if (upRect.contains(pos))
        {
            int next = (currentIdx < 0) ? 0 : (currentIdx + 1) % values.size();
            model->setData(valueIndex, values[next]);
        }
        else if (downRect.contains(pos))
        {
            int prev = (currentIdx <= 0) ? values.size() - 1 : currentIdx - 1;
            model->setData(valueIndex, values[prev]);
        }
    }
    else
    {
        // Integer or Decimal.
        if (!isNumeric(text))
            return true;

        QPoint pos = mouseEvent->pos();
        QRect upRect = upArrowRect(option);
        QRect downRect = downArrowRect(option);

        double step = (mode == Integer) ? 1.0 : 0.1;

        // Check for zero-padding in Integer mode.
        QString trimmed = text.trimmed();
        bool zeroPadded = false;
        int padWidth = 0;
        if (mode == Integer && trimmed.length() > 1 && trimmed[0] == '0')
        {
            bool allDigits = true;
            for (const QChar& c : trimmed)
            {
                if (!c.isDigit()) { allDigits = false; break; }
            }
            if (allDigits)
            {
                zeroPadded = true;
                padWidth = trimmed.length();
            }
        }

        if (upRect.contains(pos))
        {
            double val = trimmed.toDouble() + step;
            if (mode == Integer)
            {
                QString result = QString::number(static_cast<int>(val));
                if (zeroPadded)
                    result = result.rightJustified(padWidth, '0');
                model->setData(valueIndex, result);
            }
            else
                model->setData(valueIndex, QString::number(val, 'f', 1));
        }
        else if (downRect.contains(pos))
        {
            double val = trimmed.toDouble() - step;
            if (mode == Integer)
            {
                QString result = QString::number(static_cast<int>(val));
                if (zeroPadded && static_cast<int>(val) >= 0)
                    result = result.rightJustified(padWidth, '0');
                model->setData(valueIndex, result);
            }
            else
                model->setData(valueIndex, QString::number(val, 'f', 1));
        }
    }

    return true;  // Always consume events on the interaction column.
}

QWidget* NumericValueDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                             const QModelIndex& index) const
{
    // Never create an editor for the interaction column.
    if (index.column() == 2)
        return nullptr;

    return QStyledItemDelegate::createEditor(parent, option, index);
}
