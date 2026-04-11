#include "CloneGroupRegistry.h"

#include "Commands/AbstractCommand.h"

#include <QtCore/QBuffer>
#include <QtCore/QXmlStreamWriter>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <sstream>

Q_GLOBAL_STATIC(CloneGroupRegistry, cloneGroupRegistry)

CloneGroupRegistry::CloneGroupRegistry()
{
}

CloneGroupRegistry& CloneGroupRegistry::getInstance()
{
    return *cloneGroupRegistry();
}

void CloneGroupRegistry::registerCommand(const QString& groupId, AbstractCommand* command)
{
    if (groupId.isEmpty() || command == nullptr)
        return;

    if (groups[groupId].contains(command))
        return; // Already registered, skip duplicate destroyed connection.

    groups[groupId].insert(command);
    connect(command, &QObject::destroyed, this, [this, command]() {
        unregisterCommand(command);
    });
}

void CloneGroupRegistry::unregisterCommand(AbstractCommand* command)
{
    if (command == nullptr)
        return;

    for (auto it = groups.begin(); it != groups.end(); )
    {
        it->remove(command);
        if (it->size() == 1)
        {
            // Auto-dissolve: only one member left, unlink it.
            AbstractCommand* remaining = *it->begin();
            it = groups.erase(it); // Remove group first so re-entrant call finds nothing.
            remaining->setCloneGroupId("");
        }
        else if (it->isEmpty())
        {
            it = groups.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CloneGroupRegistry::syncFromSource(const QString& groupId, AbstractCommand* source)
{
    if (syncing || groupId.isEmpty() || !groups.contains(groupId) || source == nullptr)
        return;

    syncing = true;

    // Serialize source command properties to XML string.
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartDocument();
    writer.writeStartElement("properties");
    source->writeProperties(writer);
    writer.writeEndElement();
    writer.writeEndDocument();

    // Parse XML to boost property tree and sync to siblings.
    std::wistringstream stream(xml.toStdWString());
    boost::property_tree::wptree fullPt;
    try
    {
        boost::property_tree::read_xml(stream, fullPt);
        auto& pt = fullPt.get_child(L"properties");

        // Sync to all siblings (excluding source).
        // Block signals during readProperties to prevent cascading signal emissions.
        // Each setter in readProperties emits propertyChanged + a specific signal,
        // which would trigger configureOscSubscriptions and further sync attempts.
        for (AbstractCommand* sibling : groups[groupId])
        {
            if (sibling == source)
                continue;

            sibling->blockSignals(true);
            sibling->readProperties(pt);
            sibling->blockSignals(false);
        }
    }
    catch (const std::exception& ex)
    {
        qWarning("CloneGroupRegistry: failed to sync group '%s': %s", qPrintable(groupId), ex.what());
    }
    catch (...)
    {
        qWarning("CloneGroupRegistry: failed to sync group '%s': unknown error", qPrintable(groupId));
    }

    syncing = false;
}

bool CloneGroupRegistry::isSyncing() const
{
    return syncing;
}
