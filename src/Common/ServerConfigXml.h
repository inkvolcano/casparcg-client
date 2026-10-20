#pragma once

// casparcg.config as a tree the config editor can change and write back without
// losing anything it does not understand.
//
// The server reads the file with boost's property tree and ignores unknown
// elements, so a config often carries things no editor knows about: settings of
// a newer server, notes, whole commented-out channels. All of it is kept -
// elements, attributes and comments, in their order, including the comments
// before and after <configuration> (the shipped config keeps its reference of
// every option in one). Only whitespace is not: the file is written back with
// four-space indents.
//
// Built on QXmlStreamReader/Writer, so it needs nothing beyond QtCore and is
// tested by tools/test-serverconfig.

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QXmlStreamReader>

#include <memory>

namespace ServerConfigXml
{
    struct Node;
    using NodePtr = std::shared_ptr<Node>;

    struct Node
    {
        bool comment = false;
        QString name;                                  // element name; empty for a comment
        QList<QPair<QString, QString>> attributes;     // in file order
        QString text;                                  // a leaf's text, or the comment's
        QList<NodePtr> children;                       // elements and comments, in file order
        Node* parent = nullptr;
    };

    struct Document
    {
        bool ok = false;
        QString error;
        int errorLine = 0;

        QList<NodePtr> before;      // comments before the root element
        NodePtr root;
        QList<NodePtr> after;       // comments after it
    };

    inline NodePtr makeElement(const QString& name, const QString& text = QString())
    {
        NodePtr node = std::make_shared<Node>();
        node->name = name;
        node->text = text;
        return node;
    }

    inline void appendChild(const NodePtr& parent, const NodePtr& child)
    {
        child->parent = parent.get();
        parent->children.append(child);
    }

    // Elements only, optionally of one name, in order.
    inline QList<NodePtr> elements(const NodePtr& node, const QString& name = QString())
    {
        QList<NodePtr> found;
        if (node == nullptr)
            return found;
        for (const NodePtr& child : node->children)
        {
            if (!child->comment && (name.isEmpty() || child->name == name))
                found.append(child);
        }
        return found;
    }

    // The first element of that name, as the server's get() finds it.
    inline NodePtr child(const NodePtr& node, const QString& name)
    {
        if (node == nullptr)
            return nullptr;
        for (const NodePtr& c : node->children)
        {
            if (!c->comment && c->name == name)
                return c;
        }
        return nullptr;
    }

    // "a/b/c" from `node`; null when any step is missing. Empty path is the node.
    inline NodePtr find(const NodePtr& node, const QString& path)
    {
        NodePtr current = node;
        for (const QString& step : path.split('/', Qt::SkipEmptyParts))
        {
            current = child(current, step);
            if (current == nullptr)
                return nullptr;
        }
        return current;
    }

    inline NodePtr ensure(const NodePtr& node, const QString& path)
    {
        NodePtr current = node;
        for (const QString& step : path.split('/', Qt::SkipEmptyParts))
        {
            NodePtr next = child(current, step);
            if (next == nullptr)
            {
                next = makeElement(step);
                appendChild(current, next);
            }
            current = next;
        }
        return current;
    }

    inline int attributeIndex(const Node& node, const QString& name)
    {
        for (int i = 0; i < node.attributes.count(); i++)
        {
            if (node.attributes.at(i).first == name)
                return i;
        }
        return -1;
    }

    inline bool hasAttribute(const NodePtr& node, const QString& name)
    {
        return node != nullptr && attributeIndex(*node, name) >= 0;
    }

    inline QString attribute(const NodePtr& node, const QString& name)
    {
        if (node == nullptr)
            return QString();
        const int i = attributeIndex(*node, name);
        return i < 0 ? QString() : node->attributes.at(i).second;
    }

    inline void setAttribute(const NodePtr& node, const QString& name, const QString& value)
    {
        const int i = attributeIndex(*node, name);
        if (i < 0)
            node->attributes.append(qMakePair(name, value));
        else
            node->attributes[i].second = value;
    }

    inline void removeAttribute(const NodePtr& node, const QString& name)
    {
        const int i = attributeIndex(*node, name);
        if (i >= 0)
            node->attributes.removeAt(i);
    }

    inline bool isEmptyElement(const NodePtr& node)
    {
        return node->children.isEmpty() && node->attributes.isEmpty() && node->text.trimmed().isEmpty();
    }

    inline void remove(const NodePtr& node)
    {
        Node* parent = node->parent;
        if (parent == nullptr)
            return;
        for (int i = 0; i < parent->children.count(); i++)
        {
            if (parent->children.at(i).get() == node.get())
            {
                parent->children.removeAt(i);
                break;
            }
        }
        node->parent = nullptr;
    }

    // Moves an element among the elements of its parent, by `delta` places,
    // carrying it past comments. Returns whether it moved.
    inline bool move(const NodePtr& node, int delta)
    {
        Node* parent = node->parent;
        if (parent == nullptr || delta == 0)
            return false;

        QList<int> elementSlots;
        int at = -1;
        for (int i = 0; i < parent->children.count(); i++)
        {
            if (parent->children.at(i)->comment)
                continue;
            if (parent->children.at(i).get() == node.get())
                at = elementSlots.count();
            elementSlots.append(i);
        }

        const int target = at + delta;
        if (at < 0 || target < 0 || target >= elementSlots.count())
            return false;

        const int from = elementSlots.at(at);
        const int to = elementSlots.at(target);
        parent->children.move(from, to);
        return true;
    }

    // A value by path from an owner element: "a/b" is the text of element b, "a/@x"
    // attribute x of element a, "@x" an attribute of the owner, "." its own text.
    // `present` tells "not there" from "there and empty".
    inline QString value(const NodePtr& owner, const QString& path, bool* present = nullptr)
    {
        if (present != nullptr)
            *present = false;
        if (owner == nullptr)
            return QString();

        if (path == ".")
        {
            if (present != nullptr)
                *present = true;
            return owner->text.trimmed();
        }

        const int at = path.lastIndexOf("@");
        if (at >= 0)
        {
            const NodePtr element = find(owner, path.left(at));
            const QString name = path.mid(at + 1);
            if (element == nullptr || !hasAttribute(element, name))
                return QString();
            if (present != nullptr)
                *present = true;
            return attribute(element, name);
        }

        const NodePtr element = find(owner, path);
        if (element == nullptr)
            return QString();
        if (present != nullptr)
            *present = true;
        return element->text.trimmed();
    }

    // Sets a value by the same paths. An empty value removes the element or
    // attribute - the server reads an empty element as an empty string rather
    // than its default, which for some settings stops it - and then removes the
    // groups it leaves empty (<subregion/>, <hdr-metadata/>), up to the owner.
    // `keepGroups` leaves those groups alone: an empty <vanc/> still turns VANC on.
    inline void setValue(const NodePtr& owner, const QString& path, const QString& text, bool keepGroups = false)
    {
        const QString value = text.trimmed();

        if (path == ".")
        {
            owner->text = value;
            return;
        }

        const int at = path.lastIndexOf("@");
        NodePtr leaf;
        if (at >= 0)
        {
            const QString elementPath = path.left(at);
            const QString name = path.mid(at + 1);
            if (value.isEmpty())
            {
                leaf = find(owner, elementPath);
                if (leaf == nullptr)
                    return;
                removeAttribute(leaf, name);
            }
            else
            {
                setAttribute(ensure(owner, elementPath), name, value);
                return;
            }
        }
        else if (!value.isEmpty())
        {
            NodePtr element = ensure(owner, path);
            element->text = value;
            return;
        }
        else
        {
            leaf = find(owner, path);
            if (leaf == nullptr)
                return;
            if (leaf->parent != nullptr && leaf.get() != owner.get())
            {
                leaf->text.clear();
                if (leaf->children.isEmpty() && leaf->attributes.isEmpty())
                {
                    Node* parent = leaf->parent;
                    remove(leaf);
                    // Walk up through groups left empty.
                    while (!keepGroups && parent != nullptr && parent != owner.get() && parent->children.isEmpty()
                           && parent->attributes.isEmpty() && parent->text.trimmed().isEmpty() && parent->parent != nullptr)
                    {
                        Node* up = parent->parent;
                        for (int i = 0; i < up->children.count(); i++)
                        {
                            if (up->children.at(i).get() == parent)
                            {
                                up->children.removeAt(i);
                                break;
                            }
                        }
                        parent = up;
                    }
                }
            }
            return;
        }

        // An attribute was removed: drop its element if that left it empty and it
        // is not the owner.
        if (!keepGroups && leaf != nullptr && leaf.get() != owner.get() && isEmptyElement(leaf))
            remove(leaf);
    }

    inline Document parse(const QString& xml)
    {
        Document document;
        QXmlStreamReader reader(xml);
        QList<NodePtr> stack;

        while (!reader.atEnd())
        {
            reader.readNext();
            switch (reader.tokenType())
            {
                case QXmlStreamReader::StartElement:
                {
                    NodePtr node = makeElement(reader.name().toString());
                    for (const QXmlStreamAttribute& attribute : reader.attributes())
                        node->attributes.append(qMakePair(attribute.qualifiedName().toString(), attribute.value().toString()));

                    if (stack.isEmpty())
                    {
                        if (document.root != nullptr)
                        {
                            document.error = "More than one root element.";
                            document.errorLine = int(reader.lineNumber());
                            return document;
                        }
                        document.root = node;
                    }
                    else
                    {
                        appendChild(stack.last(), node);
                    }
                    stack.append(node);
                    break;
                }
                case QXmlStreamReader::EndElement:
                    if (!stack.isEmpty())
                    {
                        stack.last()->text = stack.last()->text.trimmed();
                        stack.removeLast();
                    }
                    break;
                case QXmlStreamReader::Characters:
                    if (!stack.isEmpty() && !reader.isWhitespace())
                        stack.last()->text += reader.text().toString();
                    break;
                case QXmlStreamReader::Comment:
                {
                    NodePtr node = std::make_shared<Node>();
                    node->comment = true;
                    node->text = reader.text().toString();
                    if (!stack.isEmpty())
                        appendChild(stack.last(), node);
                    else if (document.root == nullptr)
                        document.before.append(node);
                    else
                        document.after.append(node);
                    break;
                }
                default:
                    break;
            }
        }

        if (reader.hasError())
        {
            document.error = reader.errorString();
            document.errorLine = int(reader.lineNumber());
            return document;
        }
        if (document.root == nullptr)
        {
            document.error = "The file holds no XML element.";
            return document;
        }

        document.ok = true;
        return document;
    }

    inline QString escapeText(QString text)
    {
        text.replace('&', "&amp;");
        text.replace('<', "&lt;");
        text.replace('>', "&gt;");
        return text;
    }

    inline QString escapeAttribute(QString text)
    {
        text = escapeText(text);
        text.replace('"', "&quot;");
        return text;
    }

    inline void writeNode(QString& out, const NodePtr& node, int depth)
    {
        const QString indent(depth * 4, ' ');

        if (node->comment)
        {
            out += indent + "<!--" + node->text + "-->\n";
            return;
        }

        QString open = "<" + node->name;
        for (const QPair<QString, QString>& attribute : node->attributes)
            open += QString(" %1=\"%2\"").arg(attribute.first, escapeAttribute(attribute.second));

        if (node->children.isEmpty())
        {
            if (node->text.isEmpty())
                out += indent + open + " />\n";
            else
                out += indent + open + ">" + escapeText(node->text) + "</" + node->name + ">\n";
            return;
        }

        out += indent + open + ">\n";
        if (!node->text.isEmpty())
            out += indent + "    " + escapeText(node->text) + "\n";
        for (const NodePtr& child : node->children)
            writeNode(out, child, depth + 1);
        out += indent + "</" + node->name + ">\n";
    }

    inline QString write(const Document& document)
    {
        QString out = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        for (const NodePtr& node : document.before)
            writeNode(out, node, 0);
        if (document.root != nullptr)
            writeNode(out, document.root, 0);
        for (const NodePtr& node : document.after)
        {
            out += "\n";
            writeNode(out, node, 0);
        }
        return out;
    }
}
