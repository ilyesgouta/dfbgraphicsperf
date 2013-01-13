// DFbGraphicsPerf
// Copyright (C) 2011, Ilyes Gouta, ilyes.gouta@gmail.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "tracecontrollerdialog.h"
#include "ui_tracecontrollerdialog.h"

TraceControllerDialog::TraceControllerDialog(AllocationRenderController *controller, int period) :
    QDialog(0),
    ui(new Ui::TraceControllerDialog)
{
    m_parent = controller;
    ui->setupUi(this);

    connect(ui->play, SIGNAL(toggled(bool)), this, SLOT(playPausePressed(bool)));
    connect(ui->renderPace, SIGNAL(sliderMoved(int)), this, SLOT(renderPace(int)));

    connect(ui->timelineSlider, SIGNAL(sliderMoved(int)), this, SLOT(timeLineSliderMoved(int)));
    connect(ui->timelineSlider, SIGNAL(sliderReleased()), this, SLOT(timeLineSliderReleased()));

    ui->play->setText("&Play");

    setFixedSize(width(), height());

    ui->renderPace->setValue(period);

    m_max = 0;

    char buf[64];

    sprintf(buf, "Rendering Pace (%d ms)", period);
    ui->label->setText(buf);

    setWindowTitle("Trace Controller");
}

TraceControllerDialog::~TraceControllerDialog()
{
    delete ui;
}

void TraceControllerDialog::playPausePressed(bool toggled)
{
    if (!toggled) {
        emit pausePlayback();
        ui->play->setText("&Play");
    } else {
        emit resumePlayback();
        ui->play->setText("&Pause");
    }
}

void TraceControllerDialog::renderPace(int value)
{
    char buf[64];

    sprintf(buf, "Rendering Pace (%d ms)", value);
    ui->label->setText(buf);

    emit renderPaceChanged(value);
}

void TraceControllerDialog::setTimeLineMinMax(int min, int max)
{
    char buf[64];

    sprintf(buf, "Timeline (%d frames)", max);
    ui->label_2->setText(buf);

    m_max = max;

    ui->timelineSlider->setMinimum(min);
    ui->timelineSlider->setMaximum(max);
}

void TraceControllerDialog::setTimeLinePosition(int value)
{
    char buf[64];

    sprintf(buf, "Timeline (%d/%d frames)", value, m_max);
    ui->label_2->setText(buf);

    ui->timelineSlider->setValue(value);
}

void TraceControllerDialog::stop()
{
    ui->play->setText("&Play");
    ui->play->setChecked(false);

    ui->timelineSlider->setValue(0);

    ui->label_2->setText("Timeline");
}

void TraceControllerDialog::timeLineSliderMoved(int value)
{
    emit timeLineTracking(value);
}

void TraceControllerDialog::timeLineSliderReleased()
{
    int value = ui->timelineSlider->value();
    emit timeLineReleased(value);
}
