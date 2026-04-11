#pragma once

#include "Shared.h"
#include "Processing.NDI.Lib.h"

#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QThread>

// Lightweight source descriptor shared with the UI layer.
// Stores only the owned QString name — raw NDI pointers are NOT safe
// to keep because they become stale after discovery refresh.
struct NDI_EXPORT NdiSourceInfo {
    QString name;            // Display name, e.g. "MACHINE (Channel 1)"

    NdiSourceInfo() {}
    explicit NdiSourceInfo(const QString& name_) : name(name_) {}
};

// Singleton that loads the NDI Runtime DLL, initialises NDI, and runs
// continuous source discovery on a background thread.
class NDI_EXPORT NdiManager : public QObject
{
    Q_OBJECT

public:
    static NdiManager& getInstance();

    // Attempt to load the NDI Runtime DLL and initialise NDI.
    // Returns true on success, false if the runtime is not installed.
    // Safe to call multiple times — subsequent calls return the cached result.
    bool initialize();

    // Shut down discovery, release NDI.
    void uninitialize();

    // Whether the NDI library was loaded and initialised successfully.
    bool isAvailable() const;

    // Thread-safe snapshot of currently discovered sources.
    QList<NdiSourceInfo> getSources() const;

    // Access the NDI function table (only valid after successful initialize()).
    const NDIlib_v5* api() const;

    Q_SIGNAL void sourcesChanged();

    explicit NdiManager();
    ~NdiManager();

private:

    // Background discovery worker.
    class DiscoveryWorker;
    friend class DiscoveryWorker;

    bool loadLibrary();
    void startDiscovery();
    void stopDiscovery();

    // DLL handle.
    void* dllHandle = nullptr;

    // NDI function table (returned by NDIlib_v5_load).
    const NDIlib_v5* p_NDI = nullptr;
    bool available = false;

    // Source discovery.
    QThread* discoveryThread = nullptr;
    DiscoveryWorker* discoveryWorker = nullptr;

    mutable QMutex sourcesMutex;
    QList<NdiSourceInfo> sources;

    Q_SLOT void onSourcesDiscovered(const QList<NdiSourceInfo>& newSources);
};
