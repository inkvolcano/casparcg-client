#pragma once

#include "Shared.h"

#include "ReferenceCaptureRules.h"

#include <QtCore/QObject>
#include <QtCore/QString>

#include <functional>

// "From Preview": a reference picture taken from a server's preview channel,
// by NDI or by a still the server writes (Common/ReferenceCaptureRules.h says
// which). Answers once, on the GUI thread, with the picture as a data URI ready
// for AbstractCommand::setReferenceImage, or with why there is none.
class WIDGETS_EXPORT ReferenceCapture : public QObject
{
    Q_OBJECT

    public:
        using Done = std::function<void(const QString& picture, const QString& problem, const QString& how)>;

        // `context` going away cancels the answer (the Inspector moved on).
        static void capture(QObject* context, const QString& deviceName, int itemChannel, const Done& done);

        static ReferenceCaptureRules::Setting setting();
        static void setSetting(ReferenceCaptureRules::Setting setting);

        // What is known about the channel, read now.
        static ReferenceCaptureRules::Situation situationFor(const QString& deviceName, int channel);

        // The NDI output the server's casparcg.config declares for a channel
        // (consumers/ndi, its name possibly empty). Reads the file only; NDI is
        // not started. configRead / declared say how far it got.
        static QString declaredNdiName(const QString& deviceName, int channel, bool* configRead = nullptr, bool* declared = nullptr);

        // The channel a capture reads: the server's preview channel, else the item's own.
        static int channelFor(const QString& deviceName, int itemChannel);

    private:
        static void captureNdi(QObject* context, const QString& sourceName, const Done& done);
        static void captureStill(QObject* context, const QString& deviceName, int channel, const Done& done);
};
