#pragma once

#include <QtWidgets/QStyledItemDelegate>

class NumericValueDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    enum Mode { Text = 0, Integer = 1, Decimal = 2, Boolean = 3, Color = 4, Cycle = 5 };

    explicit NumericValueDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override;

private:
    Mode rowMode(const QModelIndex& index) const;
    QRect upArrowRect(const QStyleOptionViewItem& option) const;
    QRect downArrowRect(const QStyleOptionViewItem& option) const;
    bool isNumeric(const QString& text) const;
};
