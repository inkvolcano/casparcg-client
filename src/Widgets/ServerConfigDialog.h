#pragma once

#include "Shared.h"

#include "ServerConfigSchema.h"

#include <QtCore/QList>
#include <QtCore/QString>

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTabWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
QT_END_NAMESPACE

// Edit a CasparCG server's casparcg.config: every setting the server reads, with
// a list to choose from wherever the server takes only certain values.
//
// Opened from a server's menu in Server Status, on the casparcg.config beside
// that server's executable. On the left the file's parts - the settings pages,
// the AMCP ports, the channels with their outputs and producers, custom video
// modes - and on the right the settings of the one chosen. Below, what the
// server would make of the file as it stands: what would stop it from starting,
// what output it would skip, and what it would quietly ignore. And the file
// exactly as it will be written.
//
// Nothing the editor does not know is lost: unknown elements, comments, and a
// newer server's settings stay where they are. A setting left on "Default" is
// left out of the file, never written empty - the server reads an empty element
// as an empty value, which for some settings stops it. Every save writes a dated
// backup beside the file first.
//
// The settings, their values and the checks are in Common/ServerConfigSchema.h,
// the file handling in Common/ServerConfigXml.h; tools/test-serverconfig.
class WIDGETS_EXPORT ServerConfigDialog : public QDialog
{
    Q_OBJECT

    public:
        static void openFor(const QString& deviceName, QWidget* parent);

        explicit ServerConfigDialog(const QString& deviceName, QWidget* parent = nullptr);

    protected:
        void reject() override;

    private:
        // One entry of the list on the left: a settings page, a heading over a
        // list, or one element of the file.
        struct Target
        {
            QString kind;                       // a page kind, "heading:<list>", or an item kind
            ServerConfigXml::NodePtr node;      // the element its settings are read from
            int number = 0;                     // a channel's number
        };

        QString deviceName;
        QString serverExecutable;
        QString configPath;

        ServerConfigXml::Document document;
        ServerConfigSchema::Version version = ServerConfigSchema::Version::Master;
        bool dirty = false;
        bool busy = false;

        QLabel* labelFile = nullptr;
        QComboBox* comboVersion = nullptr;
        QTreeWidget* tree = nullptr;
        QToolButton* buttonAdd = nullptr;
        QPushButton* buttonRemove = nullptr;
        QPushButton* buttonUp = nullptr;
        QPushButton* buttonDown = nullptr;
        QScrollArea* formArea = nullptr;
        QTabWidget* tabs = nullptr;
        QListWidget* listProblems = nullptr;
        QPlainTextEdit* textSource = nullptr;
        QLabel* labelResult = nullptr;
        QPushButton* buttonSave = nullptr;
        QPushButton* buttonRestart = nullptr;
        QTimer* refreshTimer = nullptr;

        QList<Target> targets;

        void load(const QString& path);
        bool canRestart() const;

        void rebuildTree(const ServerConfigXml::NodePtr& select = nullptr, const QString& selectKind = QString());
        int currentTarget() const;
        QString titleOf(const Target& target) const;

        void showForm();
        QWidget* editorFor(const ServerConfigSchema::Field& field, const ServerConfigXml::NodePtr& owner, QWidget* parent);

        // The channel, DeckLink or Art-Net output the selection is in, if any.
        ServerConfigXml::NodePtr enclosing(const QString& name) const;

        void fillAddMenu(QMenu* menu);
        void addItem(const QString& kind, const ServerConfigXml::NodePtr& container);
        void removeCurrent();
        void moveCurrent(int delta);

        void changed();
        void refresh();
        void updateButtons();

        bool save();
        void saveAndRestart();

        void say(const QString& text, bool problem = false);
};
