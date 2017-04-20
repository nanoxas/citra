// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QObject>
#include <QThreadPool>
#include <QVBoxLayout>
#include "common/common_paths.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/loader/loader.h"
#include "game_list.h"
#include "game_list_p.h"
#include "ui_settings.h"

GameList::GameList(QWidget* parent) : QWidget{parent} {
    watcher = std::make_unique<QFileSystemWatcher>();
    worker_thread = std::make_unique<QThread>();

    QVBoxLayout* layout = new QVBoxLayout;

    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);

    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

    item_model->insertColumns(0, COLUMN_COUNT);
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, "Name");
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, "File type");
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, "Size");

    connect(tree_view, &QTreeView::activated, this, &GameList::ValidateEntry);
    connect(tree_view, &QTreeView::customContextMenuRequested, this, &GameList::PopupContextMenu);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_view);
    setLayout(layout);
}

GameList::~GameList() {
    worker_thread->requestInterruption();
    worker_thread->wait(1000);
    worker_thread->terminate();
}

void GameList::AddEntry(const QList<QStandardItem*>& entry_items) {
    item_model->invisibleRootItem()->appendRow(entry_items);
}

void GameList::ValidateEntry(const QModelIndex& item) {
    // We don't care about the individual QStandardItem that was selected, but its row.
    int row = item_model->itemFromIndex(item)->row();
    QStandardItem* child_file = item_model->invisibleRootItem()->child(row, COLUMN_NAME);
    QString file_path = child_file->data(GameListItemPath::FullPathRole).toString();

    if (file_path.isEmpty())
        return;
    std::string std_file_path(file_path.toStdString());
    if (!FileUtil::Exists(std_file_path) || FileUtil::IsDirectory(std_file_path))
        return;
    emit GameChosen(file_path);
}

void GameList::DonePopulating() {
    connect(watcher.get(), &QFileSystemWatcher::directoryChanged, this, &GameList::Refresh);
    tree_view->setEnabled(true);
}

void GameList::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = tree_view->indexAt(menu_location);
    if (!item.isValid())
        return;

    int row = item_model->itemFromIndex(item)->row();
    QStandardItem* child_file = item_model->invisibleRootItem()->child(row, COLUMN_NAME);
    u64 program_id = child_file->data(GameListItemPath::ProgramIdRole).toULongLong();

    QMenu context_menu;
    QAction* open_save_location = context_menu.addAction(tr("Open Save Data Location"));
    open_save_location->setEnabled(program_id != 0);
    connect(open_save_location, &QAction::triggered,
            [&]() { emit OpenSaveFolderRequested(program_id); });
    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void GameList::PopulateAsync(const QString& dir_path, bool deep_scan) {
    std::string path = dir_path.toStdString();
    if (!FileUtil::Exists(path) || !FileUtil::IsDirectory(path)) {
        LOG_ERROR(Frontend, "Could not find game list folder at %s", path.c_str());
        return;
    }
    tree_view->setEnabled(false);
    // Delete any rows that might already exist if we're repopulating
    item_model->removeRows(0, item_model->rowCount());

    // Cancel the previous worker if its still running and
    worker_thread->requestInterruption();
    auto worker = new GameListWorker(watcher.get(), path, deep_scan);
    connect(worker, &GameListWorker::EntryReady, this, &GameList::AddEntry, Qt::QueuedConnection);
    connect(worker, &GameListWorker::DoneProcessing, this, &GameList::DonePopulating,
            Qt::QueuedConnection);
    connect(worker, &GameListWorker::DoneProcessing, worker_thread.get(), &QThread::quit);

    connect(worker_thread.get(), &QThread::started, worker, &GameListWorker::UpdateGameList,
            Qt::QueuedConnection);
    connect(worker_thread.get(), &QThread::finished, worker, &GameListWorker::deleteLater);

    worker->moveToThread(worker_thread.get());
    // watcher->moveToThread(worker.get());
    worker_thread->start();
}

void GameList::SaveInterfaceLayout() {
    UISettings::values.gamelist_header_state = tree_view->header()->saveState();
}

void GameList::LoadInterfaceLayout() {
    auto header = tree_view->header();
    if (!header->restoreState(UISettings::values.gamelist_header_state)) {
        // We are using the name column to display icons and titles
        // so make it as large as possible as default.
        header->resizeSection(COLUMN_NAME, header->width());
    }

    item_model->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

const QStringList GameList::supported_file_extensions = {"3ds", "3dsx", "elf", "axf",
                                                         "cci", "cxi",  "app"};

static bool HasSupportedFileExtension(const std::string& file_name) {
    QFileInfo file = QFileInfo(file_name.c_str());
    return GameList::supported_file_extensions.contains(file.suffix(), Qt::CaseInsensitive);
}

void GameList::RefreshGameDirectory() {
    if (!UISettings::values.gamedir.isEmpty()) {
        LOG_INFO(Frontend, "Change detected in the games directory. Reloading game list.");
        PopulateAsync(UISettings::values.gamedir, UISettings::values.gamedir_deepscan);
    }
}

void GameList::Refresh(const QString& path) {
    LOG_INFO(Frontend, "Change detected in the games directory. Printing: %s",
             path.toStdString().c_str());
}

void GameListWorker::AddFstEntriesToGameList(const std::string& dir_path, unsigned int recursion) {
    const auto callback = [this, recursion](unsigned* num_entries_out, const std::string& directory,
                                            const std::string& virtual_name) -> bool {
        std::string physical_name = directory + DIR_SEP + virtual_name;
        if (QThread::currentThread()->isInterruptionRequested()) {
            LOG_DEBUG(Frontend, "Interrupting!");
            return false; // Breaks the callback loop.
        }
        bool isDir = FileUtil::IsDirectory(physical_name);
        if (!isDir && HasSupportedFileExtension(physical_name)) {
            std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(physical_name);
            if (!loader)
                return true;

            std::vector<u8> smdh;
            loader->ReadIcon(smdh);

            u64 program_id = 0;
            loader->ReadProgramId(program_id);

            emit EntryReady({
                new GameListItemPath(QString::fromStdString(physical_name), smdh, program_id),
                new GameListItem(
                    QString::fromStdString(Loader::GetFileTypeString(loader->GetFileType()))),
                new GameListItemSize(FileUtil::GetSize(physical_name)),
            });
        } else if (isDir && recursion > 0) {
            LOG_DEBUG(Frontend, "Watching dir %s", physical_name.c_str());
            watcher->addPath(QString::fromStdString(physical_name));
            AddFstEntriesToGameList(physical_name, recursion - 1);
        }
        return true;
    };

    FileUtil::ForeachDirectoryEntry(nullptr, dir_path, callback);
}

void GameListWorker::UpdateGameList() {
    auto watch_dirs = watcher->directories();
    if (!watch_dirs.isEmpty()) {
        watcher->removePaths(watch_dirs);
    }

    LOG_DEBUG(Frontend, "Watching dir %s", dir_path.c_str());
    watcher->addPath(QString::fromStdString(dir_path));
    AddFstEntriesToGameList(dir_path, deep_scan ? 256 : 0);

    // watcher->moveToThread(QApplication::instance()->thread());
    emit DoneProcessing();
}