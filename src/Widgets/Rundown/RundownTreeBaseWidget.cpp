#include "RundownTreeBaseWidget.h"
#include "RundownItemFactory.h"
#include "RundownGroupWidget.h"
#include "RundownUndoCommands.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "TriggerBankRegistry.h"
#include "Commands/AbstractCommand.h"
#include "Commands/MovieCommand.h"
#include "Commands/TemplateCommand.h"
#include "Events/Rundown/AllowRemoteTriggeringEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"
#include "Events/Rundown/RemoveItemFromAutoPlayQueueEvent.h"
#include "Events/Rundown/CurrentItemChangedEvent.h"
#include "Models/LibraryModel.h"

#include <iostream>

#include <QtCore/QDebug>
#include <QtCore/QUuid>

#include <QtGui/QDrag>
#include <QtGui/QPainter>
#include <QtGui/QClipboard>

#include <QtWidgets/QApplication>

// TreeSnapshotCommand implementation.
void TreeSnapshotCommand::undo() { m_tree->restoreFromSnapshot(m_beforeXml); }

void TreeSnapshotCommand::redo()
{
    // Skip the first redo — the operation has already been applied by the caller.
    if (m_firstRedo)
    {
        m_firstRedo = false;
        return;
    }
    m_tree->restoreFromSnapshot(m_afterXml);
}

RundownTreeBaseWidget* RundownTreeBaseWidget::dragSourceWidget = nullptr;
bool RundownTreeBaseWidget::s_isCutOperation = false;
QVector<QPointer<AbstractCommand>> RundownTreeBaseWidget::s_copiedCommands = {};

int RundownTreeBaseWidget::getItemDepth(QTreeWidgetItem* item)
{
    int depth = 0;
    QTreeWidgetItem* p = item ? item->parent() : nullptr;
    while (p) { depth++; p = p->parent(); }
    return depth;
}

RundownTreeBaseWidget::RundownTreeBaseWidget(QWidget* parent)
    : QTreeWidget(parent), compactView(false), theme(""), lock(false)
{
    this->theme = DatabaseManager::getInstance().getConfigurationByName("Theme").getValue();
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    this->m_undoStack = new QUndoStack(this);

    QString undoLimitStr = DatabaseManager::getInstance().getConfigurationByName("UndoHistoryLimit").getValue();
    this->m_undoStack->setUndoLimit(undoLimitStr.isEmpty() ? 50 : undoLimitStr.toInt());

    QObject::connect(&EventManager::getInstance(), SIGNAL(undoLimitChanged(int)), this, SLOT(undoLimitChanged(int)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent&)), this, SLOT(repositoryRundown(const RepositoryRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(channelChanged(const ChannelChangedEvent&)), this, SLOT(channelChanged(const ChannelChangedEvent&)));
}

QString RundownTreeBaseWidget::serializeTree() const
{
    QString data;
    QXmlStreamWriter writer(&data);

    writer.writeStartDocument();
    writer.writeStartElement("items");
    for (int i = 0; i < QTreeWidget::invisibleRootItem()->childCount(); i++)
        writeProperties(QTreeWidget::invisibleRootItem()->child(i), writer);
    writer.writeEndElement();
    writer.writeEndDocument();

    return data;
}

void RundownTreeBaseWidget::restoreFromSnapshot(const QString& xml)
{
    m_undoRestoring = true;

    setUpdatesEnabled(false);
    blockSignals(true);      // Prevent currentItemChanged/itemSelectionChanged with dangling pointers.
    removeAllItems();

    std::wstringstream wstringstream;
    wstringstream << xml.toStdWString();

    boost::property_tree::wptree pt;
    boost::property_tree::xml_parser::read_xml(wstringstream, pt);

    for (boost::property_tree::wptree::value_type& parentValue : pt.get_child(L"items"))
    {
        if (parentValue.first != L"item")
            continue;

        AbstractRundownWidget* parentWidget = readProperties(parentValue.second);
        parentWidget->setInGroup(false);
        parentWidget->setExpanded(false);

        QTreeWidgetItem* parentItem = new QTreeWidgetItem();
        QTreeWidget::invisibleRootItem()->addChild(parentItem);
        QTreeWidget::setItemWidget(parentItem, 0, dynamic_cast<QWidget*>(parentWidget));

        if (parentWidget->isGroup())
        {
            bool expanded = parentValue.second.get(L"expanded", false);
            parentItem->setExpanded(expanded);
            parentWidget->setExpanded(expanded);

            if (parentValue.second.count(L"items") > 0)
            {
                for (boost::property_tree::wptree::value_type& childValue : parentValue.second.get_child(L"items"))
                {
                    if (childValue.first != L"item")
                        continue;

                    AbstractRundownWidget* childWidget = readProperties(childValue.second);
                    childWidget->setInGroup(true);

                    QTreeWidgetItem* childItem = new QTreeWidgetItem();
                    parentItem->addChild(childItem);
                    QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));

                    // Handle inner groups (depth 2).
                    if (childWidget->isGroup())
                    {
                        bool childExpanded = childValue.second.get(L"expanded", false);
                        childItem->setExpanded(childExpanded);
                        childWidget->setExpanded(childExpanded);

                        if (childValue.second.count(L"items") > 0)
                        {
                            for (auto& gcValue : childValue.second.get_child(L"items"))
                            {
                                if (gcValue.first != L"item")
                                    continue;

                                AbstractRundownWidget* gcWidget = readProperties(gcValue.second);
                                gcWidget->setInGroup(true);

                                QTreeWidgetItem* gcItem = new QTreeWidgetItem();
                                childItem->addChild(gcItem);
                                QTreeWidget::setItemWidget(gcItem, 0, dynamic_cast<QWidget*>(gcWidget));
                            }
                        }
                    }
                }
            }

            updateGroupWidget(parentItem);
        }
    }

    // Restore bank assignments from loaded items.
    for (int i = 0; i < QTreeWidget::invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* item = QTreeWidget::invisibleRootItem()->child(i);
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget != nullptr && widget->getCommand() != nullptr)
        {
            int bank = widget->getCommand()->getTriggerBank();
            if (bank > 0)
                TriggerBankRegistry::getInstance().assign(bank, item);
        }
    }

    QTreeWidget::doItemsLayout();
    blockSignals(false);     // Re-enable signals now that the tree is rebuilt.
    setUpdatesEnabled(true);
    checkEmptyRundown();

    // Re-apply disabled visual to every item after rebuild.
    for (int i = 0; i < QTreeWidget::invisibleRootItem()->childCount(); i++)
        refreshDisabledVisual(QTreeWidget::invisibleRootItem()->child(i));

    m_undoRestoring = false;
}

void RundownTreeBaseWidget::beginUndoSnapshot(const QString& description)
{
    if (m_undoRestoring)
        return;

    m_undoNestingDepth++;

    // Only capture the "before" state for the outermost scope.
    if (m_undoNestingDepth == 1)
    {
        m_pendingUndoDescription = description;
        m_pendingUndoBefore = serializeTree();
    }
}

void RundownTreeBaseWidget::endUndoSnapshot()
{
    if (m_undoRestoring)
        return;

    if (m_undoNestingDepth <= 0)
        return;

    m_undoNestingDepth--;

    // Only push the command when the outermost scope ends.
    if (m_undoNestingDepth == 0)
    {
        QString after = serializeTree();
        if (m_pendingUndoBefore != after)
            m_undoStack->push(new TreeSnapshotCommand(this, m_pendingUndoDescription, m_pendingUndoBefore, after));

        m_pendingUndoBefore.clear();
        m_pendingUndoDescription.clear();
    }
}

bool RundownTreeBaseWidget::getCompactView() const
{
    return this->compactView;
}

void RundownTreeBaseWidget::setCompactView(bool compactView)
{
    this->compactView = compactView;
}

bool RundownTreeBaseWidget::isLocked() const
{
    return this->lock;
}

void RundownTreeBaseWidget::setLocked(bool locked)
{
    this->lock = locked;
}

void RundownTreeBaseWidget::writeProperties(QTreeWidgetItem* item, QXmlStreamWriter& writer) const
{
    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
    if (widget == nullptr || widget->getLibraryModel() == nullptr)
        return;

    if (widget->getLibraryModel()->getType() == "GROUP")
    {
        QString label = widget->getLibraryModel()->getLabel();

        writer.writeStartElement("item");
        writer.writeTextElement("type", widget->getLibraryModel()->getType());
        writer.writeTextElement("label", label);
        writer.writeTextElement("expanded", (item->isExpanded() == true ? "true" : "false"));
        widget->getCommand()->writeProperties(writer);
        widget->writeProperties(writer);

        writer.writeStartElement("items");
        for (int i = 0; i < item->childCount(); i++)
            writeProperties(item->child(i), writer);

        writer.writeEndElement();
        writer.writeEndElement();
    }
    else
    {
        QString deviceName = widget->getLibraryModel()->getDeviceName();
        QString label = widget->getLibraryModel()->getLabel();
        QString name = widget->getLibraryModel()->getName();

        writer.writeStartElement("item");
        writer.writeTextElement("type", widget->getLibraryModel()->getType());
        writer.writeTextElement("devicename", deviceName);
        writer.writeTextElement("label", label);
        writer.writeTextElement("name", name);
        widget->getCommand()->writeProperties(writer);
        widget->writeProperties(writer);
        writer.writeEndElement();
    }
}

AbstractRundownWidget* RundownTreeBaseWidget::readProperties(boost::property_tree::wptree& pt)
{
    QString type = QString::fromStdWString(pt.get(L"type", L""));

    AbstractRundownWidget* widget = NULL;
    if (type == "GROUP")
    {
        QString label = QString::fromStdWString(pt.get(L"label", L""));

        widget = new RundownGroupWidget(LibraryModel(0, label, "", "", type, 0, ""), this);
        widget->setExpanded(true);
        widget->setCompactView(this->compactView);
        widget->getCommand()->readProperties(pt);
        widget->readProperties(pt);
    }
    else
    {
        QString deviceName = QString::fromStdWString(pt.get(L"devicename", L""));
        QString label = QString::fromStdWString(pt.get(L"label", L""));
        QString name = QString::fromStdWString(pt.get(L"name", L""));

        widget = RundownItemFactory::getInstance().createWidget(LibraryModel(0, label, name, deviceName, type, 0, ""));
        widget->setCompactView(this->compactView);
        widget->getCommand()->readProperties(pt);
        widget->readProperties(pt);
    }

    if (widget->isGroup())
    {
        if (this->compactView)
            dynamic_cast<QWidget*>(widget)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
        else
            dynamic_cast<QWidget*>(widget)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);
    }
    else
    {
        if (this->compactView)
            dynamic_cast<QWidget*>(widget)->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
        else
            dynamic_cast<QWidget*>(widget)->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
    }

    return widget;
}

bool RundownTreeBaseWidget::copySelectedItems()
{
    s_isCutOperation = false;
    s_copiedCommands.clear();

    QString data;
    QXmlStreamWriter writer(&data);

    writer.setAutoFormatting(XmlFormatting::ENABLE_FORMATTING);
    writer.setAutoFormattingIndent(XmlFormatting::NUMBER_OF_SPACES);

    writer.writeStartDocument();
    writer.writeStartElement("items");
    for (int i = 0; i < QTreeWidget::selectedItems().count(); i++)
    {
        QTreeWidgetItem* item = QTreeWidget::selectedItems().at(i);
        writeProperties(item, writer);

        // Store source command pointers in the same flat order as XML serialization.
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget && widget->getCommand())
        {
            s_copiedCommands.append(QPointer<AbstractCommand>(widget->getCommand()));

            // For group items, also store child commands.
            if (widget->isGroup())
            {
                for (int j = 0; j < item->childCount(); j++)
                {
                    AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item->child(j), 0));
                    if (childWidget && childWidget->getCommand())
                        s_copiedCommands.append(QPointer<AbstractCommand>(childWidget->getCommand()));
                }
            }
        }
    }

    writer.writeEndElement();
    writer.writeEndDocument();

    qApp->clipboard()->setText(data);

    return true;
}

void RundownTreeBaseWidget::copyItemProperties()
{
    copySelectedItems();
}

bool RundownTreeBaseWidget::pasteItemProperties()
{
    qDebug(qPrintable(qApp->clipboard()->text()));

    std::wstringstream wstringstream;
    wstringstream << qApp->clipboard()->text().toStdWString();

    boost::property_tree::wptree pt;
    boost::property_tree::xml_parser::read_xml(wstringstream, pt);

    for (boost::property_tree::wptree::value_type &parentValue : pt.get_child(L"items"))
    {
        QString type = QString::fromStdWString(parentValue.second.get(L"type", L""));

        foreach (QTreeWidgetItem* item, QTreeWidget::selectedItems())
        {
            AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
            if (widget->getLibraryModel()->getType() == type)
                widget->getCommand()->readProperties(parentValue.second);
        }
    }

    return true;
}

bool RundownTreeBaseWidget::pasteItemPropertiesNoData()
{
    std::wstringstream wstringstream;
    wstringstream << qApp->clipboard()->text().toStdWString();

    boost::property_tree::wptree pt;
    boost::property_tree::xml_parser::read_xml(wstringstream, pt);

    for (boost::property_tree::wptree::value_type &parentValue : pt.get_child(L"items"))
    {
        QString type = QString::fromStdWString(parentValue.second.get(L"type", L""));

        foreach (QTreeWidgetItem* item, QTreeWidget::selectedItems())
        {
            AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
            if (widget->getLibraryModel()->getType() == type)
            {
                // Save template data models before pasting properties.
                TemplateCommand* templateCmd = dynamic_cast<TemplateCommand*>(widget->getCommand());
                QList<KeyValueModel> savedModels;
                if (templateCmd != nullptr)
                    savedModels = templateCmd->getTemplateDataModels();

                widget->getCommand()->readProperties(parentValue.second);

                // Restore template data models so variables are preserved.
                if (templateCmd != nullptr)
                    templateCmd->setTemplateDataModels(savedModels);
            }
        }
    }

    return true;
}

bool RundownTreeBaseWidget::pasteSelectedItems(bool repositoryRundown, bool preserveCloneLinks)
{
    UndoScope undo(this, "Paste Items");

    std::wstringstream wstringstream;
    wstringstream << qApp->clipboard()->text().toStdWString();

    int offset = 1; // Drop offset.
    boost::property_tree::wptree pt;
    boost::property_tree::xml_parser::read_xml(wstringstream, pt);

    if (pt.get_child(L"items").count(L"allowremotetriggering") > 0)
    {
        bool allowRemoteTriggering = pt.get_child(L"items").get(L"allowremotetriggering", false);
        EventManager::getInstance().fireAllowRemoteTriggeringEvent(AllowRemoteTriggeringEvent(allowRemoteTriggering));
    }

    EventManager::getInstance().fireRepositoryRundownEvent(RepositoryRundownEvent(repositoryRundown));

    setUpdatesEnabled(false);

    for (boost::property_tree::wptree::value_type &parentValue : pt.get_child(L"items"))
    {
        if (parentValue.first != L"item")
            continue;

        AbstractRundownWidget* parentWidget = readProperties(parentValue.second);

        // Clear clone links on pasted items so they become independent copies.
        if (!preserveCloneLinks)
            parentWidget->getCommand()->setCloneGroupId("");

        int row  = QTreeWidget::currentIndex().row();

        QTreeWidgetItem* parentItem = new QTreeWidgetItem();
        if (QTreeWidget::currentItem() == NULL || QTreeWidget::currentItem()->parent() == NULL) // Top level item.
        {
            parentWidget->setInGroup(false);
            parentWidget->setExpanded(false);

            // If we don't have a selected row then we add the item to the bottom of the
            // rundown. This can be the case when we drag and drop a preset to the rundown.
            if (row != -1)
                QTreeWidget::invisibleRootItem()->insertChild(row + offset++, parentItem);
            else
                QTreeWidget::invisibleRootItem()->addChild(parentItem);
        }
        else
        {
            // Pasting inside a group. Check depth constraints for groups.
            int pasteDepth = getItemDepth(QTreeWidget::currentItem());

            if (parentWidget->isGroup())
            {
                // Block pasting groups into inner groups (depth >= 2).
                if (pasteDepth >= 2)
                    continue;

                // Block pasting groups that contain inner groups (would exceed depth).
                bool hasInnerGroups = false;
                if (parentValue.second.count(L"items") > 0)
                {
                    for (auto& cv : parentValue.second.get_child(L"items"))
                    {
                        QString childType = QString::fromStdWString(cv.second.get(L"type", L""));
                        if (childType == "GROUP") { hasInnerGroups = true; break; }
                    }
                }
                if (hasInnerGroups)
                    continue;
            }

            parentWidget->setInGroup(true);

            QTreeWidget::currentItem()->parent()->insertChild(row + offset++, parentItem);
        }

        QTreeWidget::setItemWidget(parentItem, 0, dynamic_cast<QWidget*>(parentWidget));
        //QTreeWidget::setCurrentItem(parentItem);

        if (parentWidget->isGroup())
        {
            bool expanded = parentValue.second.get(L"expanded", false);
            parentItem->setExpanded(expanded);
            parentWidget->setExpanded(expanded);

            for (boost::property_tree::wptree::value_type &childValue : parentValue.second.get_child(L"items"))
            {
                if (childValue.first != L"item")
                    continue;

                AbstractRundownWidget* childWidget = readProperties(childValue.second);

                if (!preserveCloneLinks)
                    childWidget->getCommand()->setCloneGroupId("");

                childWidget->setInGroup(true);

                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                parentItem->addChild(childItem);

                QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));

                // If the child is itself a group (inner group), process its children.
                if (childWidget->isGroup())
                {
                    bool childExpanded = childValue.second.get(L"expanded", false);
                    childItem->setExpanded(childExpanded);
                    childWidget->setExpanded(childExpanded);

                    if (getCompactView())
                        dynamic_cast<QWidget*>(childWidget)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
                    else
                        dynamic_cast<QWidget*>(childWidget)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);

                    if (childValue.second.count(L"items") > 0)
                    {
                        for (auto& grandchildValue : childValue.second.get_child(L"items"))
                        {
                            if (grandchildValue.first != L"item")
                                continue;

                            AbstractRundownWidget* gcWidget = readProperties(grandchildValue.second);

                            if (!preserveCloneLinks)
                                gcWidget->getCommand()->setCloneGroupId("");

                            gcWidget->setInGroup(true);

                            QTreeWidgetItem* gcItem = new QTreeWidgetItem();
                            childItem->addChild(gcItem);

                            QTreeWidget::setItemWidget(gcItem, 0, dynamic_cast<QWidget*>(gcWidget));
                        }
                    }

                    updateGroupWidget(childItem);
                }
            }
        }

    }

    QTreeWidget::doItemsLayout();
    setUpdatesEnabled(true);

    // Restore bank assignments from loaded items.
    for (int i = 0; i < QTreeWidget::invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* item = QTreeWidget::invisibleRootItem()->child(i);
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget != nullptr && widget->getCommand() != nullptr)
        {
            int bank = widget->getCommand()->getTriggerBank();
            if (bank > 0 && TriggerBankRegistry::getInstance().getItem(bank) == nullptr)
            {
                TriggerBankRegistry::getInstance().assign(bank, item);
            }
            else if (bank > 0 && TriggerBankRegistry::getInstance().getItem(bank) != item)
            {
                widget->getCommand()->setTriggerBank(0); // Pasted duplicate, clear it.
            }
        }

        // Also check children (group items).
        for (int j = 0; j < item->childCount(); j++)
        {
            QTreeWidgetItem* childItem = item->child(j);
            AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(childItem, 0));
            if (childWidget != nullptr && childWidget->getCommand() != nullptr)
            {
                int bank = childWidget->getCommand()->getTriggerBank();
                if (bank > 0 && TriggerBankRegistry::getInstance().getItem(bank) == nullptr)
                {
                    TriggerBankRegistry::getInstance().assign(bank, childItem);
                }
                else if (bank > 0 && TriggerBankRegistry::getInstance().getItem(bank) != childItem)
                {
                    childWidget->getCommand()->setTriggerBank(0);
                }
            }
        }
    }

    updateAllGroupWidgets();
    checkEmptyRundown();

    emit itemsChanged();

    return true;
}

bool RundownTreeBaseWidget::pasteAsLinkedClones()
{
    // Ensure every source command gets a clone group ID so pasted items can link back.
    for (int i = 0; i < s_copiedCommands.size(); i++)
    {
        auto& ptr = s_copiedCommands[i];
        if (!ptr.isNull() && ptr->getCloneGroupId().isEmpty())
            ptr->setCloneGroupId(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    // Update the clipboard XML to include clone group IDs from source commands.
    QString clipText = qApp->clipboard()->text();
    if (clipText.isEmpty())
        return false;

    try
    {
        std::wstringstream in;
        in << clipText.toStdWString();

        boost::property_tree::wptree pt;
        boost::property_tree::xml_parser::read_xml(in, pt);

        int cmdIndex = 0;
        for (auto& parentValue : pt.get_child(L"items"))
        {
            if (parentValue.first != L"item")
                continue;

            if (cmdIndex < s_copiedCommands.size() && !s_copiedCommands[cmdIndex].isNull())
            {
                QString id = s_copiedCommands[cmdIndex]->getCloneGroupId();
                parentValue.second.put(L"clonegroupid", id.toStdWString());
            }
            cmdIndex++;

            // Handle children inside group items.
            auto childItems = parentValue.second.get_child_optional(L"items");
            if (childItems)
            {
                for (auto& childValue : *childItems)
                {
                    if (childValue.first != L"item")
                        continue;

                    if (cmdIndex < s_copiedCommands.size() && !s_copiedCommands[cmdIndex].isNull())
                    {
                        QString id = s_copiedCommands[cmdIndex]->getCloneGroupId();
                        childValue.second.put(L"clonegroupid", id.toStdWString());
                    }
                    cmdIndex++;
                }
            }
        }

        // Write updated XML back to clipboard.
        std::wstringstream out;
        boost::property_tree::xml_parser::write_xml(out, pt);
        qApp->clipboard()->setText(QString::fromStdWString(out.str()));
    }
    catch (...)
    {
        return false;
    }

    // Paste with clone links preserved.
    return pasteSelectedItems(false, true);
}

bool RundownTreeBaseWidget::duplicateSelectedItems()
{
    UndoScope undo(this, "Duplicate Items");

    // Save the latest value stored in the clipboard.
    QString latest = qApp->clipboard()->text();

    if (!copySelectedItems())
        return true;

    if (!pasteSelectedItems())
        return true;

    // Set previous stored clipboard value.
    qApp->clipboard()->setText(latest);

    return true;
}

void RundownTreeBaseWidget::checkEmptyRundown()
{
    if (this->theme == Appearance::CURVE_THEME)
        QTreeWidget::setStyleSheet((QTreeWidget::invisibleRootItem()->childCount() == 0) ? "#treeWidgetRundown { border-width: 1; border-color: firebrick; }" : "#treeWidgetRundown { border-width: 1; }");
    else
        QTreeWidget::setStyleSheet((QTreeWidget::invisibleRootItem()->childCount() == 0) ? "#treeWidgetRundown { border-width: 1; border-color: firebrick; }" : "#treeWidgetRundown { border-width: 0; border-top-width: 1; }");
}

void RundownTreeBaseWidget::removeSelectedItems()
{
    UndoScope undo(this, "Remove Items");

    foreach (QTreeWidgetItem* item, QTreeWidget::selectedItems())
    {
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget->isGroup())
        {
            for (int i = item->childCount() - 1; i >= 0; i--)
            {
                QTreeWidgetItem* child = item->child(i);
                QWidget* childWidget = QTreeWidget::itemWidget(child, 0);

                // Remove bank assignments for grandchildren (inner groups).
                for (int k = 0; k < child->childCount(); k++)
                    TriggerBankRegistry::getInstance().unassignByItem(child->child(k));

                // Remove bank assignment if any.
                TriggerBankRegistry::getInstance().unassignByItem(child);

                // Remove our items from the auto play queue if it exists.
                EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(child));

                // Clear current playing item.
                EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(child));

                delete childWidget;
                delete child;
            }
        }

        // Remove bank assignment if any.
        TriggerBankRegistry::getInstance().unassignByItem(item);

        // Remove our items from the AutoPlay queue if it exists.
        EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item));

        // Clear current playing item.
        EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item));

        delete widget;
        delete item;
    }

    updateAllGroupWidgets();
    checkEmptyRundown();
}

void RundownTreeBaseWidget::removeAllItems()
{
    // Only unassign banks belonging to this tree, so the other pane's assignments survive.
    for (int i = 0; i < QTreeWidget::invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* item = QTreeWidget::invisibleRootItem()->child(i);
        TriggerBankRegistry::getInstance().unassignByItem(item);
        for (int j = 0; j < item->childCount(); j++)
        {
            TriggerBankRegistry::getInstance().unassignByItem(item->child(j));
            for (int k = 0; k < item->child(j)->childCount(); k++)
                TriggerBankRegistry::getInstance().unassignByItem(item->child(j)->child(k));
        }
    }

    for (int i = QTreeWidget::invisibleRootItem()->childCount() - 1; i >= 0; i--)
    {
        QTreeWidgetItem* item = QTreeWidget::invisibleRootItem()->child(i);
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));

        if (widget != nullptr && widget->isGroup())
        {
            for (int j = item->childCount() - 1; j >= 0; j--)
            {
                QTreeWidgetItem* childItem = item->child(j);
                QWidget* childWidget = QTreeWidget::itemWidget(childItem, 0);

                // Handle inner groups (depth 2): clean up grandchildren first.
                AbstractRundownWidget* childRundown = dynamic_cast<AbstractRundownWidget*>(childWidget);
                if (childRundown != nullptr && childRundown->isGroup())
                {
                    for (int k = childItem->childCount() - 1; k >= 0; k--)
                    {
                        QWidget* gcWidget = QTreeWidget::itemWidget(childItem->child(k), 0);
                        EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(childItem->child(k)));
                        EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(childItem->child(k)));
                        delete gcWidget;
                        delete childItem->child(k);
                    }
                }

                EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(childItem));
                EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(childItem));

                delete childWidget;
                delete childItem;
            }
        }

        EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item));
        EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item));

        delete widget;
        delete item;
    }

    checkEmptyRundown();
}

void RundownTreeBaseWidget::toggleDisableSelectedItems()
{
    UndoScope undo(this, "Toggle Disable Items");
    QList<QTreeWidgetItem*> selected = QTreeWidget::selectedItems();
    if (selected.isEmpty())
        return;

    // If any selected item is enabled, disable all; otherwise enable all (toggle by majority).
    bool anyEnabled = false;
    for (QTreeWidgetItem* item : selected)
    {
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget != nullptr && widget->getCommand() != nullptr && !widget->getCommand()->getDisabled())
        {
            anyEnabled = true;
            break;
        }
    }

    bool newState = anyEnabled; // disable if any enabled, else enable all

    for (QTreeWidgetItem* item : selected)
    {
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget != nullptr && widget->getCommand() != nullptr)
        {
            widget->getCommand()->setDisabled(newState);
            refreshDisabledVisual(item);
        }
    }
}

void RundownTreeBaseWidget::applyDisabledVisual(QTreeWidgetItem* item, bool effectiveDisabled)
{
    if (item == nullptr)
        return;
    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
    if (widget == nullptr)
        return;
    widget->setRundownDisabled(effectiveDisabled);
}

void RundownTreeBaseWidget::refreshDisabledVisual(QTreeWidgetItem* item)
{
    if (item == nullptr)
        return;

    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
    if (widget == nullptr || widget->getCommand() == nullptr)
        return;

    bool ownDisabled = widget->getCommand()->getDisabled();

    // Check parent group disabled state (only depth-1 cascade — children of disabled group).
    bool parentDisabled = false;
    QTreeWidgetItem* parent = item->parent();
    while (parent != nullptr)
    {
        AbstractRundownWidget* parentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(parent, 0));
        if (parentWidget != nullptr && parentWidget->getCommand() != nullptr && parentWidget->getCommand()->getDisabled())
        {
            parentDisabled = true;
            break;
        }
        parent = parent->parent();
    }

    bool effective = ownDisabled || parentDisabled;
    applyDisabledVisual(item, effective);

    // If this item is a group, cascade refresh to all descendants so their visual is up to date.
    if (widget->isGroup())
    {
        for (int i = 0; i < item->childCount(); i++)
        {
            QTreeWidgetItem* child = item->child(i);
            refreshDisabledVisual(child);
            if (dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(child, 0)) != nullptr &&
                dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(child, 0))->isGroup())
            {
                for (int j = 0; j < child->childCount(); j++)
                    refreshDisabledVisual(child->child(j));
            }
        }
    }
}

void RundownTreeBaseWidget::groupItems()
{
    UndoScope undo(this, "Group Items");
    if (QTreeWidget::currentItem() == NULL)
        return;

    QList<QTreeWidgetItem*> selected = QTreeWidget::selectedItems();
    if (selected.isEmpty())
        return;

    // All items must be at the same depth and same parent.
    int depth = getItemDepth(selected.at(0));
    QTreeWidgetItem* commonParent = selected.at(0)->parent();
    for (QTreeWidgetItem* item : selected)
    {
        if (getItemDepth(item) != depth || item->parent() != commonParent)
            return;
    }

    // Items at depth 2 can't be grouped (would exceed max 2-level nesting).
    if (depth >= 2)
        return;

    // Validate group items in the selection.
    for (QTreeWidgetItem* item : selected)
    {
        QWidget* w = QTreeWidget::itemWidget(item, 0);
        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
        if (rw && rw->isGroup())
        {
            // Groups at depth 1 can't be grouped (would create a depth-2 group).
            if (depth >= 1)
                return;

            // Groups at depth 0: block if they contain inner groups (would exceed depth).
            for (int i = 0; i < item->childCount(); i++)
            {
                QWidget* cw = QTreeWidget::itemWidget(item->child(i), 0);
                AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
                if (crw && crw->isGroup())
                    return;
            }
        }
    }

    // Create the new group.
    QTreeWidgetItem* parentItem = new QTreeWidgetItem();
    RundownGroupWidget* groupWidget = new RundownGroupWidget(LibraryModel(0, "Group", "", "", "GROUP", 0, ""), this);
    groupWidget->setActive(true);
    groupWidget->setExpanded(true);
    groupWidget->setCompactView(getCompactView());

    if (depth == 0)
    {
        // Top-level grouping.
        int row = QTreeWidget::indexOfTopLevelItem(selected.at(0));
        QTreeWidget::invisibleRootItem()->insertChild(row, parentItem);
    }
    else
    {
        // Depth 1: create inner group inside the parent group.
        groupWidget->setInGroup(true);
        int row = commonParent->indexOfChild(selected.at(0));
        commonParent->insertChild(row, parentItem);
    }

    QTreeWidget::setItemWidget(parentItem, 0, dynamic_cast<QWidget*>(groupWidget));
    QTreeWidget::expandItem(parentItem);

    if (getCompactView())
        dynamic_cast<QWidget*>(groupWidget)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
    else
        dynamic_cast<QWidget*>(groupWidget)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);

    // Clone selected items into the new group.
    for (QTreeWidgetItem* item : selected)
    {
        AbstractRundownWidget* srcWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));

        if (srcWidget->isGroup())
        {
            // Clone the group and all its children as an inner group.
            QTreeWidgetItem* innerGroupItem = new QTreeWidgetItem();
            parentItem->addChild(innerGroupItem);

            AbstractRundownWidget* innerWidget = srcWidget->clone();
            innerWidget->setInGroup(true);
            innerWidget->setActive(false);

            QTreeWidget::setItemWidget(innerGroupItem, 0, dynamic_cast<QWidget*>(innerWidget));

            if (item->isExpanded())
                QTreeWidget::expandItem(innerGroupItem);
            innerWidget->setExpanded(item->isExpanded());

            if (getCompactView())
                dynamic_cast<QWidget*>(innerWidget)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
            else
                dynamic_cast<QWidget*>(innerWidget)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);

            // Clone all children of the original group.
            for (int i = 0; i < item->childCount(); i++)
            {
                QTreeWidgetItem* grandchildItem = new QTreeWidgetItem();
                innerGroupItem->addChild(grandchildItem);

                AbstractRundownWidget* grandchildWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item->child(i), 0))->clone();
                grandchildWidget->setInGroup(true);
                grandchildWidget->setActive(false);

                QTreeWidget::setItemWidget(grandchildItem, 0, dynamic_cast<QWidget*>(grandchildWidget));

                if (getCompactView())
                    dynamic_cast<QWidget*>(grandchildWidget)->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
                else
                    dynamic_cast<QWidget*>(grandchildWidget)->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
            }

            updateGroupWidget(innerGroupItem);
        }
        else
        {
            // Regular item: clone into new group.
            QTreeWidgetItem* childItem = new QTreeWidgetItem();
            parentItem->addChild(childItem);

            AbstractRundownWidget* childWidget = srcWidget->clone();
            childWidget->setInGroup(true);
            childWidget->setActive(false);

            QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));
        }
    }

    removeSelectedItems();

    updateGroupWidget(parentItem);

    // If inner group was created inside an outer group, update the outer group too.
    if (commonParent != nullptr)
        updateGroupWidget(commonParent);

    QTreeWidget::doItemsLayout(); // Refresh
    QTreeWidget::setCurrentItem(parentItem);
}

void RundownTreeBaseWidget::ungroupItems()
{
    UndoScope undo(this, "Ungroup Items");
    if (QTreeWidget::currentItem() == NULL)
        return;

    AbstractRundownWidget* currentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(QTreeWidget::currentItem(), 0));

    if (currentWidget->isGroup())
    {
        // Ungrouping a group: dissolve it, promote children to the parent level.
        QTreeWidgetItem* currentItem = QTreeWidget::currentItem();
        QTreeWidgetItem* currentItemAbove = QTreeWidget::itemAbove(currentItem);
        QTreeWidgetItem* outerParent = currentItem->parent(); // nullptr for top-level groups
        int depth = getItemDepth(currentItem);

        QTreeWidgetItem* insertTarget = outerParent ? outerParent : QTreeWidget::invisibleRootItem();
        int row = outerParent ? outerParent->indexOfChild(currentItem)
                              : QTreeWidget::indexOfTopLevelItem(currentItem);
        bool childrenStayInGroup = (outerParent != nullptr); // Children remain in a group if outer group exists.

        QTreeWidgetItem* newItem = NULL;
        for (int i = 0; i < currentItem->childCount(); i++)
        {
            QTreeWidgetItem* item = currentItem->child(i);
            AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));

            newItem = new QTreeWidgetItem();
            insertTarget->insertChild(row + 1, newItem);

            AbstractRundownWidget* newWidget = childWidget->clone();
            newWidget->setInGroup(childrenStayInGroup);
            newWidget->setActive(false);

            QTreeWidget::setItemWidget(newItem, 0, dynamic_cast<QWidget*>(newWidget));

            // If child is an inner group, also clone its children.
            if (childWidget->isGroup())
            {
                if (item->isExpanded())
                    QTreeWidget::expandItem(newItem);
                newWidget->setExpanded(item->isExpanded());

                if (getCompactView())
                    dynamic_cast<QWidget*>(newWidget)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
                else
                    dynamic_cast<QWidget*>(newWidget)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);

                for (int j = 0; j < item->childCount(); j++)
                {
                    QTreeWidgetItem* grandchild = item->child(j);
                    QTreeWidgetItem* newGrandchild = new QTreeWidgetItem();
                    newItem->addChild(newGrandchild);

                    AbstractRundownWidget* gcWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(grandchild, 0))->clone();
                    gcWidget->setInGroup(true);
                    gcWidget->setActive(false);
                    QTreeWidget::setItemWidget(newGrandchild, 0, dynamic_cast<QWidget*>(gcWidget));

                    if (getCompactView())
                        dynamic_cast<QWidget*>(gcWidget)->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
                    else
                        dynamic_cast<QWidget*>(gcWidget)->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
                }

                updateGroupWidget(newItem);
            }

            row++;
        }

        QTreeWidget::setCurrentItem(currentItemAbove);

        EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(currentItem));
        EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(currentItem));
        delete currentItem;

        // Update the outer group if we ungrouped an inner group.
        if (outerParent != nullptr)
            updateGroupWidget(outerParent);
    }
    else if (QTreeWidget::currentItem()->parent() != NULL)
    {
        // Selected non-group items within a group: move them out to parent level.
        QTreeWidgetItem* parentItem = QTreeWidget::currentItem()->parent();
        QTreeWidgetItem* grandparent = parentItem->parent();
        int depth = getItemDepth(QTreeWidget::currentItem());

        QTreeWidgetItem* insertTarget;
        int insertRow;
        bool stillInGroup;

        if (depth == 2 && grandparent != nullptr)
        {
            // In inner group: move to outer group.
            insertTarget = grandparent;
            insertRow = grandparent->indexOfChild(parentItem);
            stillInGroup = true;
        }
        else
        {
            // In top-level group: move to top level.
            insertTarget = QTreeWidget::invisibleRootItem();
            insertRow = QTreeWidget::indexOfTopLevelItem(parentItem);
            stillInGroup = false;
        }

        QTreeWidgetItem* newItem = NULL;
        foreach (QTreeWidgetItem* item, QTreeWidget::selectedItems())
        {
            newItem = new QTreeWidgetItem();
            insertTarget->insertChild(insertRow + 1, newItem);

            AbstractRundownWidget* newWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0))->clone();
            newWidget->setInGroup(stillInGroup);
            newWidget->setActive(false);

            QTreeWidget::setItemWidget(newItem, 0, dynamic_cast<QWidget*>(newWidget));

            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item));
            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item));
            delete item;

            insertRow++;
        }

        QTreeWidget::setCurrentItem(newItem);

        if (parentItem->childCount() == 0)
        {
            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(parentItem));
            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(parentItem));
            delete parentItem;
        }

        // Update the outer group if applicable.
        if (grandparent != nullptr)
            updateGroupWidget(grandparent);
    }

    updateAllGroupWidgets();
    QTreeWidget::doItemsLayout(); // Refresh
}

void RundownTreeBaseWidget::moveItemUp()
{
    UndoScope undo(this, "Move Up");
    if (QTreeWidget::currentItem() == NULL)
        return;

    int row  = QTreeWidget::currentIndex().row();
    QTreeWidgetItem* currentItem = QTreeWidget::currentItem();
    QTreeWidgetItem* parentItem = QTreeWidget::currentItem()->parent();

    if (dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0))->isGroup())
    {
        int rowCount = 0;
        if (currentItem != NULL && row > rowCount)
        {
            AbstractRundownWidget* parentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0))->clone();
            parentWidget->setInGroup(true);
            parentWidget->setExpanded(true);

            QTreeWidgetItem* parentItem = new QTreeWidgetItem();
            QTreeWidget::invisibleRootItem()->insertChild(row - 1, parentItem);
            QTreeWidget::setItemWidget(parentItem, 0, dynamic_cast<QWidget*>(parentWidget));

            if (QTreeWidget::currentItem()->isExpanded())
                QTreeWidget::expandItem(parentItem);

            for (int i = 0; i < QTreeWidget::currentItem()->childCount(); i++)
            {
                QTreeWidgetItem* item = QTreeWidget::currentItem()->child(i);

                AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0))->clone();
                childWidget->setInGroup(true);

                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                parentItem->addChild(childItem);
                QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));
            }

            // Remove our items from the auto play queue if it exists.
            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(currentItem));

            // Clear current playing item.
            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(currentItem));

            delete currentItem;

            QTreeWidget::setCurrentItem(parentItem);
            updateAllGroupWidgets();
            QTreeWidget::doItemsLayout(); // Refresh
        }
    }
    else
    {
        int rowCount = 0;
        if (currentItem != NULL && row > rowCount)
        {
            AbstractRundownWidget* newWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0))->clone();

            if (parentItem == NULL) // Top level item.
            {
                QTreeWidget::invisibleRootItem()->takeChild(row);
                QTreeWidget::invisibleRootItem()->insertChild(row - 1, currentItem);
            }
            else // Group item.
            {
                newWidget->setInGroup(true);

                QTreeWidget::currentItem()->parent()->takeChild(row);
                QTreeWidget::currentItem()->parent()->insertChild(row - 1, currentItem);
            }

            QTreeWidget::setItemWidget(currentItem, 0, dynamic_cast<QWidget*>(newWidget));
            QTreeWidget::setCurrentItem(currentItem);
            updateAllGroupWidgets();
            QTreeWidget::doItemsLayout(); // Refresh
        }
    }
}

void RundownTreeBaseWidget::moveItemDown()
{
    UndoScope undo(this, "Move Down");
    if (QTreeWidget::currentItem() == NULL)
        return;

    int row  = QTreeWidget::currentIndex().row();
    QTreeWidgetItem* currentItem = QTreeWidget::currentItem();
    QTreeWidgetItem* parentItem = QTreeWidget::currentItem()->parent();

    if (dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0))->isGroup())
    {
        int rowCount = 0;
        if (parentItem == NULL) // Top level item.
            rowCount = QTreeWidget::invisibleRootItem()->childCount() - 1;

        if (currentItem != NULL && row < rowCount)
        {
            AbstractRundownWidget* parentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0))->clone();
            parentWidget->setInGroup(true);
            parentWidget->setExpanded(true);

            QTreeWidgetItem* parentItem = new QTreeWidgetItem();
            QTreeWidget::invisibleRootItem()->insertChild(row + 2, parentItem);
            QTreeWidget::setItemWidget(parentItem, 0, dynamic_cast<QWidget*>(parentWidget));

            if (QTreeWidget::currentItem()->isExpanded())
                QTreeWidget::expandItem(parentItem);

            for (int i = 0; i < QTreeWidget::currentItem()->childCount(); i++)
            {
                QTreeWidgetItem* item = QTreeWidget::currentItem()->child(i);

                AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0))->clone();
                childWidget->setInGroup(true);

                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                parentItem->addChild(childItem);
                QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));
            }

            // Remove our items from the auto play queue if it exists.
            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(currentItem));

            // Clear current playing item.
            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(currentItem));

            delete currentItem;

            QTreeWidget::setCurrentItem(parentItem);
            updateAllGroupWidgets();
            QTreeWidget::doItemsLayout(); // Refresh
        }
    }
    else
    {
        int rowCount = 0;
        if (parentItem == NULL) // Top level item.
            rowCount = QTreeWidget::invisibleRootItem()->childCount() - 1;
        else
            rowCount = QTreeWidget::currentItem()->parent()->childCount() - 1;

        if (currentItem != NULL && row < rowCount)
        {
            AbstractRundownWidget* newWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0))->clone();

            if (parentItem == NULL) // Top level item.
            {
                QTreeWidget::invisibleRootItem()->takeChild(row);
                QTreeWidget::invisibleRootItem()->insertChild(row + 1, currentItem);
            }
            else // Group item.
            {
                newWidget->setInGroup(true);

                QTreeWidget::currentItem()->parent()->takeChild(row);
                QTreeWidget::currentItem()->parent()->insertChild(row + 1, currentItem);
            }

            QTreeWidget::setItemWidget(currentItem, 0, dynamic_cast<QWidget*>(newWidget));
            QTreeWidget::setCurrentItem(currentItem);
            updateAllGroupWidgets();
            QTreeWidget::doItemsLayout(); // Refresh
        }
    }
}

void RundownTreeBaseWidget::moveItemOutOfGroup()
{
    UndoScope undo(this, "Move Out of Group");
    if (QTreeWidget::currentItem() == NULL || QTreeWidget::currentItem()->parent() == NULL) // Top level item.
        return;

    QTreeWidgetItem* currentItem = QTreeWidget::currentItem();
    QTreeWidgetItem* parentItem = currentItem->parent(); // The group containing this item.
    QTreeWidgetItem* grandparent = parentItem->parent(); // nullptr if parent is top-level group.
    QTreeWidgetItem* parentItemAbove = QTreeWidget::itemAbove(parentItem);
    AbstractRundownWidget* currentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0));

    int currentRow = parentItem->indexOfChild(currentItem);

    // Determine where to insert: one level up from the parent group.
    QTreeWidgetItem* insertTarget;
    int insertRow;
    bool stillInGroup;

    if (grandparent != nullptr)
    {
        // Parent is an inner group: move to outer group as sibling of inner group.
        insertTarget = grandparent;
        insertRow = grandparent->indexOfChild(parentItem);
        stillInGroup = true;
    }
    else
    {
        // Parent is a top-level group: move to top level.
        insertTarget = QTreeWidget::invisibleRootItem();
        insertRow = QTreeWidget::indexOfTopLevelItem(parentItem);
        stillInGroup = false;
    }

    QTreeWidgetItem* newItem = new QTreeWidgetItem();
    AbstractRundownWidget* newWidget = currentWidget->clone();
    newWidget->setInGroup(stillInGroup);

    parentItem->takeChild(currentRow);
    insertTarget->insertChild(insertRow + 1, newItem);
    QTreeWidget::setItemWidget(newItem, 0, dynamic_cast<QWidget*>(newWidget));

    // If moving a group out (inner group becoming top-level or moving to outer group),
    // also clone its children.
    if (currentWidget->isGroup())
    {
        if (currentItem->isExpanded())
            QTreeWidget::expandItem(newItem);
        newWidget->setExpanded(currentItem->isExpanded());

        if (getCompactView())
            dynamic_cast<QWidget*>(newWidget)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
        else
            dynamic_cast<QWidget*>(newWidget)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);

        for (int i = 0; i < currentItem->childCount(); i++)
        {
            QTreeWidgetItem* childItem = new QTreeWidgetItem();
            newItem->addChild(childItem);

            AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem->child(i), 0))->clone();
            childWidget->setInGroup(true);
            childWidget->setActive(false);
            QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));

            if (getCompactView())
                dynamic_cast<QWidget*>(childWidget)->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
            else
                dynamic_cast<QWidget*>(childWidget)->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
        }

        updateGroupWidget(newItem);
    }

    QTreeWidget::setCurrentItem(newItem);

    EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(currentItem));
    EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(currentItem));
    delete currentItem;

    if (parentItem->childCount() == 0)
    {
        QTreeWidget::setCurrentItem(parentItemAbove);

        EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(parentItem));
        EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(parentItem));
        delete parentItem;
    }
    else
    {
        updateGroupWidget(parentItem);
    }

    // Update the grandparent (outer group) if applicable.
    if (grandparent != nullptr)
        updateGroupWidget(grandparent);

    QTreeWidget::doItemsLayout(); // Refresh
}

void RundownTreeBaseWidget::moveItemIntoGroup()
{
    UndoScope undo(this, "Move Into Group");

    if (QTreeWidget::currentItem() == NULL)
        return;

    QTreeWidgetItem* currentItem = QTreeWidget::currentItem();
    AbstractRundownWidget* currentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0));
    int currentDepth = getItemDepth(currentItem);

    // Can't move items that are already at max depth.
    if (currentDepth >= 2)
        return;

    bool isCurrentGroup = currentWidget->isGroup();

    if (isCurrentGroup)
    {
        // Groups at depth 1 can't move into another group (would create depth 2 group).
        if (currentDepth >= 1)
            return;

        // Check if the group contains inner groups (would exceed depth on nesting).
        for (int i = 0; i < currentItem->childCount(); i++)
        {
            QWidget* cw = QTreeWidget::itemWidget(currentItem->child(i), 0);
            AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
            if (crw && crw->isGroup())
                return;
        }
    }

    // Find the item directly above in the same container.
    QTreeWidgetItem* container = currentItem->parent() ? currentItem->parent() : QTreeWidget::invisibleRootItem();
    int row = container->indexOfChild(currentItem);
    if (row <= 0)
        return;

    QTreeWidgetItem* targetGroup = container->child(row - 1);
    if (targetGroup == NULL)
        return;

    AbstractRundownWidget* targetWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(targetGroup, 0));
    if (targetWidget == NULL || !targetWidget->isGroup())
        return;

    // Clone the widget and add to target group.
    QTreeWidgetItem* newItem = new QTreeWidgetItem();
    AbstractRundownWidget* cloned = currentWidget->clone();
    cloned->setInGroup(true);

    targetGroup->addChild(newItem);
    QTreeWidget::setItemWidget(newItem, 0, dynamic_cast<QWidget*>(cloned));

    // If moving a group, also clone its children as an inner group.
    if (isCurrentGroup)
    {
        if (currentItem->isExpanded())
            QTreeWidget::expandItem(newItem);
        cloned->setExpanded(currentItem->isExpanded());

        if (getCompactView())
            dynamic_cast<QWidget*>(cloned)->setMinimumHeight(Rundown::COMPACT_ITEM_HEIGHT);
        else
            dynamic_cast<QWidget*>(cloned)->setMinimumHeight(Rundown::DEFAULT_ITEM_HEIGHT);

        for (int i = 0; i < currentItem->childCount(); i++)
        {
            QTreeWidgetItem* childItem = new QTreeWidgetItem();
            newItem->addChild(childItem);

            AbstractRundownWidget* childWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem->child(i), 0))->clone();
            childWidget->setInGroup(true);
            childWidget->setActive(false);
            QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));

            if (getCompactView())
                dynamic_cast<QWidget*>(childWidget)->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
            else
                dynamic_cast<QWidget*>(childWidget)->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
        }

        updateGroupWidget(newItem);
    }

    QTreeWidget::setCurrentItem(newItem);

    // Remove old item.
    container->takeChild(row);
    EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(currentItem));
    EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(currentItem));
    delete currentItem;

    updateGroupWidget(targetGroup);

    // Update outer group if applicable.
    if (targetGroup->parent() != nullptr)
        updateGroupWidget(targetGroup->parent());

    QTreeWidget::doItemsLayout(); // Refresh
}

void RundownTreeBaseWidget::setExpanded(bool expanded)
{
    if (QTreeWidget::currentItem() == nullptr)
        return;

    QWidget* selectedWidget = QTreeWidget::itemWidget(QTreeWidget::currentItem(), 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(selectedWidget);

    if (rundownWidget->isGroup()) // Group.
        rundownWidget->setExpanded(expanded);
}

void RundownTreeBaseWidget::updateGroupWidget(QTreeWidgetItem* item)
{
    if (item == nullptr)
        return;

    QWidget* widget = QTreeWidget::itemWidget(item, 0);
    if (widget == nullptr)
        return;

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr || !rundownWidget->isGroup())
        return;

    RundownGroupWidget* groupWidget = dynamic_cast<RundownGroupWidget*>(widget);
    if (groupWidget != nullptr)
    {
        groupWidget->updateGroupInfo(item);
        // Use minimumHeight() instead of height() because setFixedHeight() updates
        // the min/max but the actual geometry isn't updated until the next layout pass.
        item->setSizeHint(0, QSize(widget->width(), widget->minimumHeight()));

        // Schedule a layout refresh to update item positioning.
        QTreeWidget::scheduleDelayedItemsLayout();
    }
}

void RundownTreeBaseWidget::updateAllGroupWidgets()
{
    for (int i = 0; i < QTreeWidget::invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* item = QTreeWidget::invisibleRootItem()->child(i);
        updateGroupWidget(item);

        // Also update inner groups.
        for (int j = 0; j < item->childCount(); j++)
        {
            QTreeWidgetItem* child = item->child(j);
            QWidget* childWidget = QTreeWidget::itemWidget(child, 0);
            if (childWidget != nullptr)
            {
                AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(childWidget);
                if (rw != nullptr && rw->isGroup())
                    updateGroupWidget(child);
            }
        }
    }

    QTreeWidget::doItemsLayout();
}

void RundownTreeBaseWidget::channelChanged(const ChannelChangedEvent& event)
{
    Q_UNUSED(event);

    updateAllGroupWidgets();
}

void RundownTreeBaseWidget::undoLimitChanged(int limit)
{
    this->m_undoStack->setUndoLimit(limit);
}

void RundownTreeBaseWidget::keyPressEvent(QKeyEvent* event)
{
    if (this->lock)
    {
        if (event->key() == Qt::Key_Insert)
            applyRepositoryChanges();
        else if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier)
            copySelectedItems();
        else
        {
            if (event->key() == Qt::Key_Left)
                setExpanded(false);
            else if (event->key() == Qt::Key_Right)
                setExpanded(true);

            QTreeWidget::keyPressEvent(event);
        }
    }
    else
    {
        if (event->key() == Qt::Key_Delete)
            removeSelectedItems();
        else if (event->key() == Qt::Key_D && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
            duplicateSelectedItems();
        else if (event->key() == Qt::Key_D && event->modifiers() == Qt::ControlModifier)
            toggleDisableSelectedItems();
        else if (event->key() == Qt::Key_C && event->modifiers() == Qt::ControlModifier)
            copySelectedItems();
        else if (event->key() == Qt::Key_V && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
            pasteAsLinkedClones();
        else if (event->key() == Qt::Key_V && event->modifiers() == Qt::ControlModifier)
        {
            pasteSelectedItems(false, s_isCutOperation);
            s_isCutOperation = false;
        }
        else if (event->key() == Qt::Key_G && event->modifiers() == Qt::ControlModifier)
            groupItems();
        else if (event->key() == Qt::Key_U && event->modifiers() == Qt::ControlModifier)
            ungroupItems();
        else if (event->key() == Qt::Key_X && event->modifiers() == Qt::ControlModifier)
        {
            copySelectedItems();
            s_isCutOperation = true;
            removeSelectedItems();
        }
        else if (event->key() == Qt::Key_Up && (event->modifiers() == Qt::ControlModifier || (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::KeypadModifier)))
            moveItemUp();
        else if (event->key() == Qt::Key_Down && (event->modifiers() == Qt::ControlModifier || (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::KeypadModifier)))
            moveItemDown();
        else if (event->key() == Qt::Key_Left && (event->modifiers() == Qt::ControlModifier || (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::KeypadModifier)))
            moveItemOutOfGroup();
        else if (event->key() == Qt::Key_Right && (event->modifiers() == Qt::ControlModifier || (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::KeypadModifier)))
            moveItemIntoGroup();
        else
        {
            if (event->key() == Qt::Key_Left)
                setExpanded(false);
            else if (event->key() == Qt::Key_Right)
                setExpanded(true);

            QTreeWidget::keyPressEvent(event);
        }
    }

}

void RundownTreeBaseWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        dragStartPosition = event->pos();

    QTreeWidget::mousePressEvent(event);
}

void RundownTreeBaseWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (this->lock)
        return;

    if (!(event->buttons() & Qt::LeftButton))
             return;

    if ((event->pos() - dragStartPosition).manhattanLength() < qApp->startDragDistance())
         return;

    if (!copySelectedItems())
        return;

    QMimeData* mimeData = new QMimeData();
    mimeData->setData("application/rundown-item", qApp->clipboard()->text().toUtf8());

    // Ctrl+drag signals a linked clone operation.
    if (event->modifiers() & Qt::ControlModifier)
        mimeData->setData("application/rundown-clone", QByteArray("1"));

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);

    dragSourceWidget = this;
    drag->exec(Qt::CopyAction);
    dragSourceWidget = nullptr;
}

Qt::DropActions RundownTreeBaseWidget::supportedDropActions() const
{
    return Qt::CopyAction;
}

QStringList RundownTreeBaseWidget::mimeTypes() const
{
    QStringList list;
    list.append("application/library-item");
    list.append("application/rundown-item");

    return list;
}

void RundownTreeBaseWidget::dragEnterEvent(QDragEnterEvent* event)
{
    event->acceptProposedAction();
}

void RundownTreeBaseWidget::dragMoveEvent(QDragMoveEvent* event)
{
    QTreeWidget::dragMoveEvent(event);

    QModelIndex idx = indexAt(event->position().toPoint());
    if (idx.isValid())
    {
        QRect itemRect = visualRect(idx);
        // Draw the indicator line at the bottom edge of the item under cursor.
        m_dropIndicatorRect = QRect(0, itemRect.bottom(), viewport()->width(), 2);
        m_showDropIndicator = true;
    }
    else
    {
        m_showDropIndicator = false;
    }

    viewport()->update();
}

void RundownTreeBaseWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_showDropIndicator = false;
    viewport()->update();
    QTreeWidget::dragLeaveEvent(event);
}

void RundownTreeBaseWidget::paintEvent(QPaintEvent* event)
{
    QTreeWidget::paintEvent(event);

    if (m_showDropIndicator)
    {
        QPainter painter(viewport());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(45, 140, 255));
        painter.drawRect(m_dropIndicatorRect);
    }
}

bool RundownTreeBaseWidget::dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* mimeData, Qt::DropAction action)
{
    m_showDropIndicator = false;
    viewport()->update();

    UndoScope undo(this, "Drag & Drop");

    Q_UNUSED(index);
    Q_UNUSED(action);

    if (!mimeData->hasFormat("application/library-item") && !mimeData->hasFormat("application/rundown-item"))
        return false;

    if (mimeData->hasFormat("application/library-item"))
    {
        QString dndData = QString::fromUtf8(mimeData->data("application/library-item"));
        if (dndData.startsWith("<treeWidgetVideo>") ||
            dndData.startsWith("<treeWidgetTool>") ||
            dndData.startsWith("<treeWidgetTemplate>") ||
            dndData.startsWith("<treeWidgetImage>") ||
            dndData.startsWith("<treeWidgetAudio>")) // External drop from the library.
        {
            QTreeWidget::setCurrentItem(parent);

            setUpdatesEnabled(false);

            QStringList dndDataSplit = dndData.split(";");
            foreach(QString data, dndDataSplit)
            {
                QStringList dataSplit = data.split(",,");
                // Emit directly so the owning RundownTreeWidget handles it
                // regardless of which pane is focused.
                emit libraryItemDropped(LibraryModel(dataSplit.at(2).toInt(), dataSplit.at(3), dataSplit.at(1),
                                                     dataSplit.at(4), dataSplit.at(5), dataSplit.at(6).toInt(),
                                                     dataSplit.at(7)));
            }

            doItemsLayout();
            setUpdatesEnabled(true);
        }
        else if (dndData.startsWith("<treeWidgetPreset>")) // External drop from the preset library.
        {
            QTreeWidget::setCurrentItem(parent);

            QStringList dataSplit = dndData.split(",,");
            emit presetItemDropped(dataSplit.at(3));
        }
    }
    else if (mimeData->hasFormat("application/rundown-item"))
    {
        QString dndData = QString::fromUtf8(mimeData->data("application/rundown-item"));
        if (dndData.contains("<items>")) // Internal drop
        {
            if (mimeData->hasFormat("application/rundown-clone"))
            {
                // Ctrl+drag: create linked clones at drop position, keep originals.
                RundownTreeBaseWidget* sourceTree = (dragSourceWidget != nullptr) ? dragSourceWidget : this;
                QList<QTreeWidgetItem*> sourceItems = sourceTree->selectedItems();

                QTreeWidget::setCurrentItem(parent);
                int row = QTreeWidget::currentIndex().row();

                bool inGroup = (parent != nullptr && parent->parent() != nullptr);
                QTreeWidgetItem* groupParent = inGroup ? parent->parent() : nullptr;

                int offset = 1;
                for (QTreeWidgetItem* srcItem : sourceItems)
                {
                    AbstractRundownWidget* srcWidget = dynamic_cast<AbstractRundownWidget*>(sourceTree->itemWidget(srcItem, 0));
                    if (!srcWidget || srcWidget->isGroup())
                        continue;

                    AbstractCommand* srcCmd = srcWidget->getCommand();
                    if (srcCmd->getCloneGroupId().isEmpty())
                        srcCmd->setCloneGroupId(QUuid::createUuid().toString(QUuid::WithoutBraces));

                    AbstractRundownWidget* cloneWidget = srcWidget->clone();
                    cloneWidget->getCommand()->setCloneGroupId(srcCmd->getCloneGroupId());
                    cloneWidget->setInGroup(inGroup);
                    cloneWidget->setCompactView(this->compactView);

                    QTreeWidgetItem* cloneItem = new QTreeWidgetItem();
                    if (groupParent)
                        groupParent->insertChild(row + offset, cloneItem);
                    else if (row >= 0)
                        QTreeWidget::invisibleRootItem()->insertChild(row + offset, cloneItem);
                    else
                        QTreeWidget::invisibleRootItem()->addChild(cloneItem);

                    QWidget* cloneQWidget = dynamic_cast<QWidget*>(cloneWidget);
                    int widgetHeight = cloneQWidget->minimumHeight();
                    if (widgetHeight <= 0)
                        widgetHeight = this->compactView ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;

                    cloneQWidget->setFixedHeight(widgetHeight);
                    cloneItem->setSizeHint(0, QSize(cloneQWidget->width(), widgetHeight));

                    QTreeWidget::setItemWidget(cloneItem, 0, cloneQWidget);
                    offset++;
                }

                // Don't delete source items — clone keeps originals.
            }
            else if (dragSourceWidget != nullptr && dragSourceWidget != this)
            {
                // Cross-tree drop: paste here, delete from source.
                QTreeWidget::setCurrentItem(parent);

                if (!pasteSelectedItems())
                    return false;

                selectItemBelow();

                // Delete the dragged items from the source tree.
                QList<QTreeWidgetItem*> sourceItems = dragSourceWidget->selectedItems();
                for (int i = sourceItems.count() - 1; i >= 0; i--)
                {
                    QTreeWidgetItem* item = sourceItems.at(i);
                    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(dragSourceWidget->itemWidget(item, 0));
                    if (widget->isGroup())
                    {
                        for (int j = item->childCount() - 1; j >= 0; j--)
                        {
                            QWidget* childWidget = dragSourceWidget->itemWidget(item->child(j), 0);

                            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item->child(j)));
                            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item->child(j)));

                            delete childWidget;
                            delete item->child(j);
                        }
                    }

                    EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item));
                    EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item));

                    delete widget;
                    delete item;
                }

                dragSourceWidget->updateAllGroupWidgets();
                dragSourceWidget->checkEmptyRundown();
            }
            else
            {
                // Same-tree drop: existing behavior.
                QList<QTreeWidgetItem*> items = QTreeWidget::selectedItems();

                QTreeWidget::setCurrentItem(parent);

                if (!pasteSelectedItems())
                    return false;

                selectItemBelow();

                for (int i = items.count() - 1; i >= 0; i--)
                {
                    QTreeWidgetItem* item = items.at(i);
                    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
                    if (widget->isGroup())
                    {
                        for (int j = item->childCount() - 1; j >= 0; j--)
                        {
                            QWidget* childWidget = QTreeWidget::itemWidget(item->child(j), 0);

                            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item->child(j)));
                            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item->child(j)));

                            delete childWidget;
                            delete item->child(j);
                        }
                    }

                    EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item));
                    EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item));

                    delete widget;
                    delete item;
                }
            }
        }
    }

    updateAllGroupWidgets();

    emit itemsChanged();

    return true;
}

bool RundownTreeBaseWidget::hasItemBelow() const
{
    if (QTreeWidget::currentItem() == NULL)
        return false;

    QTreeWidgetItem* itemBelow = NULL;
    if (dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(QTreeWidget::currentItem(), 0))->isGroup()) // Group.
        itemBelow = QTreeWidget::invisibleRootItem()->child(QTreeWidget::currentIndex().row() + 1);
    else
        itemBelow = QTreeWidget::itemBelow(QTreeWidget::currentItem());

    return (itemBelow == NULL) ? false : true;
}

void RundownTreeBaseWidget::selectItemAbove()
{
    if (QTreeWidget::currentItem() == NULL)
    {
        QTreeWidget::setCurrentItem(QTreeWidget::invisibleRootItem()->child(0));
        return;
    }

    QTreeWidgetItem* itemAbove = NULL;
    if (dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(QTreeWidget::currentItem(), 0))->isGroup()) // Group.
        itemAbove = QTreeWidget::invisibleRootItem()->child(QTreeWidget::currentIndex().row() - 1);
    else
        itemAbove = QTreeWidget::itemAbove(QTreeWidget::currentItem());

    if (itemAbove != NULL)
    {
        QTreeWidgetItem* previousItem = QTreeWidget::currentItem();

        QTreeWidget::setCurrentItem(itemAbove);

        EventManager::getInstance().fireCurrentItemChangedEvent(CurrentItemChangedEvent(itemAbove, previousItem));
    }
}

void RundownTreeBaseWidget::selectItemBelow()
{
    if (QTreeWidget::currentItem() == NULL)
    {
        QTreeWidget::setCurrentItem(QTreeWidget::invisibleRootItem()->child(QTreeWidget::invisibleRootItem()->childCount() - 1));
        return;
    }

    QTreeWidgetItem* itemBelow = NULL;
    if (dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(QTreeWidget::currentItem(), 0))->isGroup()) // Group.
        itemBelow = QTreeWidget::invisibleRootItem()->child(QTreeWidget::currentIndex().row() + 1);
    else
        itemBelow = QTreeWidget::itemBelow(QTreeWidget::currentItem());

    if (itemBelow != NULL)
    {
        QTreeWidgetItem* previousItem = QTreeWidget::currentItem();

        QTreeWidget::setCurrentItem(itemBelow);

        EventManager::getInstance().fireCurrentItemChangedEvent(CurrentItemChangedEvent(itemBelow, previousItem));
    }
}

void RundownTreeBaseWidget::repositoryRundown(const RepositoryRundownEvent& event)
{
    this->lock = event.getRepositoryRundown();
}

void RundownTreeBaseWidget::applyRepositoryChanges()
{
    qDebug("Apply repository changes");

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Updating rundown..."));

    // Get the current selected item story id.
    QString currentStoryId = currentItemStoryId();

    int index = 0;
    while (index < this->repositoryChanges.count())
    {
        const RepositoryChangeModel& model = this->repositoryChanges.at(index);

        // Skip update if ADD or REMOVE contians the current selected item story id.
        if ((model.getType() == "REMOVE" && model.getStoryId() == currentStoryId) || (model.getType() == "ADD" && containsStoryId(currentStoryId, model.getData())))
        {
            index++;
            continue;
        }

        if (model.getType() == "ADD")
            addRepositoryItem(model.getStoryId(), model.getData());
        else
            removeRepositoryItem(model.getStoryId());

        this->repositoryChanges.removeAt(index);
    }

    // Do we have updates which we can nott apply?
    if (this->repositoryChanges.count() > 0)
        checRepositoryChanges();

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(""));
}

QString RundownTreeBaseWidget::currentItemStoryId()
{
    QString currentStoryId;
    if (QTreeWidget::currentItem() != NULL)
    {
        QTreeWidgetItem* currentItem = QTreeWidget::currentItem();
        AbstractRundownWidget* currentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem, 0));
        AbstractRundownWidget* parentWidget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(currentItem->parent(), 0));

        if (parentWidget != NULL)
            currentStoryId = parentWidget->getCommand()->getStoryId(); // Group item.
        else
            currentStoryId = currentWidget->getCommand()->getStoryId(); // Group or top level item.
    }

    return currentStoryId;
}

bool RundownTreeBaseWidget::containsStoryId(const QString& storyId, const QString& data)
{
    if (!data.isEmpty())
    {
        std::wstringstream wstringstream;
        wstringstream << data.toStdWString();

        boost::property_tree::wptree pt;
        boost::property_tree::xml_parser::read_xml(wstringstream, pt);

        for (const boost::property_tree::wptree::value_type &parentValue : pt.get_child(L"items"))
        {
            if (parentValue.second.count(L"storyid") > 0)
            {
                QString storyid = QString::fromStdWString(parentValue.second.get(L"storyid", L""));
                if (storyid == storyId)
                    return true;
            }
        }
    }

    return false;
}

void RundownTreeBaseWidget::addRepositoryChange(const RepositoryChangeModel& model)
{
    this->repositoryChanges.append(model);
}

void RundownTreeBaseWidget::checRepositoryChanges()
{
    if (this->theme == Appearance::CURVE_THEME)
        QTreeWidget::setStyleSheet((this->repositoryChanges.count() > 0) ? "#treeWidgetRundown { border-width: 1; border-color: darkorange; }" : "#treeWidgetRundown { border-width: 1; }");
    else
        QTreeWidget::setStyleSheet((this->repositoryChanges.count() > 0) ? "#treeWidgetRundown { border-width: 1; border-color: darkorange; }" : "#treeWidgetRundown { border-width: 0; border-top-width: 1; }");
}

void RundownTreeBaseWidget::addRepositoryItem(const QString& storyId, const QString& data)
{
    int row = -1;

    for (int i = QTreeWidget::topLevelItemCount() - 1; i >= 0; i--)
    {
        QTreeWidgetItem* item = QTreeWidget::topLevelItem(i);
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget->getCommand()->getStoryId() == storyId)
        {
            row = QTreeWidget::indexFromItem(item).row();
            break; // We have found the last story id in the rundown.
        }
    }

    int offset = 1;
    std::wstringstream wstringstream;
    wstringstream << data.toStdWString();

    boost::property_tree::wptree pt;
    boost::property_tree::xml_parser::read_xml(wstringstream, pt);

    setUpdatesEnabled(false);

    for (boost::property_tree::wptree::value_type &parentValue : pt.get_child(L"items"))
    {
        if (parentValue.first != L"item")
            continue;

        AbstractRundownWidget* parentWidget = readProperties(parentValue.second);
        parentWidget->setInGroup(false);
        parentWidget->setExpanded(false);

        QTreeWidgetItem* parentItem = new QTreeWidgetItem();

        QTreeWidget::invisibleRootItem()->insertChild(row + offset++, parentItem);
        QTreeWidget::setItemWidget(parentItem, 0, dynamic_cast<QWidget*>(parentWidget));

        if (parentWidget->isGroup())
        {
            for (boost::property_tree::wptree::value_type &childValue : parentValue.second.get_child(L"items"))
            {
                AbstractRundownWidget* childWidget = readProperties(childValue.second);
                childWidget->setInGroup(true);

                QTreeWidgetItem* childItem = new QTreeWidgetItem();
                parentItem->addChild(childItem);

                QTreeWidget::setItemWidget(childItem, 0, dynamic_cast<QWidget*>(childWidget));
            }
        }

        updateGroupWidget(parentItem);
    }

    QTreeWidget::doItemsLayout();
    setUpdatesEnabled(true);
}

void RundownTreeBaseWidget::removeRepositoryItem(const QString& storyId)
{
    for (int i = QTreeWidget::topLevelItemCount() - 1; i >= 0; i--)
    {
        QTreeWidgetItem* item = QTreeWidget::topLevelItem(i);
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(QTreeWidget::itemWidget(item, 0));
        if (widget->getCommand()->getStoryId() == storyId)
        {
            if (widget->isGroup())
            {
                for (int i = item->childCount() - 1; i >= 0; i--)
                {
                    QWidget* childWidget = QTreeWidget::itemWidget(item->child(i), 0);

                    // Remove our items from the AutoPlay queue if it exists.
                    EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item->child(i)));

                    // Clear current playing item.
                    EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item->child(i)));

                    delete childWidget;
                    delete item->child(i);
                }
            }

            // Remove our items from the auto play queue if it exists.
            EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(item));

            // Clear current playing item.
            EventManager::getInstance().fireClearCurrentPlayingItemEvent(ClearCurrentPlayingItemEvent(item));

            delete widget;
            delete item;
        }
    }
}
