/*
 * Copyright (C) 2011 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     listenerri <listenerri@gmail.com>
 *
 * Maintainer: listenerri <listenerri@gmail.com>
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

#include "connectivitychecker.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QScopedPointer>
#include <QGSettings>
#include <QTimer>

static const QStringList CheckUrls {
    "https://www.baidu.com",
    "https://www.bing.com",
    "https://www.google.com",
    "https://www.amazon.com",
    "https://github.com",
};

using namespace dde::network;

ConnectivityChecker::ConnectivityChecker(QObject *parent)
    : QObject(parent)
    , m_updateTimer(new QTimer(this))
{
    //五分钟刷新一次当前网络联通状态
    m_updateTimer->setInterval(1000 * 5 * 60);
    m_updateTimer->setSingleShot(false);
    connect(m_updateTimer, &QTimer::timeout, this, &ConnectivityChecker::startCheck);
    m_updateTimer->start();

    m_settings = new QGSettings("com.deepin.dde.network-utils","/com/deepin/dde/network-utils/");
    m_checkUrls = m_settings->get("network-checker-urls").toStringList();
    connect(m_settings,&QGSettings::changed,[=](const QString key) {
        if (key == "network-checker-urls") {
            m_checkUrls = m_settings->get("network-checker-urls").toStringList();
        }
    });
}

void ConnectivityChecker::startCheck()
{
    QNetworkAccessManager nam;

    if (m_checkUrls.isEmpty()) {
        m_checkUrls = CheckUrls;
    }

    for (auto url : m_checkUrls) {
        QScopedPointer<QNetworkReply> reply(nam.head(QNetworkRequest(QUrl(url))));
        qDebug() << "Check connectivity using url:" << url;

        // Do not use waitForReadyRead to block thread,
        // the QNetworkReply is not implement this virtual method
        // and it will just return false immediately
//        reply->waitForReadyRead(-1);

        // Blocking, about 30 second to timeout
        QEventLoop synchronous;
        connect(&nam, &QNetworkAccessManager::finished, &synchronous, &QEventLoop::quit);
        synchronous.exec();

        reply->close();
        if (reply->error() == QNetworkReply::NoError &&
           (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 204)) {
            qDebug() << "Connected to url:" << url;
            Q_EMIT checkFinished(true);
            return;
        }

        qDebug() << "Failed to connect url:" << url;
    }

    Q_EMIT checkFinished(false);
}
