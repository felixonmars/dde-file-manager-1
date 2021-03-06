/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
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

#include "trashmanager.h"
#include "dfileservices.h"
#include "dfilewatcher.h"
#include "dfileproxywatcher.h"
#include "dfileinfo.h"
#include "models/trashfileinfo.h"

#include "app/define.h"
#include "app/filesignalmanager.h"
#include "dfmevent.h"

#include "interfaces/dfmstandardpaths.h"
#include "singleton.h"
#include "shutil/dfmfilelistfile.h"

#include "dfmeventdispatcher.h"

#include <QDebug>
#include <QUrlQuery>
#include <QTimer>
#include <QCoreApplication>
#include <QThread>

#include <fileoperations/filejob.h>
#include "app/define.h"
#include "singleton.h"
#include "dialogs/dialogmanager.h"
#include "dialogs/dtaskdialog.h"

static bool kWorking = false; // tmp
class TrashDirIterator : public DDirIterator
{
public:
    TrashDirIterator(const DUrl &url,
                     const QStringList &nameFilters,
                     QDir::Filters filter,
                     QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags);
    ~TrashDirIterator() override;

    DUrl next() Q_DECL_OVERRIDE;
    bool hasNext() const Q_DECL_OVERRIDE;

    QString fileName() const Q_DECL_OVERRIDE;
    DUrl fileUrl() const Q_DECL_OVERRIDE;
    const DAbstractFileInfoPointer fileInfo() const Q_DECL_OVERRIDE;
    DUrl url() const Q_DECL_OVERRIDE;

    DFMFileListFile *hiddenFiles = nullptr;
private:
    QDirIterator *iterator;
    bool nextIsCached = false;
    QDir::Filters filters;
};

TrashDirIterator::TrashDirIterator(const DUrl &url, const QStringList &nameFilters,
                                   QDir::Filters filter, QDirIterator::IteratorFlags flags)
    : DDirIterator()
    , filters(filter)
{
    QString path = DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath) + url.path();
    iterator = new QDirIterator(path, nameFilters, filter, flags);

    hiddenFiles = new DFMFileListFile(path);
}

TrashDirIterator::~TrashDirIterator()
{
    if (iterator) {
        delete iterator;
    }

    if (hiddenFiles) {
        delete hiddenFiles;
    }
}

DUrl TrashDirIterator::next()
{
    if (nextIsCached) {
        nextIsCached = false;

        QString path = iterator->filePath();
        return DUrl::fromTrashFile(path.remove(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath)));
    }
    return DUrl::fromTrashFile(DUrl::fromLocalFile(iterator->next()).path().remove(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath)));
}

bool TrashDirIterator::hasNext() const
{
    if (nextIsCached) {
        return true;
    }

    bool hasNext = iterator->hasNext();

    if (!hasNext) {
        return false;
    }

    bool showHidden = filters.testFlag(QDir::Hidden);
    DAbstractFileInfoPointer info;

    do {
        const_cast<TrashDirIterator *>(this)->iterator->next();
        QString absoluteFilePath = iterator->fileInfo().absoluteFilePath();
        info = DAbstractFileInfoPointer(new TrashFileInfo(DUrl::fromLocalFile(absoluteFilePath)));

        if (!info->isPrivate() && (showHidden || (!info->isHidden() && !hiddenFiles->contains(info->fileName())))) {
            break;
        }

        info.reset();
    } while (iterator->hasNext());

    // file is exists
    if (info) {
        const_cast<TrashDirIterator *>(this)->nextIsCached = true;

        return true;
    }

    return false;
//    return iterator->hasNext();
}

QString TrashDirIterator::fileName() const
{
    return iterator->fileName();
}

DUrl TrashDirIterator::fileUrl() const
{
    return DUrl::fromTrashFile(iterator->filePath().remove(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath)));
}

const DAbstractFileInfoPointer TrashDirIterator::fileInfo() const
{
    return DFileService::instance()->createFileInfo(Q_NULLPTR, fileUrl());
}

DUrl TrashDirIterator::url() const
{
    return DUrl::fromTrashFile(iterator->path().remove(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath)));
}

TrashManager::TrashManager(QObject *parent)
    : DAbstractFileController(parent)
    , m_trashFileWatcher(new DFileWatcher(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath), this))
{
    m_isTrashEmpty = isEmpty();
    QString trashFilePath = DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath);
    //make sure trash file exists
    if (!QFile::exists(trashFilePath))
        QDir().mkdir(trashFilePath);

    connect(m_trashFileWatcher, &DFileWatcher::fileDeleted, this, &TrashManager::trashFilesChanged);
    connect(m_trashFileWatcher, &DFileWatcher::subfileCreated, this, &TrashManager::trashFilesChanged);
    m_trashFileWatcher->startWatcher();
}

const DAbstractFileInfoPointer TrashManager::createFileInfo(const QSharedPointer<DFMCreateFileInfoEvent> &event) const
{
    return DAbstractFileInfoPointer(new TrashFileInfo(event->url()));
}

bool TrashManager::openFile(const QSharedPointer<DFMOpenFileEvent> &event) const
{
    Q_UNUSED(event)

    qWarning() << "trash open action is disable : " << event->url();
//    return FileServices::instance()->openFile(DUrl::fromLocalFile(TRASHFILEPATH + fileUrl.path()));
    return false;
}

DUrlList TrashManager::moveToTrash(const QSharedPointer<DFMMoveToTrashEvent> &event) const
{
    fileService->deleteFiles(event->sender(), event->urlList(), true, false, true);

    return DUrlList();
}

bool TrashManager::restoreFile(const QSharedPointer<DFMRestoreFromTrashEvent> &event) const
{
    ::kWorking = true;
    DUrlList originUrls;
    DUrlList urlList = event->urlList();
    //如果是全部还原操作 需要先遍历trash根目录下的所有目录
    if (urlList.size() == 1 && DUrl::fromTrashFile("/") == urlList.first()) {
        urlList.clear();
        // fix bug#33763 回收站内的隐藏文件应该被找出来进行还原
        for (const DAbstractFileInfoPointer &info : DFileService::instance()->getChildren(Q_NULLPTR, DUrl::fromTrashFile("/"), QStringList(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden))
            urlList << info->fileUrl();
    }
    bool ok = restoreTrashFile(urlList, &originUrls);

    if (ok && !originUrls.isEmpty()) {
        DFMEventDispatcher::instance()->processEvent<DFMSaveOperatorEvent>(event, dMakeEventPointer<DFMMoveToTrashEvent>(nullptr, originUrls));
    }
    ::kWorking = false;
    return ok;
}

bool TrashManager::writeFilesToClipboard(const QSharedPointer<DFMWriteUrlsToClipboardEvent> &event) const
{
    if (event->action() != DFMGlobal::CopyAction &&
            // 取消对剪切操作的屏蔽
            event->action() != DFMGlobal::CutAction) {
        event->ignore();
        return false;
    }

    DUrlList localList;

    for (const DUrl &url : event->urlList()) {
        const QString &path = url.path();

        localList << DUrl::fromLocalFile(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath) + path);

        // blumia: shouldn't do this since we are just copy file out. It's okay if the file name is a hashed name.
        //         use the restore function provided by right click menu to perform restore behavior, not copy-paste.
//        if(path.lastIndexOf('/') > 0) {
//            localList << DUrl::fromLocalFile(DFMStandardPaths::location(DFMStandardPaths::TrashInfosPath) + path);
//        }
    }

    return fileService->writeFilesToClipboard(event->sender(), event->action(), localList);
}

DUrlList TrashManager::pasteFile(const QSharedPointer<DFMPasteEvent> &event) const
{
    if (event->action() != DFMGlobal::CutAction || event->targetUrl() != DUrl::fromTrashFile("/")) {
        return DUrlList();
    }

    if (event->urlList().isEmpty())
        return DUrlList();

    return fileService->moveToTrash(event->sender(), event->urlList());
}


bool TrashManager::deleteFiles(const QSharedPointer<DFMDeleteEvent> &event) const
{
    DUrlList localList;

    for (const DUrl &url : event->urlList()) {
        if (DUrl::fromTrashFile("/") == url) {
            cleanTrash(event->sender());
            return true;
        }

        const QString &path = url.path();

        localList << DUrl::fromLocalFile(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath) + path);

        if (path.lastIndexOf('/') == 0) {
            localList << DUrl::fromLocalFile(DFMStandardPaths::location(DFMStandardPaths::TrashInfosPath) + path + ".trashinfo");
        }
    }

    fileService->deleteFiles(event->sender(), localList, false, event->silent());

    return true;
}

const DDirIteratorPointer TrashManager::createDirIterator(const QSharedPointer<DFMCreateDiriterator> &event) const
{
    return DDirIteratorPointer(new TrashDirIterator(event->url(), event->nameFilters(), event->filters(), event->flags()));
}

namespace TrashManagerPrivate {
DUrl localToTrash(const DUrl &url)
{
    const QString &localPath = url.toLocalFile();
    const QString &trashPath = DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath);

    if (!localPath.startsWith(trashPath))
        return DUrl();

    DUrl u = DUrl::fromTrashFile(localPath.mid(trashPath.length()));

    if (u.path().isEmpty())
        u.setPath("/");

    return u;
}
QString trashToLocal(const DUrl &url)
{
    return DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath) + url.path();
}
}

DAbstractFileWatcher *TrashManager::createFileWatcher(const QSharedPointer<DFMCreateFileWatcherEvent> &event) const
{
    return new DFileProxyWatcher(event->url(),
                                 new DFileWatcher(TrashManagerPrivate::trashToLocal(event->url())),
                                 TrashManagerPrivate::localToTrash);
}

bool TrashManager::restoreTrashFile(const DUrlList &list, DUrlList *restoreOriginUrls)
{
    bool ok = true;
    if (list.size() == 0)
        return true;

    DUrlList restoreFailedList;
    DUrlList restoreFileOriginUrlList;

    DUrlList urlist;
    QStringList pathlist;
    bool isRestoreAll = false;
    for (const DUrl &url : list) {
//        if (url == DUrl::fromTrashFile("/")) {
//            isRestoreAll = true;
//            urlist << url;
//            break;
//        }

        QString jid = dialogManager->getJobIdByUrl(url);
        if (jid.isEmpty() && !urlist.contains(url)) {
            urlist << url;
            pathlist << url.toLocalFile();
        } else {
            qDebug() << "restore filejob url was existed " << url;
        }
    }

    if (!isRestoreAll && urlist.size() == 0) {
        DTaskDialog *dlg = dialogManager->taskDialog();
        if (dlg && dlg->getTaskListWidget() && dlg->getTaskListWidget()->count() > 0) {
            dlg->raise();
        }
        return true; // job already existed
    }

    struct ScopedPointerCustomDeleter {
        static inline void cleanup(FileJob *job)
        {
            job->jobRemoved();
            dialogManager->removeJob(job->getJobId());
            job->deleteLater();
        }
    };
    QScopedPointer<FileJob, ScopedPointerCustomDeleter> job(new FileJob(FileJob::Restore));
    job->setProperty("pathlist", pathlist);
    job->setManualRemoveJob(true);
//    if (isRestoreAll) {
//        if (urlist.size() != 1)
//            dialogManager->addJob(job.data());
//    }
//    else {
    dialogManager->addJob(job.data());
    //    }

    job->jobPrepared();
    int i = 0;
    int total = urlist.size();
    for (const DUrl &url : urlist) {

//        if (url == DUrl::fromTrashFile("/")) {
//            // restore all
//            DUrlList list;

//            for (const DAbstractFileInfoPointer &info : DFileService::instance()->getChildren(Q_NULLPTR, DUrl::fromTrashFile("/"), QStringList(), QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System))
//                list << info->fileUrl();

//            if (list.isEmpty())
//                return true;

//            return restoreTrashFile(list, restoreOriginUrls);
//        } else {
++i;
job->setRestoreProgress(double(i) / total);
//        }

        //###(zccrs): 必须通过 DAbstractFileInfoPointer 使用
        //            因为对象会被缓存，所以有可能在其它线程中被使用
        //            如果直接定义一个TrashFileInfo对象，就可能存在对象被重复释放
        QExplicitlySharedDataPointer<TrashFileInfo> info(new TrashFileInfo(url));

        bool ret = info->restore(job.data());
        if (!job->getIsApplyToAll()) {
            job->resetCustomChoice(); // if not apply to all we should reset button state
        }

        if (!ret && info->exists()) {
            restoreFailedList << info->fileUrl();
        } else {
            restoreFileOriginUrlList << info->originUrl();
        }

        ok = ok && ret;
    }
    job->setRestoreProgress(0);
    emit job->finished();

    if (!ok && restoreFailedList.count() > 0) {
        emit fileSignalManager->requestShowRestoreFailedDialog(restoreFailedList);
    }

    if (restoreOriginUrls)
        *restoreOriginUrls = restoreFileOriginUrlList;

    return ok;
}

void TrashManager::cleanTrash(const QObject *sender) const
{
    DUrlList list;
    const DUrl &file_url = DUrl::fromLocalFile(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath));
    const DUrl &info_url = DUrl::fromLocalFile(DFMStandardPaths::location(DFMStandardPaths::TrashInfosPath));

    if (QFile::exists(file_url.toLocalFile())) {
        list << file_url;
    }
    bool ret = fileService->deleteFiles(sender, list, false, false, true);

    // 清空回收站意味着回收站所有文件都被删除，因此直接删除info的目录即可
    if (ret) {
        QString infoPaht = info_url.toLocalFile();
        QProcess::execute("rm -r \"" + infoPaht.toUtf8() + "\"");
    }
}

bool TrashManager::isEmpty()
{
    QDir dir(DFMStandardPaths::location(DFMStandardPaths::TrashFilesPath));

    if (!dir.exists())
        return true;

    dir.setFilter(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);

    QDirIterator iterator(dir);

    return !iterator.hasNext();
}

bool TrashManager::isWorking()
{
    return ::kWorking;
}

void TrashManager::trashFilesChanged(const DUrl &url)
{
    Q_UNUSED(url);
    if (m_isTrashEmpty == isEmpty())
        return;

    m_isTrashEmpty = isEmpty();
    emit fileSignalManager->trashStateChanged();
}
