#include "screenmanagerwayland.h"
#include "screenobjectwayland.h"
#include "dbus/dbusdisplay.h"
#include "dbus/dbusdock.h"
#include "dbus/dbusmonitor.h"

#include <QGuiApplication>
#include <QScreen>

#define SCREENOBJECT(screen) dynamic_cast<ScreenObjectWayland*>(screen)
ScreenManagerWayland::ScreenManagerWayland(QObject *parent)
    : AbstractScreenManager(parent)
{
    m_display = new DBusDisplay(this);
    init();
}

ScreenManagerWayland::~ScreenManagerWayland()
{
    if (m_display){
        m_display->deleteLater();
        m_display = nullptr;
    }
}

ScreenPointer ScreenManagerWayland::primaryScreen()
{
    QString primaryName = m_display->primary();
    if (primaryName.isEmpty())
        qCritical() << "get primary name failed";

    ScreenPointer ret;
    for ( ScreenPointer sp : m_screens.values()) {
        if (sp->name() == primaryName){
            ret = sp;
            break;
        }
    }
    return ret;
}

QVector<ScreenPointer> ScreenManagerWayland::screens() const
{
    QVector<ScreenPointer> order;
    for (const QDBusObjectPath &path : m_display->monitors()){
        if (m_screens.contains(path.path())){
            ScreenPointer sp = m_screens.value(path.path());
            ScreenObjectWayland *screen = SCREENOBJECT(sp.data());
            if (screen){
                if (screen->enabled())
                    order.append(sp);
            }else
                order.append(sp);
        }
    }
    return order;
}

QVector<ScreenPointer> ScreenManagerWayland::logicScreens() const
{
    QVector<ScreenPointer> order;
    QString primaryName = m_display->primary();
    if (primaryName.isEmpty())
        qCritical() << "get primary name failed";

    //调整主屏幕到第一
    for (const QDBusObjectPath &path : m_display->monitors()){
        if (path.path().isEmpty()){
            qWarning() << "monitor: QDBusObjectPath is empty";
            continue;
        }

        if (m_screens.contains(path.path())){
            ScreenPointer sp = m_screens.value(path.path());
            if (sp == nullptr){
                qCritical() << "get scrreen failed path" << path.path();
                continue;
            }
            if (sp->name() == primaryName){
                order.push_front(sp);
            }
            else{
                ScreenObjectWayland *screen = SCREENOBJECT(sp.data());
                if (screen){
                    if (screen->enabled())
                        order.push_back(sp);
                }else
                    order.push_back(sp);
            }
        }
    }
    return order;
}

ScreenPointer ScreenManagerWayland::screen(const QString &name) const
{
    ScreenPointer ret;
    for (const ScreenPointer &sp : m_screens.values()) {
        if (sp->name() == name){
            ret = sp;
            break;
        }
    }
    return ret;
}

qreal ScreenManagerWayland::devicePixelRatio() const
{
    //dbus获取的缩放不是应用值而是设置值，目前还是使用QT来获取
    return qApp->primaryScreen()->devicePixelRatio();
}

AbstractScreenManager::DisplayMode ScreenManagerWayland::displayMode() const
{
    auto pending = m_display->GetRealDisplayMode();
    pending.waitForFinished();
    if (pending.isError()){
        qWarning() << "Display GetRealDisplayMode Error:" << pending.error().name() << pending.error().message();
        AbstractScreenManager::DisplayMode ret = AbstractScreenManager::DisplayMode(m_display->displayMode());
        return ret;
    }else {
        /*
        DisplayModeMirror: 1
        DisplayModeExtend: 2
        DisplayModeOnlyOne: 3
        DisplayModeUnknow: 4
        */
        int mode = pending.argumentAt(0).toInt();
        qDebug() << "GetRealDisplayMode resulet" << mode;
        if (mode > 0 && mode < 4)
            return (AbstractScreenManager::DisplayMode)mode;
        else
            return AbstractScreenManager::Custom;
    }
}

void ScreenManagerWayland::reset()
{
    if (m_display){
        delete m_display;
        m_display = nullptr;
    }

    m_display = new DBusDisplay(this);
    init();
}

void ScreenManagerWayland::onMonitorChanged()
{
    QStringList monitors;
    //检查新增的屏幕
    for (auto objectPath : m_display->monitors()){
        QString path = objectPath.path();
        if (path.isEmpty())
            continue;

        //新增的
        if (!m_screens.contains(path)){
            ScreenPointer sp(new ScreenObjectWayland(new DBusMonitor(path)));
            m_screens.insert(path,sp);
            connectScreen(sp);
        }
        monitors << path;
    }

    //检查移除的屏幕
    for (const QString &path : m_screens.keys()){
        if (!monitors.contains(path)){
            ScreenPointer sp = m_screens.take(path);
            disconnectScreen(sp);
        }
    }
    emit sigScreenChanged();
}

void ScreenManagerWayland::onDockChanged()
{
#ifdef UNUSED_SMARTDOCK
    auto screen = primaryScreen();
    if (screen == nullptr){
        qCritical() << "primaryScreen() return nullptr!!!";
        return;
    }
    emit sigScreenAvailableGeometryChanged(screen, screen->availableGeometry());
#else
    //新增动态dock区功能，dock区不再只是在主屏幕,随鼠标移动
    emit sigScreenAvailableGeometryChanged(nullptr, QRect());
#endif
}

void ScreenManagerWayland::onScreenGeometryChanged(const QRect &rect)
{
    ScreenObjectWayland *sc = SCREENOBJECT(sender());
    if (sc != nullptr && m_screens.contains(sc->path())) {
        ScreenPointer sp = m_screens.value(sc->path());
        emit sigScreenGeometryChanged(sp, rect);
    }
}

void ScreenManagerWayland::init()
{
    m_screens.clear();

    //先尝试使用Qt信号，若有问题再使用DBUS的信号
    connect(qApp, &QGuiApplication::screenAdded, this, &ScreenManagerWayland::onMonitorChanged);
    connect(m_display, &DBusDisplay::MonitorsChanged, this, &ScreenManagerWayland::onMonitorChanged);
    connect(m_display, &DBusDisplay::PrimaryChanged, this, &AbstractScreenManager::sigScreenChanged);
#ifdef UNUSE_TEMP
    connect(m_display, &DBusDisplay::DisplayModeChanged, this, &AbstractScreenManager::sigDisplayModeChanged);
#else
    //临时方案，
    connect(m_display, &DBusDisplay::DisplayModeChanged, this, [this](){
        m_lastMode = m_display->GetRealDisplayMode();
        qDebug() << "mode changed" << m_lastMode;
        emit sigDisplayModeChanged();
    });

    //临时方案，使用PrimaryRectChanged信号作为拆分/合并信号
    connect(m_display, &DBusDisplay::PrimaryRectChanged, this, [this](){
        int mode = m_display->GetRealDisplayMode();
        qDebug() << "deal merge and split" << mode << m_lastMode;
        if (m_lastMode == mode)
            return;
        m_lastMode = mode;
        emit sigDisplayModeChanged();
    });
#endif

    //dock区处理
    connect(DockInfoIns,&DBusDock::FrontendWindowRectChanged,this, &ScreenManagerWayland::onDockChanged);
    connect(DockInfoIns,&DBusDock::HideModeChanged,this, &ScreenManagerWayland::onDockChanged);
    //connect(DockInfoIns,&DBusDock::PositionChanged,this, &ScreenManagerWayland::onDockChanged);不关心位子改变，有bug#25148，全部由区域改变触发

    //初始化屏幕
    for (auto objectPath : m_display->monitors()){
        const QString path = objectPath.path();
        ScreenPointer sp(new ScreenObjectWayland(new DBusMonitor(path)));
        m_screens.insert(path,sp);
        connectScreen(sp);
    }
}

void ScreenManagerWayland::connectScreen(ScreenPointer sp)
{
    connect(sp.get(),&AbstractScreen::sigGeometryChanged,this,
            &ScreenManagerWayland::onScreenGeometryChanged);
}

void ScreenManagerWayland::disconnectScreen(ScreenPointer sp)
{
    disconnect(sp.get(),&AbstractScreen::sigGeometryChanged,this,
            &ScreenManagerWayland::onScreenGeometryChanged);
}
