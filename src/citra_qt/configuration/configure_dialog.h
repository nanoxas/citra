// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>

namespace Ui {
class ConfigureDialog;
}

enum class ConfigureDialogTab {
    GENERAL,
    WEB,
    DEBUG,
    AUDIO,
    INPUT,
    SYSTEM,
    GRAPHICS,
};

class ConfigureDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureDialog(QWidget* parent, ConfigureDialogTab tab = ConfigureDialogTab::GENERAL);
    ~ConfigureDialog() = default;

    void applyConfiguration();

signals:
    void languageChanged(const QString& locale);

private slots:
    void onLanguageChanged(const QString& locale);

private:
    std::unique_ptr<Ui::ConfigureDialog> ui;
};
