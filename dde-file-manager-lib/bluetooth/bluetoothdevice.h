/*
 * Copyright (C) 2019 ~ 2019 Deepin Technology Co., Ltd.
 *
 * Author:     zhangsheng <zhangsheng@uniontech.com>
 *
 * Maintainer: zhangsheng <zhangsheng@uniontech.com>
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
#ifndef BLUETOOTHDEVICE_H
#define BLUETOOTHDEVICE_H

#include <QObject>

class BluetoothDevice : public QObject
{
    Q_OBJECT

public:
    enum State {
        StateUnavailable = 0,
        StateAvailable   = 1,
        StateConnected   = 2
    };
    Q_ENUM(State)

    explicit BluetoothDevice(QObject *parent = nullptr);

    inline QString id() const { return m_id; }
    void setId(const QString &id);

    inline QString name() const { return m_name; }
    void setName(const QString &name);

    inline QString alias() const { return m_alias; }
    void setAlias(const QString &alias);

    inline QString icon() const { return m_icon; }
    void setIcon(const QString &icon);

    inline bool paired() const { return m_paired; }
    void setPaired(bool paired);

    inline bool trusted() const { return m_trusted; }
    void setTrusted(bool trusted);

    inline State state() const { return m_state; }
    void setState(const State &state);


private:
    QString m_id;
    QString m_name;
    QString m_alias;
    QString m_icon;
    bool m_paired;
    bool m_trusted;
    State m_state;
};

#endif // BLUETOOTHDEVICE_H