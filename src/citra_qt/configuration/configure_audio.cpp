// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QAudioDeviceInfo>
#include <QtGlobal>
#include "audio_core/cubeb_input.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "citra_qt/configuration/configure_audio.h"
#include "core/settings.h"
#include "ui_configure_audio.h"

ConfigureAudio::ConfigureAudio(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureAudio>()) {
    ui->setupUi(this);

    ui->output_sink_combo_box->clear();
    ui->output_sink_combo_box->addItem("auto");
    for (const auto& sink_detail : AudioCore::g_sink_details) {
        ui->output_sink_combo_box->addItem(sink_detail.id);
    }

    ui->input_device_combo_box->clear();
    ui->input_device_combo_box->addItem(tr("Default"));
    for (const auto& device : AudioCore::ListCubebInputDevices()) {
        ui->input_device_combo_box->addItem(QString::fromStdString(device));
    }

    connect(ui->input_type_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::updateAudioInputDevices);

    connect(ui->volume_slider, &QSlider::valueChanged, [this] {
        ui->volume_indicator->setText(tr("%1 %").arg(ui->volume_slider->sliderPosition()));
    });

    this->setConfiguration();
    connect(ui->output_sink_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::updateAudioOutputDevices);
}

ConfigureAudio::~ConfigureAudio() {}

void ConfigureAudio::setConfiguration() {
    int new_sink_index = 0;
    for (int index = 0; index < ui->output_sink_combo_box->count(); index++) {
        if (ui->output_sink_combo_box->itemText(index).toStdString() == Settings::values.sink_id) {
            new_sink_index = index;
            break;
        }
    }
    ui->output_sink_combo_box->setCurrentIndex(new_sink_index);

    ui->toggle_audio_stretching->setChecked(Settings::values.enable_audio_stretching);

    // The device list cannot be pre-populated (nor listed) until the output sink is known.
    updateAudioOutputDevices(new_sink_index);

    int new_device_index = -1;
    for (int index = 0; index < ui->audio_device_combo_box->count(); index++) {
        if (ui->audio_device_combo_box->itemText(index).toStdString() ==
            Settings::values.audio_device_id) {
            new_device_index = index;
            break;
        }
    }
    ui->audio_device_combo_box->setCurrentIndex(new_device_index);

    ui->input_type_combo_box->setCurrentIndex(Settings::values.mic_input_type);
    ui->input_device_combo_box->setCurrentText(
        QString::fromStdString(Settings::values.mic_input_device));
    updateAudioInputDevices(Settings::values.mic_input_type);

    ui->volume_slider->setValue(Settings::values.volume * ui->volume_slider->maximum());
    ui->volume_indicator->setText(tr("%1 %").arg(ui->volume_slider->sliderPosition()));
}

void ConfigureAudio::applyConfiguration() {
    Settings::values.sink_id =
        ui->output_sink_combo_box->itemText(ui->output_sink_combo_box->currentIndex())
            .toStdString();
    Settings::values.enable_audio_stretching = ui->toggle_audio_stretching->isChecked();
    Settings::values.audio_device_id =
        ui->audio_device_combo_box->itemText(ui->audio_device_combo_box->currentIndex())
            .toStdString();
    Settings::values.volume =
        static_cast<float>(ui->volume_slider->sliderPosition()) / ui->volume_slider->maximum();
    u8 new_input_type = ui->input_type_combo_box->currentIndex();
    if (Settings::values.mic_input_type != new_input_type) {
        // TODO Sync mic settings, stop old mic if sampling, start new mic??
        switch (new_input_type) {
        case 0:
            Frontend::RegisterMic(std::make_shared<Frontend::Mic::NullMic>());
            break;
        case 1:
            Frontend::RegisterMic(std::make_shared<AudioCore::CubebInput>());
            break;
        case 2:
            // TODO actual static input frontend
            Frontend::RegisterMic(std::make_shared<Frontend::Mic::NullMic>());
            break;
        }
    }
    Settings::values.mic_input_type = new_input_type;
    Settings::values.mic_input_device = ui->input_device_combo_box->currentText().toStdString();
}

void ConfigureAudio::updateAudioOutputDevices(int sink_index) {
    ui->audio_device_combo_box->clear();
    ui->audio_device_combo_box->addItem(AudioCore::auto_device_name);

    std::string sink_id = ui->output_sink_combo_box->itemText(sink_index).toStdString();
    std::vector<std::string> device_list = AudioCore::GetSinkDetails(sink_id).list_devices();
    for (const auto& device : device_list) {
        ui->audio_device_combo_box->addItem(device.c_str());
    }
}

void ConfigureAudio::updateAudioInputDevices(int index) {
    // TODO: Don't hardcode this to the index for "Real Device" without making it a constant
    // somewhere
    ui->input_device_combo_box->setEnabled(index == 1);
}

void ConfigureAudio::retranslateUi() {
    ui->retranslateUi(this);
}
