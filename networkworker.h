
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

#ifndef NETWORKWORKER_H
#define NETWORKWORKER_H

#include "networkmodel.h"

#include <QObject>

#include <com_deepin_daemon_network.h>
#include <com_deepin_daemon_airplanemode.h>
#include <com_deepin_daemon_network_proxychains.h>

namespace dde {

namespace network {

using NetworkInter = com::deepin::daemon::Network;
using ProxyChains = com::deepin::daemon::network::ProxyChains;
using AirplaneMode = com::deepin::daemon::AirplaneMode;

class NetworkWorker : public QObject
{
    Q_OBJECT

public:
    explicit NetworkWorker(NetworkModel *model, QObject *parent = nullptr, bool sync = false);
    /**
     * @def active
     * @brief 初始化数据
     * @param bSync
     */
    void active(bool bSync = false);
    void deactive();
private:
    void initConnect();

public Q_SLOTS:
    void activateConnection(const QString &devPath, const QString &uuid);
    void activateAccessPoint(const QString &devPath, const QString &apPath, const QString &uuid);
    void deleteConnection(const QString &uuid);
    /**
     * @def deactiveConnection
     * @brief 断开连接
     * @param uuid
     */
    void deactiveConnection(const QString &uuid);
    /**
     * @def disconnectDevice
     * @brief 适配器层面断开连接，目前使用这个较多
     * @param devPath
     */
    void disconnectDevice(const QString &devPath);
    /**
     * @def requestWirelessScan
     * @brief 刷新wifi数据，这个操作会更新wirelessAccessPoints属性，会有些时间损耗，
     *  大约得到数据的时间为：5秒左右，数据也不是很全，所以在刷新操作之前，都是基于前端目前有数据的情况下
     */
    void requestWirelessScan();
    void queryChains();
    void queryAutoProxy();
    void queryProxyData();
    void queryProxyMethod();
    void queryProxyIgnoreHosts();
    void queryActiveConnInfo();
    void queryProxy(const QString &type);
    /**
     * @def  queryDeviceStatus
     * @brief 网络详情页使用的，重新获取一下适配器状态
     * @param devPath
     */
    void queryDeviceStatus(const QString &devPath);
    /**
     * @def remanageDevice
     * @brief 热点相关内容
     * @param devPath
     */
    void remanageDevice(const QString &devPath);
    /**
     * @def setVpnEnable
     * @brief 设置热点开关
     * @param enable
     */
    void setVpnEnable(const bool enable);
    /**
     * @def  setDeviceEnable
     * @brief  设置网络适配器开关
     * @param devPath
     * @param enable
     */
    void setDeviceEnable(const QString &devPath, const bool enable);
    /**
     * @def setProxyMethod
     * @brief 设置代理
     * @param proxyMethod
     */
    void setProxyMethod(const QString &proxyMethod);
    void setProxyIgnoreHosts(const QString &hosts);
    void setAutoProxy(const QString &proxy);
    void setProxy(const QString &type, const QString &addr, const QString &port);
    void setChainsProxy(const ProxyConfig &config);
    void onChainsTypeChanged(const QString &type);

private Q_SLOTS:
    /**
     * @def activateAccessPointCB
     * @brief 连接返回给Networkmodel的处理函数 callBack
     * @param w
     */
    void activateAccessPointCB(QDBusPendingCallWatcher *w);
    /**
     * @def queryAutoProxyCB
     * @brief 设置代理的处理函数 callback
     * @param w
     */
    void queryAutoProxyCB(QDBusPendingCallWatcher *w);
    void queryProxyCB(QDBusPendingCallWatcher *w);
    /**
     * @def queryProxyMethodCB
     * @brief  设置代理
     * @param w
     */
    void queryProxyMethodCB(QDBusPendingCallWatcher *w);
    void queryProxyIgnoreHostsCB(QDBusPendingCallWatcher *w);
    void queryDeviceStatusCB(QDBusPendingCallWatcher *w);
    void queryActiveConnInfoCB(QDBusPendingCallWatcher *w);

private:
    NetworkInter m_networkInter;
    /**
     * @brief m_airplaneMode   飞行模式接口
     */
    AirplaneMode m_airplaneMode;
    ProxyChains *m_chainsInter;
    NetworkModel *m_networkModel;
};

}   // namespace network

}   // namespace dde

#endif // NETWORKWORKER_H
