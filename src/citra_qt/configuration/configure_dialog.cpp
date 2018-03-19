// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configuration/configure_dialog.h"
#include "core/settings.h"
#include "ui_configure_dialog.h"

ConfigureDialog::ConfigureDialog(QWidget* parent, ConfigureDialogTab tab)
    : QDialog(parent), ui(new Ui::ConfigureDialog) {
    ui->setupUi(this);

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &ConfigureDialog::applyConfiguration);
    connect(ui->tab_list, &QListWidget::currentItemChanged, this, &ConfigureDialog::UpdateTab);
}

void ConfigureDialog::applyConfiguration() {
    ui->general_tab->applyConfiguration();
    ui->system_tab->applyConfiguration();
    ui->input_tab->applyConfiguration();
    ui->graphics_tab->applyConfiguration();
    ui->audio_tab->applyConfiguration();
    ui->debug_tab->applyConfiguration();
    ui->web_tab->applyConfiguration();
    Settings::Apply();
}

void ConfigureDialog::onLanguageChanged(const QString& locale) {
    emit languageChanged(locale);
    ui->retranslateUi(this);
    ui->general_tab->retranslateUi();
    ui->system_tab->retranslateUi();
    ui->input_tab->retranslateUi();
    ui->graphics_tab->retranslateUi();
    ui->audio_tab->retranslateUi();
    ui->debug_tab->retranslateUi();
    ui->web_tab->retranslateUi();
}
