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

#include "wirelessdevice.h"

using namespace dde::network;

#include <QSet>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QTimer>

#define WIRELESS_PATH  "Path"
#define WIRELESS_STRENGTH  "Strength"
#define WIRELESS_SSID "Ssid"
#define WIRELESS_UUID "Uuid"

WirelessDevice::WirelessDevice(const QJsonObject &info, QObject *parent)
    : NetworkDevice(NetworkDevice::Wireless, info, parent)
{
}

bool WirelessDevice::supportHotspot() const
{
    return info()["SupportHotspot"].toBool();
}

const QString WirelessDevice::activeHotspotUuid() const
{
    Q_ASSERT(hotspotEnabled());

    return m_activeHotspotInfo.value("ConnectionUuid").toString();
}

const QList<QJsonObject> WirelessDevice::activeConnections() const
{
    return m_activeConnections;
}

const QList<QJsonObject> WirelessDevice::activeConnectionsInfo() const
{
    return m_activeConnectionsInfo;
}

const QList<QJsonObject> WirelessDevice::activeVpnConnectionsInfo() const
{
    QList<QJsonObject> activeVpns;
    for (const QJsonObject &activeConn : m_activeConnectionsInfo) {
        if (activeConn.value("ConnectionType").toString().startsWith("vpn-")) {
            activeVpns.append(activeConn);
        }
    }

    return activeVpns;
}

const QJsonObject WirelessDevice::activeWirelessConnectionInfo() const
{
    QJsonObject activeWireless;
    for (const QJsonObject &activeConn : m_activeConnectionsInfo) {
        if (activeConn.value("ConnectionType").toString() == "wireless") {
            activeWireless = activeConn;
            break;
        }
    }

    return activeWireless;
}

const QString WirelessDevice::activeWirelessConnName() const
{
    const QJsonObject &conn = activeWirelessConnectionInfo();
    return conn.isEmpty() ? QString() : conn.value("ConnectionName").toString();
}

const QString WirelessDevice::activeWirelessConnUuid() const
{
    const QJsonObject &conn = activeWirelessConnectionInfo();
    return conn.isEmpty() ? QString() : conn.value("ConnectionUuid").toString();
}

const QString WirelessDevice::activeWirelessConnSettingPath() const
{
    const QJsonObject &conn = activeWirelessConnectionInfo();
    return conn.isEmpty() ? QString() : conn.value("SettingPath").toString();
}

const QJsonArray WirelessDevice::apList() const
{
    QJsonArray apArray;
    for (auto ap : m_apDatas.values()) {
        apArray.append(QJsonDocument::fromJson(ap.toUtf8()).object());
    }
    return apArray;
}

void WirelessDevice::initWirelessData()
{
    //初始化连接列表
    m_apsMap.clear();
    for (QString apInfo : m_apDatas) {
        this->updateAPInfo(apInfo);
    }
    //初始化活动状态
    Q_EMIT activeConnectionsChanged(m_activeApInfo);
}

void WirelessDevice::updateAPInfo(const QString &apInfo)
{
    const auto &ap = QJsonDocument::fromJson(apInfo.toUtf8()).object();
    const auto &ssid = ap.value(WIRELESS_SSID).toString();
    if (!ssid.isEmpty()) {
        //更新连接成功的数据
        if (ssid == activeApSsid() && !activeApSsid().isEmpty() &&
                ap.value(WIRELESS_STRENGTH).toInt() != activeApStrength() ) {
            m_activeApInfo[WIRELESS_STRENGTH] = ap.value(WIRELESS_STRENGTH).toInt();
            if (m_activeApInfo[WIRELESS_UUID] != ap.value(WIRELESS_UUID).toString())
                m_activeApInfo[WIRELESS_UUID] = ap.value(WIRELESS_UUID).toString();
            if(m_activeApInfo[WIRELESS_PATH] != ap.value(WIRELESS_PATH).toString())
                m_activeApInfo[WIRELESS_PATH] = ap.value(WIRELESS_PATH).toString();
        }

        if (m_apsMap.contains(ssid)) {
            Q_EMIT apInfoChanged(ap);
        } else {
            Q_EMIT apAdded(ap);
        }
        // QMap will replace existing key-value
        m_apsMap.insert(ssid, ap);
    }
}

void WirelessDevice::deleteAP(const QString &apInfo)
{
    const auto &ap = QJsonDocument::fromJson(apInfo.toUtf8()).object();
    const auto &Ssid = ap.value(WIRELESS_SSID).toString();

    if (!Ssid.isEmpty()) {
        if (m_apsMap.contains(Ssid)) {
            m_apsMap.remove(Ssid);
            m_apDatas.remove(Ssid);
            Q_EMIT apRemoved(ap);
        }
    }
}

void WirelessDevice::setActiveConnections(const QList<QJsonObject> &activeConns)
{
    m_activeConnections = activeConns;
    //这里数据简单处理一下，里面的值在单网卡的情况下，只有一个，所以不会对内存产生负担
    for (auto activeConn: activeConns) {
        //初始化的时候会使用到的参数。会去尝试获取当前的状态
        m_activeApInfo = activeConn;
        Q_EMIT activeConnectionsChanged(activeConn);
    }
    if (activeConns.isEmpty()) {
        m_activeApInfo = QJsonObject();
        Q_EMIT activeConnectionsChanged(QJsonObject());
    }
}

void WirelessDevice::setActiveConnectionsInfo(const QList<QJsonObject> &activeConnsInfo)
{
    m_activeConnectionsInfo = activeConnsInfo;
}

void WirelessDevice::setActiveHotspotInfo(const QJsonObject &hotspotInfo)
{
    const bool changed = m_activeHotspotInfo.isEmpty() != hotspotInfo.isEmpty();

    m_activeHotspotInfo = hotspotInfo;

    if (changed)
        Q_EMIT hotspotEnabledChanged(hotspotEnabled());
}

void WirelessDevice::WirelessUpdate(const QJsonValue &WirelessList)
{
    //临时变量保存当前全部无限网络状态使用
    QMap<QString, QJsonObject> ssidDatas;
    QJsonArray WirelessDatas = WirelessList.toArray();
    for (QJsonValue data : WirelessDatas) {
        //数据为空则进行下一个循环
        if (data.isNull()) continue;
        //数据转换
        QJsonObject apInfo = data.toObject();
        //当不存在两个Key的时候,则进行下一个循环
        if (!apInfo.contains(WIRELESS_SSID) && !apInfo.contains(WIRELESS_PATH)) continue;
        //当名字为空，则排除掉
        if (apInfo.value(WIRELESS_SSID).toString().isEmpty()) continue;
        QString ssid = apInfo.value(WIRELESS_SSID).toString("");
        int oldStrength = (ssidDatas[ssid]).value(WIRELESS_STRENGTH).toInt(0);
        int newStrength = apInfo.value(WIRELESS_STRENGTH).toInt(0);
        if (newStrength > oldStrength) {
            //修改了的会直接更新,没有的会直接插入
            ssidDatas.insert(ssid, apInfo);
        }
    }

    //由于改变和增加,都是调用updateAPInfo，所以可以直接将改变了的全部直接发送出去
    for (QString ssidKey : ssidDatas.keys()) {
        QString strApInfo = QString(QJsonDocument(ssidDatas.value(ssidKey)).toJson());
        //这一步会根据当前有的做修改，没有的会加上,如果有的则会更新
        m_apDatas.insert(ssidKey, strApInfo);
        this->updateAPInfo(strApInfo);
    }
    for (QString PathKey : m_apDatas.keys()) {
        if (!ssidDatas.contains(PathKey)) {
            this->deleteAP(m_apDatas.value(PathKey));

        }
    }
    ssidDatas.clear();
}
