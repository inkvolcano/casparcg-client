#pragma once

#include "Shared.h"
#include "ui_DeviceDialog.h"

#include "CasparDevice.h"

#include "Models/DeviceModel.h"

#include <QtCore/QList>
#include <QtCore/QSharedPointer>

#include <QtWidgets/QDialog>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT DeviceDialog : public QDialog, Ui::DeviceDialog
{
    Q_OBJECT

    public:
        explicit DeviceDialog(QWidget* parent = 0);

        void setDeviceModel(const DeviceModel& model);

        const QString getName() const;
        const QString getAddress() const;
        const QString getPort() const;
        const QString getUsername() const;
        const QString getPassword() const;
        const QString getDescription() const;
        const QString getShadow() const;
        int getPreviewChannel() const;
        int getLockedChannel() const;
        const QString getTemplatePath() const;
        const QString getMediaPath() const;
        const QString getServerPath() const;

    protected:
        void accept();
        bool eventFilter(QObject* target, QEvent* event);

    private:
        bool editMode;
        QLineEdit* lineEditServerPath = nullptr;

        QSharedPointer<CasparDevice> device;

        bool lookupName(const QString& name);
        bool lookupAddress(const QString& address);

        Q_SLOT void nameChanged(QString);
        Q_SLOT void addressChanged(QString);
        Q_SLOT void testConnection();
        Q_SLOT void connectionStateChanged(CasparDevice&);
        Q_SLOT void previewChannelChanged(int);
        Q_SLOT void lockedChannelChanged(int);
        Q_SLOT void browseTemplatePath();
        Q_SLOT void browseMediaPath();
        Q_SLOT void browseServerPath();
};
