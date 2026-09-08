#include "MissingMediaScanner.h"

#include "AbstractRundownWidget.h"
#include "RundownWidgetHelper.h"

#include "DatabaseManager.h"
#include "Models/DeviceModel.h"
#include "Models/LibraryModel.h"

#include "Commands/AudioCommand.h"
#include "Commands/ImageScrollerCommand.h"
#include "Commands/MovieCommand.h"
#include "Commands/StillCommand.h"
#include "Commands/TemplateCommand.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>

namespace
{
    const char* const MARKER_NAME = "labelMissingMedia";

    // The name a rundown item actually points at. It lives on the command rather
    // than the library model, because the operator can retype it in the Inspector
    // and then the two disagree — and the command is what the server would act on.
    QString mediaNameFor(AbstractCommand* command, const QString& type)
    {
        if (type == "MOVIE")
        {
            MovieCommand* movie = dynamic_cast<MovieCommand*>(command);
            return movie != nullptr ? movie->getVideoName() : QString();
        }

        if (type == "STILL")
        {
            StillCommand* still = dynamic_cast<StillCommand*>(command);
            return still != nullptr ? still->getImageName() : QString();
        }

        if (type == "AUDIO")
        {
            AudioCommand* audio = dynamic_cast<AudioCommand*>(command);
            return audio != nullptr ? audio->getAudioName() : QString();
        }

        if (type == "TEMPLATE")
        {
            TemplateCommand* templateCommand = dynamic_cast<TemplateCommand*>(command);
            return templateCommand != nullptr ? templateCommand->getTemplateName() : QString();
        }

        if (type == "IMAGESCROLLER")
        {
            ImageScrollerCommand* scroller = dynamic_cast<ImageScrollerCommand*>(command);
            return scroller != nullptr ? scroller->getImageScrollerName() : QString();
        }

        return QString();
    }
}

MissingMediaScanner::MissingMediaScanner(QObject* parent)
    : QObject(parent)
{
}

bool MissingMediaScanner::isEnabled()
{
    // On unless it has been turned off: a warning that only appears when
    // something is wrong costs nothing when nothing is.
    return DatabaseManager::getInstance().getConfigurationByName("WarnMissingMedia").getValue() != "false";
}

void MissingMediaScanner::buildLibraryIndex()
{
    this->libraryKeys.clear();
    this->devicesWithLibrary.clear();

    // Read once for the whole sweep. Media, templates and data are separate
    // tables' worth of rows in this client, and an item can point at any of them.
    QList<LibraryModel> everything;
    everything += DatabaseManager::getInstance().getLibraryMedia();
    everything += DatabaseManager::getInstance().getLibraryTemplate();
    everything += DatabaseManager::getInstance().getLibraryData();

    foreach (const LibraryModel& model, everything)
    {
        // CasparCG is not case-sensitive about media names, and neither is
        // Windows, so matching that way avoids flagging a row for the case its
        // name happens to be typed in.
        this->libraryKeys.insert(model.getName().toLower() + "|" + model.getDeviceName().toLower());
        this->devicesWithLibrary.insert(model.getDeviceName().toLower());
    }
}

MissingMediaScanner::Paths MissingMediaScanner::pathsFor(const QString& deviceName)
{
    if (this->devicePaths.contains(deviceName))
        return this->devicePaths.value(deviceName);

    DeviceModel device = DatabaseManager::getInstance().getDeviceByName(deviceName);

    Paths paths;
    paths.mediaPath = device.getMediaPath();
    paths.templatePath = device.getTemplatePath();

    // A path that is configured but points somewhere this machine cannot reach —
    // a server's local C:\ seen from another computer — must not be treated as an
    // authority, or every item would be reported missing.
    paths.mediaUsable = !paths.mediaPath.isEmpty() && QFileInfo(paths.mediaPath).isDir();
    paths.templateUsable = !paths.templatePath.isEmpty() && QFileInfo(paths.templatePath).isDir();

    this->devicePaths.insert(deviceName, paths);

    return paths;
}

bool MissingMediaScanner::existsUnder(const QString& folder, const QString& name, const QString& type) const
{
    if (folder.isEmpty() || name.isEmpty())
        return false;

    QString relative = name;
    relative.replace('\\', '/');

    // A name from a rundown file must not be able to point outside the folder it
    // is looked up in.
    if (relative.contains("..") || relative.startsWith('/') || relative.contains(':'))
        return false;

    // Names sometimes carry their extension and sometimes do not, so the exact
    // name is tried before the extensions a server would have accepted.
    if (QFileInfo::exists(QDir(folder).filePath(relative)))
        return true;

    const QStringList extensions = MediaCheck::extensionsFor(type);
    foreach (const QString& extension, extensions)
    {
        if (QFileInfo::exists(QDir(folder).filePath(relative + extension)))
            return true;
    }

    return false;
}

MissingMediaScanner::Result MissingMediaScanner::scan(QTreeWidget* tree)
{
    Result result;

    if (tree == nullptr)
        return result;

    this->devicePaths.clear();

    const bool enabled = isEnabled();
    if (enabled)
        buildLibraryIndex();
    else
        this->libraryKeys.clear();

    QTreeWidgetItem* root = tree->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
        scanItem(tree, root->child(i), result);

    return result;
}

void MissingMediaScanner::scanItem(QTreeWidget* tree, QTreeWidgetItem* item, Result& result)
{
    if (item == nullptr)
        return;

    // Groups hold the items that matter, so the walk goes all the way down
    // rather than stopping at the top level.
    for (int i = 0; i < item->childCount(); i++)
        scanItem(tree, item->child(i), result);

    QWidget* widget = tree->itemWidget(item, 0);
    if (widget == nullptr)
        return;

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr || rundownWidget->getLibraryModel() == nullptr)
        return;

    if (!isEnabled())
    {
        clearMark(widget);
        return;
    }

    const QString type = rundownWidget->getLibraryModel()->getType();

    if (!MediaCheck::typeUsesMedia(type))
    {
        clearMark(widget);
        return;
    }

    const QString name = mediaNameFor(rundownWidget->getCommand(), type);
    const QString deviceName = rundownWidget->getLibraryModel()->getDeviceName();

    MediaCheck::Evidence evidence;
    evidence.inLibrary = this->libraryKeys.contains(name.toLower() + "|" + deviceName.toLower());
    evidence.libraryUsable = this->devicesWithLibrary.contains(deviceName.toLower());

    const Paths paths = pathsFor(deviceName);
    const bool useTemplatePath = MediaCheck::typeUsesTemplatePath(type);

    evidence.pathUsable = useTemplatePath ? paths.templateUsable : paths.mediaUsable;
    evidence.onDisk = evidence.pathUsable
        && existsUnder(useTemplatePath ? paths.templatePath : paths.mediaPath, name, type);

    const MediaCheck::Verdict verdict = MediaCheck::verdictFor(type, name, evidence);

    if (verdict == MediaCheck::Verdict::NotApplicable)
    {
        clearMark(widget);
        return;
    }

    result.checked++;

    if (verdict == MediaCheck::Verdict::Missing)
    {
        result.missing++;
        mark(widget, MediaCheck::explain(type, name, evidence));
        return;
    }

    if (verdict == MediaCheck::Verdict::Unknown)
        result.unknown++;

    clearMark(widget);
}

void MissingMediaScanner::mark(QWidget* widget, const QString& tooltip)
{
    QLabel* marker = widget->findChild<QLabel*>(MARKER_NAME);

    if (marker == nullptr)
    {
        // Hung on frameItem, which every rundown item widget has, so no item
        // type needs to know about this.
        QWidget* host = widget->findChild<QFrame*>("frameItem");
        if (host == nullptr)
            return;

        marker = new QLabel(host);
        marker->setObjectName(MARKER_NAME);
        marker->setText(QString::fromUtf8("\xe2\x9a\xa0"));
        marker->setFixedSize(16, 16);
        marker->setAlignment(Qt::AlignCenter);
        marker->setStyleSheet("color: rgb(255, 190, 60); background-color: rgba(0, 0, 0, 90);"
                              "border-radius: 3px; font-size: 12px; font-weight: bold;");

        // Bottom left of the colour strip, where nothing else is drawn: the bank
        // badge takes the top and the status icons take the right.
        marker->move(RundownWidgetHelper::BADGE_WIDTH + 2, 18);
    }

    marker->setToolTip(tooltip);
    marker->show();
    marker->raise();
}

void MissingMediaScanner::clearMark(QWidget* widget)
{
    QLabel* marker = widget->findChild<QLabel*>(MARKER_NAME);

    if (marker != nullptr)
        marker->hide();
}
