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

#include "networkworker.h"

#include <QMetaProperty>

using namespace dde::network;

NetworkWorker::NetworkWorker(NetworkModel *model, QObject *parent, bool sync)
    : QObject(parent),
      m_networkInter("com.deepin.daemon.Network", "/com/deepin/daemon/Network", QDBusConnection::sessionBus(), this),
      m_chainsInter(new ProxyChains("com.deepin.daemon.Network", "/com/deepin/daemon/Network/ProxyChains", QDBusConnection::sessionBus(), this)),
      m_airplaneMode("com.deepin.daemon.AirplaneMode", "/com/deepin/daemon/AirplaneMode", QDBusConnection::systemBus(), this),
      m_networkModel(model)
{
    //关联信号和槽
    initConnect();

    m_networkInter.setSync(true);
    m_chainsInter->setSync(true);
    //初始化网络操作
    active();


}
void NetworkWorker::initConnect()
{
    //监听适配器开关
    connect(&m_networkInter, &NetworkInter::DeviceEnabled, m_networkModel, &NetworkModel::onDeviceEnableChanged);
    connect(&m_airplaneMode, &AirplaneMode::WifiEnabledChanged, m_networkModel, &NetworkModel::onAirplaneModeEnableChanged);
    //适配器状态改变
    connect(&m_networkInter, &NetworkInter::DevicesChanged, m_networkModel, &NetworkModel::onDevicesChanged);
    //活动状态同步
    connect(&m_networkInter, &NetworkInter::ActiveConnectionsChanged, m_networkModel, &NetworkModel::onActiveConnections);
    //更新详细数据值
    connect(&m_networkInter, &NetworkInter::ActiveConnectionsChanged, this, &NetworkWorker::queryActiveConnInfo);
    //检测网络连接状态，在网络更新的时候会刷新网络状态
    connect(&m_networkInter, &NetworkInter::ActiveConnectionsChanged, m_networkModel, &NetworkModel::needCheckConnectivitySecondary);
    //处理有线连接数据和vpn数据
    connect(&m_networkInter, &NetworkInter::ConnectionsChanged, m_networkModel, &NetworkModel::onWiredDataChange);
    //处理无线连接的数据
    connect(&m_networkInter, &NetworkInter::WirelessAccessPointsChanged, m_networkModel, &NetworkModel::WirelessAccessPointsChanged);
    //前端请求刷新适配器开关
    connect(m_networkModel, &NetworkModel::initDeviceEnable, this, &NetworkWorker::queryDeviceStatus);
    //刷新网络
    connect(m_networkModel, &NetworkModel::updateApList, this, &NetworkWorker::requestWirelessScan);
    //适配器状态前端请求改变
    connect(m_networkModel, &NetworkModel::requestDeviceEnable, this, &NetworkWorker::setDeviceEnable);
    //前端请求断开连接wifi
    connect(m_networkModel, &NetworkModel::requestDisconnectAp, this, &NetworkWorker::disconnectDevice);
    //前端请求连接wifi
    connect(m_networkModel, &NetworkModel::requestConnectAp, this, &NetworkWorker::activateAccessPoint);
    //前端请求删除ap保存数据
    connect(m_networkModel, &NetworkModel::deleteConnection, this, &NetworkWorker::deleteConnection);
    //VPN状态监听
    connect(&m_networkInter, &NetworkInter::VpnEnabledChanged, m_networkModel, &NetworkModel::onVPNEnabledChanged);

    //代理接口
    connect(m_chainsInter, &ProxyChains::IPChanged, m_networkModel, &NetworkModel::onChainsAddrChanged);
    connect(m_chainsInter, &ProxyChains::PasswordChanged, m_networkModel, &NetworkModel::onChainsPasswdChanged);
    connect(m_chainsInter, &ProxyChains::TypeChanged, m_networkModel, &NetworkModel::onChainsTypeChanged);
    connect(m_chainsInter, &ProxyChains::UserChanged, m_networkModel, &NetworkModel::onChainsUserChanged);
    connect(m_chainsInter, &ProxyChains::PortChanged, m_networkModel, &NetworkModel::onChainsPortChanged);

    //网络详情页状态数据
    connect(m_networkModel, &NetworkModel::requestActionConnect, this, &NetworkWorker::queryActiveConnInfo);
}

void NetworkWorker::active(bool bSync)
{
    m_networkInter.blockSignals(false);

    //这个操作会去创建有线和无线的device，这个时候会给一个网络状态给前端，
        //这个状态目前看来是不准确的，所以需要再次对状态进行更新
    m_networkModel->onDevicesChanged(m_networkInter.devices());
    //更新连接状态
    m_networkModel->onActiveConnections(m_networkInter.activeConnections());
    //初始化有线数据
    m_networkModel->onWiredDataChange(m_networkInter.connections());
    //初始化无线数据
    m_networkModel->WirelessAccessPointsChanged(m_networkInter.wirelessAccessPoints());
    //初始化飞行模式和后端开关
    m_networkModel->onAirplaneModeEnableChanged(m_airplaneMode.wifiEnabled());
    //初始化vpn是否打开
    m_networkModel->onVPNEnabledChanged(m_networkInter.vpnEnabled());

}

void NetworkWorker::deactive()
{
    m_networkInter.blockSignals(true);
}

void NetworkWorker::setVpnEnable(const bool enable)
{
    m_networkInter.setVpnEnabled(enable);
}

void NetworkWorker::setDeviceEnable(const QString &devPath, const bool enable)
{
    qDebug() << Q_FUNC_INFO << enable;
    m_networkInter.EnableDevice(QDBusObjectPath(devPath), enable);
}

void NetworkWorker::setProxyMethod(const QString &proxyMethod)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.SetProxyMethod(proxyMethod), this);

    // requery result
    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryProxyMethod);
    connect(w, &QDBusPendingCallWatcher::finished, w, &QDBusPendingCallWatcher::deleteLater);
}

void NetworkWorker::setProxyIgnoreHosts(const QString &hosts)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.SetProxyIgnoreHosts(hosts), this);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryProxyIgnoreHosts);
    connect(w, &QDBusPendingCallWatcher::finished, w, &QDBusPendingCallWatcher::deleteLater);
}

void NetworkWorker::setAutoProxy(const QString &proxy)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.SetAutoProxy(proxy), this);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryAutoProxy);
    connect(w, &QDBusPendingCallWatcher::finished, w, &QDBusPendingCallWatcher::deleteLater);
}

void NetworkWorker::setProxy(const QString &type, const QString &addr, const QString &port)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.SetProxy(type, addr, port), this);

    connect(w, &QDBusPendingCallWatcher::finished, [ = ] { queryProxy(type); });
    connect(w, &QDBusPendingCallWatcher::finished, w, &QDBusPendingCallWatcher::deleteLater);
}

void NetworkWorker::setChainsProxy(const ProxyConfig &config)
{
    m_chainsInter->Set(config.type, config.url, config.port, config.username, config.password);
}

void NetworkWorker::onChainsTypeChanged(const QString &type)
{
    m_networkModel->onChainsTypeChanged(type);
}

void NetworkWorker::queryProxy(const QString &type)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.asyncCall(QStringLiteral("GetProxy"), type), this);

    w->setProperty("proxyType", type);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryProxyCB);
}

void NetworkWorker::requestWirelessScan()
{
    qDebug() << Q_FUNC_INFO;
    m_networkInter.RequestWirelessScan();
}

void NetworkWorker::queryChains()
{
    m_networkModel->onChainsTypeChanged(m_chainsInter->type());
    m_networkModel->onChainsAddrChanged(m_chainsInter->iP());
    m_networkModel->onChainsPortChanged(m_chainsInter->port());
    m_networkModel->onChainsUserChanged(m_chainsInter->user());
    m_networkModel->onChainsPasswdChanged(m_chainsInter->password());
}

void NetworkWorker::queryAutoProxy()
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.GetAutoProxy(), this);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryAutoProxyCB);
}

void NetworkWorker::queryProxyData()
{
    queryProxy("http");
    queryProxy("https");
    queryProxy("ftp");
    queryProxy("socks");
    queryAutoProxy();
    queryProxyMethod();
    queryProxyIgnoreHosts();
}

void NetworkWorker::queryProxyMethod()
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.GetProxyMethod(), this);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryProxyMethodCB);
}

void NetworkWorker::queryProxyIgnoreHosts()
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.GetProxyIgnoreHosts(), this);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryProxyIgnoreHostsCB);
}

void NetworkWorker::queryActiveConnInfo()
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.GetActiveConnectionInfo(), this);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryActiveConnInfoCB);
}

void NetworkWorker::queryDeviceStatus(const QString &devPath)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.IsDeviceEnabled(QDBusObjectPath(devPath)), this);

    w->setProperty("devPath", devPath);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::queryDeviceStatusCB);
}

void NetworkWorker::remanageDevice(const QString &devPath)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.SetDeviceManaged(devPath, false));

    connect(w, &QDBusPendingCallWatcher::finished, this, [=] { m_networkInter.SetDeviceManaged(devPath, true); });
    connect(w, &QDBusPendingCallWatcher::finished, w, &QDBusPendingCallWatcher::deleteLater);
}

void NetworkWorker::deleteConnection(const QString &uuid)
{
    m_networkInter.DeleteConnection(uuid);
}

void NetworkWorker::deactiveConnection(const QString &uuid)
{
    m_networkInter.DeactivateConnection(uuid);
}

void NetworkWorker::disconnectDevice(const QString &devPath)
{
    m_networkInter.DisconnectDevice(QDBusObjectPath(devPath));
}

void NetworkWorker::activateConnection(const QString &devPath, const QString &uuid)
{
    m_networkInter.ActivateConnection(uuid, QDBusObjectPath(devPath));
}

void NetworkWorker::activateAccessPoint(const QString &devPath, const QString &apPath, const QString &uuid)
{
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(m_networkInter.ActivateAccessPoint(uuid, QDBusObjectPath(apPath), QDBusObjectPath(devPath)));

    w->setProperty("devPath", devPath);
    w->setProperty("apPath", apPath);
    w->setProperty("uuid", uuid);

    connect(w, &QDBusPendingCallWatcher::finished, this, &NetworkWorker::activateAccessPointCB);
}

void NetworkWorker::activateAccessPointCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QDBusObjectPath> reply = *w;
    m_networkModel->onActivateAccessPointDone(w->property("devPath").toString(),
            w->property("apPath").toString(), w->property("uuid").toString(), reply.value());

    w->deleteLater();
}

void NetworkWorker::queryAutoProxyCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    m_networkModel->onAutoProxyChanged(reply.value());

    w->deleteLater();
}

void NetworkWorker::queryProxyCB(QDBusPendingCallWatcher *w)
{
    QDBusMessage reply = w->reply();

    const QString &type = w->property("proxyType").toString();
    const QString &addr = reply.arguments()[0].toString();
    const uint port = reply.arguments()[1].toUInt();

    m_networkModel->onProxiesChanged(type, addr, port);

    w->deleteLater();
}

void NetworkWorker::queryProxyMethodCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    m_networkModel->onProxyMethodChanged(reply.value());

    w->deleteLater();
}

void NetworkWorker::queryProxyIgnoreHostsCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    m_networkModel->onProxyIgnoreHostsChanged(reply.value());

    w->deleteLater();
}

void NetworkWorker::queryDeviceStatusCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<bool> reply = *w;
    m_networkModel->onDeviceEnableChanged(w->property("devPath").toString(), reply.value());

    w->deleteLater();
}

void NetworkWorker::queryActiveConnInfoCB(QDBusPendingCallWatcher *w)
{
    QDBusPendingReply<QString> reply = *w;

    m_networkModel->onActiveConnInfoChanged(reply.value());

    w->deleteLater();
}
