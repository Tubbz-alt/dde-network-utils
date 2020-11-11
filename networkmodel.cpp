/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "networkmodel.h"
#include "networkdevice.h"
#include "wirelessdevice.h"
#include "wireddevice.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDBusObjectPath>

using namespace dde::network;

#define CONNECTED  2

Connectivity NetworkModel::m_Connectivity(Connectivity::Full);

NetworkDevice::DeviceType parseDeviceType(const QString &type)
{
    if (type == "wireless") {
        return NetworkDevice::Wireless;
    }
    if (type == "wired") {
        return NetworkDevice::Wired;
    }

    return NetworkDevice::None;
}

NetworkModel::NetworkModel(QObject *parent)
    : QObject(parent)
    , m_lastSecretDevice(nullptr)
    , m_connectivityChecker(new ConnectivityChecker(this))
    , m_connectivityCheckThread(new QThread(this))
{
    connect(this, &NetworkModel::needCheckConnectivitySecondary,
            m_connectivityChecker, &ConnectivityChecker::startCheck);
    connect(m_connectivityChecker, &ConnectivityChecker::checkFinished,
            this, &NetworkModel::onConnectivitySecondaryCheckFinished);
    m_connectivityChecker->moveToThread(m_connectivityCheckThread);
}

NetworkModel::~NetworkModel()
{
    qDeleteAll(m_devices);
}

const QString NetworkModel::connectionUuidByPath(const QString &connPath) const
{
    return connectionByPath(connPath).value("Uuid").toString();
}

const QString NetworkModel::connectionNameByUuid(const QString &connUuid) const
{
    return connectionByUuid(connUuid).value("Id").toString();
}

const QJsonObject NetworkModel::connectionByPath(const QString &connPath) const
{
    for (const auto &list : m_connections)
    {
        for (const auto &cfg : list)
        {
            if (cfg.value("Path").toString() == connPath)
                return cfg;
        }
    }

    return QJsonObject();
}

const QJsonObject NetworkModel::activeConnObjectByUuid(const QString &uuid) const
{
    for (const auto &info : m_activeConns)
    {
        if (info.value("Uuid").toString() == uuid)
            return info;
    }

    return QJsonObject();
}

const QString NetworkModel::connectionUuidByApInfo(const QJsonObject &apInfo) const
{
    for (const auto &list : m_connections)
    {
        for (const auto &cfg : list)
        {
            if (cfg.value("Ssid").toString() == apInfo.value("Ssid").toString())
                return cfg.value("Uuid").toString();
        }
    }

    return QString();
}

const QString NetworkModel::activeConnUuidByInfo(const QString &devPath, const QString &id) const
{
    for (const auto &info : m_activeConns)
    {
        if (info.value("Id").toString() != id)
            continue;

        if (info.value("Devices").toArray().contains(devPath))
            return info.value("Uuid").toString();
    }

    return QString();
}

const QJsonObject NetworkModel::connectionByUuid(const QString &uuid) const
{
    for (const auto &list : m_connections)
    {
        for (const auto &cfg : list)
        {
            if (cfg.value("Uuid").toString() == uuid)
                return cfg;
        }
    }

    return QJsonObject();
}

void NetworkModel::onActivateAccessPointDone(const QString &devPath, const QString &apPath, const QString &uuid, const QDBusObjectPath path)
{
    for (auto const dev : m_devices)
    {
        if (dev->type() != NetworkDevice::Wireless || dev->path() != devPath)
            continue;
        //当wifi是企业wifi的时候，这个才能进去，这个时候前端会去做判断。打开详情页
        if (path.path().isEmpty()) {
            Q_EMIT static_cast<WirelessDevice *>(dev)->activateAccessPointFailed(apPath, uuid);
            return;
        }
    }
}

void NetworkModel::onVPNEnabledChanged(const bool enabled)
{
    if (m_vpnEnabled != enabled)
    {
        m_vpnEnabled = enabled;

        Q_EMIT vpnEnabledChanged(m_vpnEnabled);
    }
}

void NetworkModel::onProxiesChanged(const QString &type, const QString &url, const uint port)
{
    const ProxyConfig config = { port, type, url, "", "" };
    const ProxyConfig old = m_proxies[type];


    if (old.url != config.url || old.port != config.port)
    {
        m_proxies[type] = config;

        Q_EMIT proxyChanged(type, config);
    }
}

void NetworkModel::onAutoProxyChanged(const QString &proxy)
{
    if (m_autoProxy != proxy)
    {
        m_autoProxy = proxy;

        Q_EMIT autoProxyChanged(m_autoProxy);
    }
}

void NetworkModel::onProxyMethodChanged(const QString &proxyMethod)
{
    if (m_proxyMethod != proxyMethod)
    {
        m_proxyMethod = proxyMethod;

        Q_EMIT proxyMethodChanged(m_proxyMethod);
    }
}

void NetworkModel::onProxyIgnoreHostsChanged(const QString &hosts)
{
    if (hosts != m_proxyIgnoreHosts)
    {
        m_proxyIgnoreHosts = hosts;

        Q_EMIT proxyIgnoreHostsChanged(m_proxyIgnoreHosts);
    }
}

void NetworkModel::onDevicesChanged(const QString &devices)
{
    const QJsonObject data = QJsonDocument::fromJson(devices.toUtf8()).object();

    QSet<QString> devSet;

    bool changed = false;

    for (auto it(data.constBegin()); it != data.constEnd(); ++it) {
        const auto type = parseDeviceType(it.key());
        const auto list = it.value().toArray();

        if (type == NetworkDevice::None)
            continue;

        for (auto const &l : list)
        {
            const auto info = l.toObject();
            const QString path = info.value("Path").toString();

            if (!devSet.contains(path)) {
                devSet << path;
            }

            NetworkDevice *d = device(path);
            if (!d)
            {
                changed = true;

                switch (type)
                {
                case NetworkDevice::Wireless:   d = new WirelessDevice(info, this);      break;
                case NetworkDevice::Wired:      d = new WiredDevice(info, this);         break;
                default:;
                }
                m_devices.append(d);

            } else {
                //这里由于Devices属性中的数据目前的状态会不更新，所以导致了状态可能出现不正确的情况，
                //所以这里将updateDeviceInfo中的设置操作给关闭了，如果后期后端将数据同步正确可以打开
                d->updateDeviceInfo(info);
            }
        }
    }

    // remove unexist device
    QList<NetworkDevice *> removeList;
    for (auto const d : m_devices)
    {
        if (!devSet.contains(d->path()))
            removeList << d;
    }

    for (auto const r : removeList) {
        m_devices.removeOne(r);
        r->deleteLater();
    }

    if (!removeList.isEmpty()) {
        changed = true;
    }

    if (changed) {
        Q_EMIT deviceListChanged(m_devices);
    }
}

void NetworkModel::onAirplaneModeEnableChanged(const bool enabled)
{
    qDebug() << Q_FUNC_INFO;
    for (NetworkDevice *dev : m_devices) {
        if (!dev)
            continue;
        if (dev->type() != NetworkDevice::Wireless)
            continue;
        if (enabled)
            //由于飞行模式会将底层的无线网适配器进行禁用，所以在wifi适配器打开的时候进行初始化，可以关闭打开飞行模式没有数据的情况
            (static_cast<WirelessDevice *>(dev))->initWirelessData();
       else
            Q_EMIT requestDeviceEnable(dev->path(), enabled);

    }
}

void NetworkModel::onActiveConnInfoChanged(const QString &conns)
{
    m_activeConnInfos.clear();

    QMap<QString, QJsonObject> activeConnInfo;
    QMap<QString, QJsonObject> activeHotspotInfo;

    // parse active connections info and save it by DevicePath
    QJsonArray activeConns = QJsonDocument::fromJson(conns.toUtf8()).array();
    for (const auto &info : activeConns)
    {
        const auto &connInfo = info.toObject();
        const auto &type = connInfo.value("ConnectionType").toString();
        const auto &devPath = connInfo.value("Device").toString();

        activeConnInfo.insertMulti(devPath, connInfo);
        m_activeConnInfos << connInfo;

        if (type == "wireless-hotspot") {
            activeHotspotInfo.insert(devPath, connInfo);
        }
    }

    // update device active connection
    for (auto *dev : m_devices)
    {
        const auto &devPath = dev->path();

        switch (dev->type()) {
        case NetworkDevice::Wired: {
            WiredDevice *d = static_cast<WiredDevice *>(dev);
            d->setActiveConnectionsInfo(activeConnInfo.values(devPath));
            break;
        }
        case NetworkDevice::Wireless: {
            WirelessDevice *d = static_cast<WirelessDevice *>(dev);
            d->setActiveConnectionsInfo(activeConnInfo.values(devPath));
//            d->setActiveHotspotInfo(activeHotspotInfo.value(devPath));
            break;
        }
        default:;
        }
    }
    Q_EMIT activeConnInfoChanged(m_activeConnInfos);
}

void NetworkModel::onConnectionSessionCreated(const QString &device, const QString &sessionPath)
{
    for (const auto dev : m_devices)
    {
        if (dev->path() != device)
            continue;
        Q_EMIT dev->sessionCreated(sessionPath);
        return;
    }

    Q_EMIT unhandledConnectionSessionCreated(device, sessionPath);
}

void NetworkModel::onDeviceEnableChanged(const QString &device, const bool enabled)
{
    qDebug() << Q_FUNC_INFO << enabled;
    NetworkDevice *dev = nullptr;
    for (auto const d : m_devices)
    {
        if (d->path() == device)
        {
            dev = d;
            break;
        }
    }

    if (!dev)
        return;

    dev->setEnabled(enabled);

    Q_EMIT deviceEnableChanged(device, enabled);
}

void NetworkModel::onChainsTypeChanged(const QString &type)
{
    if (type != m_chainsProxy.type) {
        m_chainsProxy.type = type;
        Q_EMIT chainsTypeChanged(type);
    }
}

void NetworkModel::onChainsAddrChanged(const QString &addr)
{
    if (addr != m_chainsProxy.url) {
        m_chainsProxy.url = addr;
        Q_EMIT chainsAddrChanged(addr);
    }
}

void NetworkModel::onChainsPortChanged(const uint port)
{
    if (port != m_chainsProxy.port) {
        m_chainsProxy.port = port;
        Q_EMIT chainsPortChanged(port);
    }
}

void NetworkModel::onChainsUserChanged(const QString &user)
{
    if (user != m_chainsProxy.username) {
        m_chainsProxy.username = user;
        Q_EMIT chainsUsernameChanged(user);
    }
}

void NetworkModel::onChainsPasswdChanged(const QString &passwd)
{
    if (passwd != m_chainsProxy.password) {
        m_chainsProxy.password = passwd;
        Q_EMIT chainsPasswdChanged(passwd);
    }
}

void NetworkModel::onConnectivitySecondaryCheckFinished(bool connectivity)
{
    m_Connectivity = connectivity ? Full : NoConnectivity;
    Q_EMIT connectivityChanged(m_Connectivity);
}

bool NetworkModel::containsDevice(const QString &devPath) const
{
    return device(devPath) != nullptr;
}

NetworkDevice *NetworkModel::device(const QString &devPath) const
{
    for (auto const d : m_devices)
        if (d->path() == devPath)
            return d;

    return nullptr;
}

void NetworkModel::onAppProxyExistChanged(bool appProxyExist)
{
    if (m_appProxyExist == appProxyExist) {
        return;
    }

    m_appProxyExist = appProxyExist;

    Q_EMIT appProxyExistChanged(appProxyExist);
}

void NetworkModel::onDeviceEnable(const QString &path, bool enabled) const
{
    qDebug() << Q_FUNC_INFO << enabled;
    //发送前端需要切换状态的信号
    Q_EMIT requestDeviceEnable(path, enabled);
    if (enabled)
        Q_EMIT updateApList();

}

void NetworkModel::WirelessAccessPointsChanged(const QString &WirelessList)
{
    //当数据非json的时候,则这个里面的项为0,则下面的for不会被执行
    QJsonObject WirelessData = QJsonDocument::fromJson(WirelessList.toUtf8()).object();
    for (QString Device : WirelessData.keys()) {
        for (auto const dev : m_devices) {
            //当类型不为无线网,path不为当前需要的device则进入下一个循环
            if (dev->type() != NetworkDevice::Wireless || dev->path() != Device) continue;
            return dynamic_cast<WirelessDevice *>(dev)->WirelessUpdate(WirelessData.value(Device));
        }
    }
}

void NetworkModel::onWiredDataChange(const QString &conns)
{
    QMap< QString, QList< QJsonObject>> commonConnections;
    QMap< QString, QMap< QString, QList< QJsonObject>>> deviceConnections;

    // 解析所有的 connection
    const QJsonObject connsObject = QJsonDocument::fromJson(conns.toUtf8()).object();
    for (auto it(connsObject.constBegin()); it != connsObject.constEnd(); ++it) {
        const auto &connList = it.value().toArray();
        const auto &connType = it.key();
        //由于无线网数据改用WirelessAccessPoints属性中获取数据，所以这里不再保存无线网的值
        if (connType.isEmpty() || connType == "wireless")
            continue;

        m_connections[connType].clear();

        for (const auto &connObject : connList) {
            const QJsonObject &connection = connObject.toObject();
            //向相应的添加相应的内容
            m_connections[connType].append(connection);

            const auto &hwAddr = connection.value("HwAddress").toString();
            if (hwAddr.isEmpty()) {
                commonConnections[connType].append(connection);
            } else {
                deviceConnections[hwAddr][connType].append(connection);
            }
        }
    }

    // 将 connections 分配给具体的设备
    for (NetworkDevice *dev : m_devices) {
        const QString &hwAddr = dev->realHwAdr();
        const QMap<QString, QList<QJsonObject>> &connsByType = deviceConnections.value(hwAddr);
        QList<QJsonObject> destConns;

        if (dev->type() == NetworkDevice::Wired) {
            destConns += commonConnections.value("wired");
            destConns += connsByType.value("wired");
            WiredDevice *wdDevice = static_cast<WiredDevice *>(dev);
            qDebug() << "destConns: " << destConns;
            wdDevice->setConnections(destConns);
        }

    }
    //这个信号是给DSL、VPN等使用的信号
    Q_EMIT connectionListChanged();
}

void NetworkModel::onActiveConnections(const QString &conns)
{
    m_activeConns.clear();
    // 按照设备分类所有 active 连接
    QMap<QString, QList<QJsonObject>> deviceActiveConnsMap;
    const QJsonObject activeConns = QJsonDocument::fromJson(conns.toUtf8()).object();
    for (auto it = activeConns.constBegin(); it != activeConns.constEnd(); ++it)
    {
        const QJsonObject &info = it.value().toObject();
        //数据为空
        if (info.isEmpty()) continue;
        //添加到活动的列表中
        m_activeConns << info;
        //状态值
        int connectionState = info.value("State").toInt();

        for (const auto &item : info.value("Devices").toArray()) {
            const QString &devicePath = item.toString();
            if (devicePath.isEmpty()) {
                continue;
            }
            deviceActiveConnsMap[devicePath] << info;

            NetworkDevice *dev = device(devicePath);
            if (dev != nullptr) {
                //这个后端状态给的不对问题，这一段后端说直接从networkmanage中拿得，没办法改，所以让前端规避
                if (dev->status() != NetworkDevice::DeviceStatus::Activated && connectionState == CONNECTED) {
                    qDebug() << devicePath << "The active connection status does not match the device connection status. It has been changed";
                    dev->setDeviceStatus(NetworkDevice::DeviceStatus::Activated);
                }
            }
        }
    }
    // 将 active 连接分配给具体的设备
    for (auto it = deviceActiveConnsMap.constBegin(); it != deviceActiveConnsMap.constEnd(); ++it) {
        NetworkDevice *dev = device(it.key());
        if (dev == nullptr) {
            continue;
        }
        switch (dev->type()) {
            case NetworkDevice::Wired: {
                WiredDevice *wdDevice = static_cast<WiredDevice *>(dev);
                wdDevice->setActiveConnections(it.value());                  
                break;
            }
            case NetworkDevice::Wireless: {
                WirelessDevice *wsDevice = static_cast<WirelessDevice *>(dev);
                wsDevice->setActiveConnections(it.value());
                break;
            }
            default:
                break;
        }
        //dock由于每次不刷新，所以需要这个信号来触发刷新
        Q_EMIT activeConnectionsChanged(it.value());
    }
    //当网卡处于断开连接状态的时候，active接口中并没有相应的数据，所以需要发送空给相应的适配器
    for (NetworkDevice *device : m_devices) {
        if (deviceActiveConnsMap.contains(device->path())) {
            continue;
        }
        switch (device->type()) {
            case NetworkDevice::Wired: {
                WiredDevice *wdDevice = static_cast<WiredDevice *>(device);
                wdDevice->setActiveConnections(QList<QJsonObject>());
                break;
            }
            case NetworkDevice::Wireless: {
                WirelessDevice *wsDevice = static_cast<WirelessDevice *>(device);
                wsDevice->setActiveConnections(QList<QJsonObject>());
                break;
            }
            default:
                break;
        }

    }
}
