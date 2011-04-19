// DFBVideoMemoryViz
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

#include "rendercontroller.h"

TraceControllerDialog::TraceControllerDialog(RenderController *controller, int period) :
    QDialog(0),
    ui(new Ui::TraceControllerDialog)
{
    m_parent = controller;
    ui->setupUi(this);

    connect(ui->play, SIGNAL(toggled(bool)), this, SLOT(playPausePressed(bool)));
    connect(ui->renderPace, SIGNAL(sliderMoved(int)), this, SLOT(renderPace(int)));

    ui->play->setText("&Pause");

    setFixedSize(width(), height());

    ui->renderPace->setValue(period);

    setWindowTitle("Trace Controller");
}

TraceControllerDialog::~TraceControllerDialog()
{
    delete ui;
}

void TraceControllerDialog::playPausePressed(bool toggled)
{
    if (toggled) {
        emit pausePlayback();
        ui->play->setText("&Play");
    } else {
        emit resumePlayback();
        ui->play->setText("&Pause");
    }
}

void TraceControllerDialog::renderPace(int value)
{
    emit renderPaceChanged(value);
}
