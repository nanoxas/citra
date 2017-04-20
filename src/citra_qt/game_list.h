// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QFileSystemWatcher>
#include <QModelIndex>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QTreeView>
#include <QWidget>

class GameListWorker;

class GameList : public QWidget {
    Q_OBJECT

public:
    enum {
        COLUMN_NAME,
        COLUMN_FILE_TYPE,
        COLUMN_SIZE,
        COLUMN_COUNT, // Number of columns
    };

    explicit GameList(QWidget* parent = nullptr);
    ~GameList() override;

    void PopulateAsync(const QString& dir_path, bool deep_scan);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

    static const QStringList supported_file_extensions;

public slots:
    void RefreshGameDirectory();
    void Refresh(const QString&);

signals:
    void GameChosen(QString game_path);
    void ShouldCancelWorker();
    void OpenSaveFolderRequested(u64 program_id);

private:
    void AddEntry(const QList<QStandardItem*>& entry_items);
    void ValidateEntry(const QModelIndex& item);
    void DonePopulating();

    void PopupContextMenu(const QPoint& menu_location);

    std::unique_ptr<QFileSystemWatcher> watcher;
    std::unique_ptr<QThread> worker_thread;
    QTreeView* tree_view = nullptr;
    QStandardItemModel* item_model = nullptr;
};
