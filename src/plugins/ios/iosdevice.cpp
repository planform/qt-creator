/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "iosdevice.h"
#include "iosconstants.h"
#include "iosconfigurations.h"
#include "iostoolhandler.h"
#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/kitinformation.h>

#include <QCoreApplication>
#include <QVariant>
#include <QVariantMap>
#include <QMessageBox>

#ifdef Q_OS_MAC
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace ProjectExplorer;

static bool debugDeviceDetection = false;

#ifdef Q_OS_MAC
static QString CFStringRef2QString(CFStringRef s)
{
    unsigned char buf[250];
    CFIndex len = CFStringGetLength(s);
    CFIndex usedBufLen;
    CFIndex converted = CFStringGetBytes(s, CFRangeMake(0,len), kCFStringEncodingUTF8,
                                         '?', false, &buf[0], sizeof(buf), &usedBufLen);
    if (converted == len)
        return QString::fromUtf8(reinterpret_cast<char *>(&buf[0]), usedBufLen);
    size_t bufSize = sizeof(buf)
            + CFStringGetMaximumSizeForEncoding(len - converted, kCFStringEncodingUTF8);
    unsigned char *bigBuf = new unsigned char[bufSize];
    memcpy(bigBuf, buf, usedBufLen);
    CFIndex newUseBufLen;
    CFStringGetBytes(s, CFRangeMake(converted,len), kCFStringEncodingUTF8,
                     '?', false, &bigBuf[usedBufLen], bufSize, &newUseBufLen);
    QString res = QString::fromUtf8(reinterpret_cast<char *>(bigBuf), usedBufLen + newUseBufLen);
    delete[] bigBuf;
    return res;
}
#endif

namespace Ios {
namespace Internal {

const char extraInfoKey[] = "extraInfo";

IosDevice::IosDevice()
    : IDevice(Core::Id(Constants::IOS_DEVICE_TYPE),
                             IDevice::AutoDetected,
                             IDevice::Hardware,
                             Constants::IOS_DEVICE_ID)
{
    setDisplayName(QCoreApplication::translate("Ios::Internal::IosDevice", "iOS Device"));
    setDeviceState(DeviceStateUnknown);
}

IosDevice::IosDevice(const IosDevice &other)
    : IDevice(other), m_extraInfo(other.m_extraInfo), m_ignoreDevice(other.m_ignoreDevice)
{ }

IosDevice::IosDevice(const QString &uid)
    : IDevice(Core::Id(Constants::IOS_DEVICE_TYPE),
                             IDevice::AutoDetected,
                             IDevice::Hardware,
                             Core::Id(Constants::IOS_DEVICE_ID).withSuffix(uid))
{
    setDisplayName(QCoreApplication::translate("Ios::Internal::IosDevice", "iOS Device"));
    setDeviceState(DeviceStateUnknown);
}


IDevice::DeviceInfo IosDevice::deviceInformation() const
{
    IDevice::DeviceInfo res;
    QMapIterator<QString, QString> i(m_extraInfo);
    while (i.hasNext()) {
        i.next();
        IosDeviceManager::TranslationMap tMap = IosDeviceManager::translationMap();
        if (tMap.contains(i.key()))
            res.append(DeviceInfoItem(tMap.value(i.key()), tMap.value(i.value(), i.value())));
    }
    return res;
}

QString IosDevice::displayType() const
{
    return QCoreApplication::translate("Ios::Internal::IosDevice", "iOS");
}

IDeviceWidget *IosDevice::createWidget()
{
    return 0;
}

QList<Core::Id> IosDevice::actionIds() const
{
    return QList<Core::Id>(); // add activation action?
}

QString IosDevice::displayNameForActionId(Core::Id actionId) const
{
    Q_UNUSED(actionId)
    return QString();
}

void IosDevice::executeAction(Core::Id actionId, QWidget *parent)
{
    Q_UNUSED(actionId)
    Q_UNUSED(parent)
}

DeviceProcessSignalOperation::Ptr IosDevice::signalOperation() const
{
    return DeviceProcessSignalOperation::Ptr();
}

IDevice::Ptr IosDevice::clone() const
{
    return IDevice::Ptr(new IosDevice(*this));
}

void IosDevice::fromMap(const QVariantMap &map)
{
    IDevice::fromMap(map);
    QVariantMap vMap = map.value(QLatin1String(extraInfoKey)).toMap();
    QMapIterator<QString, QVariant> i(vMap);
    m_extraInfo.clear();
    while (i.hasNext()) {
        i.next();
        m_extraInfo.insert(i.key(), i.value().toString());
    }
}

QVariantMap IosDevice::toMap() const
{
    QVariantMap res = IDevice::toMap();
    QVariantMap vMap;
    QMapIterator<QString, QString> i(m_extraInfo);
    while (i.hasNext()) {
        i.next();
        vMap.insert(i.key(), i.value());
    }
    res.insert(QLatin1String(extraInfoKey), vMap);
    return res;
}

QString IosDevice::uniqueDeviceID() const
{
    return id().suffixAfter(Core::Id(Constants::IOS_DEVICE_ID));
}
/*
// add back?

QString IosDevice::cpuArchitecure() const
{
    return m_extraInfo.value(QLatin1String("deviceInfo")).toMap()
            .value(QLatin1String("CPUArchitecture")).toString();
}

QString IosDevice::productType() const
{
    return m_extraInfo.value(QLatin1String("deviceInfo")).toMap()
            .value(QLatin1String("ProductType")).toString();
}*/


// IosDeviceManager

IosDeviceManager::TranslationMap IosDeviceManager::translationMap()
{
    static TranslationMap *translationMap = 0;
    if (translationMap)
        return *translationMap;
    TranslationMap &tMap = *new TranslationMap;
    tMap[QLatin1String("deviceName")]      = tr("Device name");
    tMap[QLatin1String("developerStatus")] = tr("Developer status");
    tMap[QLatin1String("deviceConnected")] = tr("Connected");
    tMap[QLatin1String("YES")]             = tr("yes");
    tMap[QLatin1String("NO")]              = tr("no");
    tMap[QLatin1String("YES")]             = tr("yes");
    tMap[QLatin1String("*unknown*")]       = tr("unknown");
    translationMap = &tMap;
    return tMap;
}

void IosDeviceManager::deviceConnected(const QString &uid, const QString &name)
{
    DeviceManager *devManager = DeviceManager::instance();
    Core::Id baseDevId(Constants::IOS_DEVICE_ID);
    Core::Id devType(Constants::IOS_DEVICE_TYPE);
    Core::Id devId = baseDevId.withSuffix(uid);
    IDevice::ConstPtr dev = devManager->find(devId);
    if (dev.isNull()) {
        IosDevice *newDev = new IosDevice(uid);
        if (!name.isNull())
            newDev->setDisplayName(name);
        if (debugDeviceDetection)
            qDebug() << "adding ios device " << uid;
        devManager->addDevice(IDevice::ConstPtr(newDev));
    } else if (dev->deviceState() != IDevice::DeviceConnected &&
               dev->deviceState() != IDevice::DeviceReadyToUse) {
        if (debugDeviceDetection)
            qDebug() << "updating ios device " << uid;
        IosDevice *newDev = 0;
        if (dev->type() == devType) {
            const IosDevice *iosDev = static_cast<const IosDevice *>(dev.data());
            newDev = new IosDevice(*iosDev);
        } else {
            newDev = new IosDevice(uid);
        }
        devManager->addDevice(IDevice::ConstPtr(newDev));
    }
    updateInfo(uid);
}

void IosDeviceManager::deviceDisconnected(const QString &uid)
{
    if (debugDeviceDetection)
        qDebug() << "detected disconnection of ios device " << uid;
    DeviceManager *devManager = DeviceManager::instance();
    Core::Id baseDevId(Constants::IOS_DEVICE_ID);
    Core::Id devType(Constants::IOS_DEVICE_TYPE);
    Core::Id devId = baseDevId.withSuffix(uid);
    IDevice::ConstPtr dev = devManager->find(devId);
    if (dev.isNull() || dev->type() != devType) {
        qDebug() << "ignoring disconnection of ios device " << uid; // should neve happen
    } else {
        const IosDevice *iosDev = static_cast<const IosDevice *>(dev.data());
        if (iosDev->deviceState() != IDevice::DeviceDisconnected) {
            if (debugDeviceDetection)
                qDebug() << "disconnecting device " << iosDev->uniqueDeviceID();
            devManager->setDeviceState(iosDev->id(), IDevice::DeviceDisconnected);
        }
    }
}

void IosDeviceManager::updateInfo(const QString &devId)
{
    IosToolHandler *requester = new IosToolHandler(IosToolHandler::IosDeviceType, this);
    connect(requester, SIGNAL(deviceInfo(Ios::IosToolHandler*,QString,Ios::IosToolHandler::Dict)),
            SLOT(deviceInfo(Ios::IosToolHandler*,QString,Ios::IosToolHandler::Dict)), Qt::QueuedConnection);
    connect(requester, SIGNAL(finished(Ios::IosToolHandler*)),
            SLOT(infoGathererFinished(Ios::IosToolHandler*)));
    requester->requestDeviceInfo(devId);
}

void IosDeviceManager::deviceInfo(IosToolHandler *, const QString &uid,
                                  const Ios::IosToolHandler::Dict &info)
{
    DeviceManager *devManager = DeviceManager::instance();
    Core::Id baseDevId(Constants::IOS_DEVICE_ID);
    Core::Id devType(Constants::IOS_DEVICE_TYPE);
    Core::Id devId = baseDevId.withSuffix(uid);
    IDevice::ConstPtr dev = devManager->find(devId);
    bool skipUpdate = false;
    IosDevice *newDev = 0;
    if (!dev.isNull() && dev->type() == devType) {
        const IosDevice *iosDev = static_cast<const IosDevice *>(dev.data());
        if (iosDev->m_extraInfo == info) {
            skipUpdate = true;
            newDev = const_cast<IosDevice *>(iosDev);
        } else {
            newDev = new IosDevice(*iosDev);
        }
    } else {
        newDev = new IosDevice(uid);
    }
    if (!skipUpdate) {
        QString devNameKey = QLatin1String("deviceName");
        if (info.contains(devNameKey))
            newDev->setDisplayName(info.value(devNameKey));
        newDev->m_extraInfo = info;
        if (debugDeviceDetection)
            qDebug() << "updated info of ios device " << uid;
        dev = IDevice::ConstPtr(newDev);
        devManager->addDevice(dev);
    }
    QLatin1String devStatusKey = QLatin1String("developerStatus");
    if (info.contains(devStatusKey)) {
        QString devStatus = info.value(devStatusKey);
        if (devStatus == QLatin1String("*off*")) {
            devManager->setDeviceState(newDev->id(), IDevice::DeviceConnected);
            if (!newDev->m_ignoreDevice && !IosConfigurations::instance().config().ignoreAllDevices) {
                QMessageBox mBox;
                mBox.setText(tr("An iOS device in user mode has been detected."));
                mBox.setInformativeText(tr("Do you want to see how to set it up for development?"));
                mBox.setStandardButtons(QMessageBox::NoAll | QMessageBox::No | QMessageBox::Yes);
                mBox.setDefaultButton(QMessageBox::No);
                int ret = mBox.exec();
                switch (ret) {
                case QMessageBox::Yes:
                    // open doc
                    break;
                case QMessageBox::No:
                    newDev->m_ignoreDevice = true;
                    break;
                case QMessageBox::NoAll:
                {
                    IosConfig conf = IosConfigurations::instance().config();
                    conf.ignoreAllDevices = true;
                    IosConfigurations::instance().setConfig(conf);
                    break;
                }
                default:
                break;
                }
            }
        } else if (devStatus == QLatin1String("Development")) {
            devManager->setDeviceState(newDev->id(), IDevice::DeviceReadyToUse);
        } else {
            devManager->setDeviceState(newDev->id(), IDevice::DeviceConnected);
        }
    }
}

void IosDeviceManager::infoGathererFinished(IosToolHandler *gatherer)
{
    gatherer->deleteLater();
}

#ifdef Q_OS_MAC
namespace {
io_iterator_t gAddedIter;
io_iterator_t gRemovedIter;

extern "C" {
void deviceConnectedCallback(void *refCon, io_iterator_t iterator)
{
    kern_return_t       kr;
    io_service_t        usbDevice;
    (void) refCon;

    while ((usbDevice = IOIteratorNext(iterator))) {
        io_name_t       deviceName;

        // Get the USB device's name.
        kr = IORegistryEntryGetName(usbDevice, deviceName);
        QString name;
        if (KERN_SUCCESS == kr)
            name = QString::fromLocal8Bit(deviceName);
        if (debugDeviceDetection)
            qDebug() << "ios device " << name << " in deviceAddedCallback";

        CFStringRef cfUid = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(
                                                         usbDevice,
                                                         CFSTR(kUSBSerialNumberString),
                                                         kCFAllocatorDefault, 0));
        QString uid = CFStringRef2QString(cfUid);
        CFRelease(cfUid);
        IosDeviceManager::instance()->deviceConnected(uid, name);

        // Done with this USB device; release the reference added by IOIteratorNext
        kr = IOObjectRelease(usbDevice);
    }
}

void deviceDisconnectedCallback(void *refCon, io_iterator_t iterator)
{
    kern_return_t       kr;
    io_service_t        usbDevice;
    (void) refCon;

    while ((usbDevice = IOIteratorNext(iterator))) {
        io_name_t       deviceName;

        // Get the USB device's name.
        kr = IORegistryEntryGetName(usbDevice, deviceName);
        if (KERN_SUCCESS != kr)
            deviceName[0] = '\0';
        if (debugDeviceDetection)
            qDebug() << "ios device " << deviceName << " in deviceDisconnectedCallback";

        {
            CFStringRef cfUid = static_cast<CFStringRef>(IORegistryEntryCreateCFProperty(
                                                             usbDevice,
                                                             CFSTR(kUSBSerialNumberString),
                                                             kCFAllocatorDefault, 0));
            QString uid = CFStringRef2QString(cfUid);
            CFRelease(cfUid);
            IosDeviceManager::instance()->deviceDisconnected(uid);
        }

        // Done with this USB device; release the reference added by IOIteratorNext
        kr = IOObjectRelease(usbDevice);
    }
}

} // extern C

} // anonymous namespace
#endif

void IosDeviceManager::monitorAvailableDevices()
{
#ifdef Q_OS_MAC
    CFMutableDictionaryRef  matchingDictionary =
                                        IOServiceMatching("IOUSBDevice" );
    {
        UInt32 vendorId = 0x05ac;
        CFNumberRef cfVendorValue = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type,
                                                    &vendorId );
        CFDictionaryAddValue( matchingDictionary, CFSTR( kUSBVendorID ), cfVendorValue);
        CFRelease( cfVendorValue );
        UInt32 productId = 0x1280;
        CFNumberRef cfProductIdValue = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type,
                                                       &productId );
        CFDictionaryAddValue( matchingDictionary, CFSTR( kUSBProductID ), cfProductIdValue);
        CFRelease( cfProductIdValue );
        UInt32 productIdMask = 0xFFC0;
        CFNumberRef cfProductIdMaskValue = CFNumberCreate( kCFAllocatorDefault, kCFNumberSInt32Type,
                                                           &productIdMask );
        CFDictionaryAddValue( matchingDictionary, CFSTR( kUSBProductIDMask ), cfProductIdMaskValue);
        CFRelease( cfProductIdMaskValue );
    }

    IONotificationPortRef notificationPort = IONotificationPortCreate(kIOMasterPortDefault);
    CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(notificationPort);

    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopDefaultMode);

    // IOServiceAddMatchingNotification releases this, so we retain for the next call
    CFRetain(matchingDictionary);

    // Now set up a notification to be called when a device is first matched by I/O Kit.
    kern_return_t kr;
    kr = IOServiceAddMatchingNotification(notificationPort,
                                          kIOMatchedNotification,
                                          matchingDictionary,
                                          deviceConnectedCallback,
                                          NULL,
                                          &gAddedIter);


    kr = IOServiceAddMatchingNotification(notificationPort,
                                          kIOTerminatedNotification,
                                          matchingDictionary,
                                          deviceDisconnectedCallback,
                                          NULL,
                                          &gRemovedIter);

    // Iterate once to get already-present devices and arm the notification
    deviceConnectedCallback(NULL, gAddedIter);
    deviceDisconnectedCallback(NULL, gRemovedIter);
#endif
}


IosDeviceManager::IosDeviceManager(QObject *parent) :
    QObject(parent)
{
}

IosDeviceManager *IosDeviceManager::instance()
{
    static IosDeviceManager obj;
    return &obj;
}

void IosDeviceManager::updateAvailableDevices(const QStringList &devices)
{
    foreach (const QString &uid, devices)
        deviceConnected(uid);

    DeviceManager *devManager = DeviceManager::instance();
    for (int iDevice = 0; iDevice < devManager->deviceCount(); ++iDevice) {
        IDevice::ConstPtr dev = devManager->deviceAt(iDevice);
        Core::Id devType(Constants::IOS_DEVICE_TYPE);
        if (dev.isNull() || dev->type() != devType)
            continue;
        const IosDevice *iosDev = static_cast<const IosDevice *>(dev.data());
        if (devices.contains(iosDev->uniqueDeviceID()))
            continue;
        if (iosDev->deviceState() != IDevice::DeviceDisconnected) {
            if (debugDeviceDetection)
                qDebug() << "disconnecting device " << iosDev->uniqueDeviceID();
            devManager->setDeviceState(iosDev->id(), IDevice::DeviceDisconnected);
        }
    }
}

IosDevice::ConstPtr IosKitInformation::device(Kit *kit)
{
    if (!kit)
        return IosDevice::ConstPtr();
    ProjectExplorer::IDevice::ConstPtr dev = ProjectExplorer::DeviceKitInformation::device(kit);
    IosDevice::ConstPtr res = dev.dynamicCast<const IosDevice>();
    return res;
}

} // namespace Internal
} // namespace Ios
