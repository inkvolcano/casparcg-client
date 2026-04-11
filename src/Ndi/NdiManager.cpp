#include "NdiManager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QGlobalStatic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

Q_GLOBAL_STATIC(NdiManager, ndiManager)

// ---------------------------------------------------------------------------
// Discovery worker — runs on a dedicated QThread.
// ---------------------------------------------------------------------------
class NdiManager::DiscoveryWorker : public QObject
{
    Q_OBJECT

public:
    explicit DiscoveryWorker(const NDIlib_v5* ndi)
        : p_NDI(ndi), stopFlag(false) {}

    void requestStop() { stopFlag = true; }

    Q_SIGNAL void sourcesDiscovered(const QList<NdiSourceInfo>& sources);

public slots:
    void run()
    {
        NDIlib_find_create_t findSettings;
        NDIlib_find_instance_t finder = p_NDI->find_create_v2(&findSettings);
        if (!finder)
        {
            qWarning("NDI: Failed to create source finder");
            return;
        }

        while (!stopFlag)
        {
            // Block up to 1 second waiting for source changes.
            if (p_NDI->find_wait_for_sources(finder, 1000))
            {
                uint32_t count = 0;
                const NDIlib_source_t* ndiSources = p_NDI->find_get_current_sources(finder, &count);

                QList<NdiSourceInfo> list;
                for (uint32_t i = 0; i < count; i++)
                {
                    list.append(NdiSourceInfo(
                        QString::fromUtf8(ndiSources[i].p_ndi_name)));
                }

                emit sourcesDiscovered(list);
            }
        }

        p_NDI->find_destroy(finder);
    }

private:
    const NDIlib_v5* p_NDI;
    volatile bool stopFlag;
};

// ---------------------------------------------------------------------------
// NdiManager
// ---------------------------------------------------------------------------
NdiManager::NdiManager()
{
}

NdiManager::~NdiManager()
{
    uninitialize();
}

NdiManager& NdiManager::getInstance()
{
    return *ndiManager();
}

bool NdiManager::initialize()
{
    if (available)
        return true;

    if (!loadLibrary())
        return false;

    if (!p_NDI->initialize())
    {
        qWarning("NDI: NDIlib_initialize() failed");
        return false;
    }

    const char* ver = p_NDI->version ? p_NDI->version() : "unknown";
    qDebug("NDI: Initialized, runtime version: %s", ver);

    available = true;
    startDiscovery();

    return true;
}

void NdiManager::uninitialize()
{
    stopDiscovery();

    if (available && p_NDI)
    {
        p_NDI->destroy();
        available = false;
    }

    p_NDI = nullptr;

#ifdef Q_OS_WIN
    if (dllHandle)
    {
        FreeLibrary(static_cast<HMODULE>(dllHandle));
        dllHandle = nullptr;
    }
#endif
}

bool NdiManager::isAvailable() const
{
    return available;
}

QList<NdiSourceInfo> NdiManager::getSources() const
{
    QMutexLocker locker(&sourcesMutex);
    return sources;
}

const NDIlib_v5* NdiManager::api() const
{
    return available ? p_NDI : nullptr;
}

// ---------------------------------------------------------------------------
// DLL loading — uses the official NDI SDK dynamic loading pattern.
// Resolve just NDIlib_v5_load and call it to get the complete function table.
// ---------------------------------------------------------------------------
bool NdiManager::loadLibrary()
{
#ifdef Q_OS_WIN
    // NDILIB_REDIST_FOLDER is defined by the SDK as "NDI_RUNTIME_DIR_V6".
    // NDILIB_LIBRARY_NAME is defined as "Processing.NDI.Lib.x64.dll".

    // Check the application directory first (bundled DLL).
    QString dllPath;
    QString appDirDll = QCoreApplication::applicationDirPath() + "/" + NDILIB_LIBRARY_NAME;
    if (QFileInfo::exists(appDirDll))
    {
        dllPath = appDirDll;
    }
    else
    {
        // Fall back to the NDI Runtime install location.
        QString redistFolder = qEnvironmentVariable(NDILIB_REDIST_FOLDER);
        if (redistFolder.isEmpty())
        {
            qWarning("NDI: %s environment variable not set. "
                     "Is the NDI Runtime installed?", NDILIB_REDIST_FOLDER);
            return false;
        }
        dllPath = redistFolder + "\\" + NDILIB_LIBRARY_NAME;
    }

    HMODULE hModule = LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (!hModule)
    {
        qWarning("NDI: Failed to load %s (error %lu)",
                 qPrintable(dllPath), GetLastError());
        return false;
    }

    dllHandle = hModule;

    // Resolve the single entry point that returns the full function table.
    typedef const NDIlib_v5* (*NDIlib_v5_load_fn)(void);
    auto loadFn = reinterpret_cast<NDIlib_v5_load_fn>(
        GetProcAddress(hModule, "NDIlib_v5_load"));
    if (!loadFn)
    {
        qWarning("NDI: Failed to resolve NDIlib_v5_load");
        FreeLibrary(hModule);
        dllHandle = nullptr;
        return false;
    }

    p_NDI = loadFn();
    if (!p_NDI)
    {
        qWarning("NDI: NDIlib_v5_load() returned NULL");
        FreeLibrary(hModule);
        dllHandle = nullptr;
        return false;
    }

    return true;
#else
    qWarning("NDI: Only supported on Windows");
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Source discovery thread
// ---------------------------------------------------------------------------
void NdiManager::startDiscovery()
{
    if (discoveryThread)
        return;

    discoveryThread = new QThread(this);
    discoveryThread->setObjectName("NdiDiscovery");

    discoveryWorker = new DiscoveryWorker(p_NDI);
    discoveryWorker->moveToThread(discoveryThread);

    QObject::connect(discoveryThread, &QThread::started, discoveryWorker, &DiscoveryWorker::run);
    QObject::connect(discoveryWorker, &DiscoveryWorker::sourcesDiscovered,
                     this, &NdiManager::onSourcesDiscovered);

    discoveryThread->start();
}

void NdiManager::stopDiscovery()
{
    if (!discoveryThread)
        return;

    discoveryWorker->requestStop();
    discoveryThread->quit();
    discoveryThread->wait(3000);

    delete discoveryWorker;
    discoveryWorker = nullptr;

    delete discoveryThread;
    discoveryThread = nullptr;
}

void NdiManager::onSourcesDiscovered(const QList<NdiSourceInfo>& newSources)
{
    {
        QMutexLocker locker(&sourcesMutex);
        sources = newSources;
    }

    emit sourcesChanged();
}

// Pull in the moc file for the nested DiscoveryWorker class.
#include "NdiManager.moc"
