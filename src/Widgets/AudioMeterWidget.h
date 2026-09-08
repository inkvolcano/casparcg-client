#pragma once

#include "Shared.h"
#include "ui_AudioMeterWidget.h"

#include "MeterBallistics.h"

#include "OscSubscription.h"
#include "Events/Inspector/ChannelChangedEvent.h"
#include "Events/Inspector/DeviceChangedEvent.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtWidgets/QWidget>

class WIDGETS_EXPORT AudioMeterWidget : public QWidget, Ui::AudioMeterWidget
{
    Q_OBJECT

    public:
        explicit AudioMeterWidget(QWidget* parent = 0);

        void configureAudioMeter(int channel);
        void configureForDevice(int audioChannel, const QString& deviceName, int serverChannel);

    protected:
        void paintEvent(QPaintEvent* event) override;

        // Clicking the meter clears a latched clip. There is nowhere else to put
        // it - the panel is a row of meters with no room for a button each - and
        // it is what every mixer in the building already does.
        void mousePressEvent(QMouseEvent* event) override;

    private:
        int channel;
        double currentLevel;

        // What the meter shows between readings: the bar's fall, the peak marker
        // and whether a clip has been seen and not yet acknowledged.
        MeterBallistics::Meter meter;
        QTimer* decayTimer = nullptr;
        bool peakHoldEnabled = true;
        bool clipIndicatorEnabled = true;
        LibraryModel* model;
        AbstractCommand* command;

        bool directMode = false;

        OscSubscription* audioSubscription;

        double convertToLevel(int value);
        void configureOscSubscriptions();

        Q_SLOT void deviceChanged(const DeviceChangedEvent&);
        Q_SLOT void channelChanged(const ChannelChangedEvent&);
        Q_SLOT void emptyRundown(const EmptyRundownEvent&);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void audioSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void decayTick();
};
