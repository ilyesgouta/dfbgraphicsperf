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

#ifndef TRACECONTROLLERDIALOG_H
#define TRACECONTROLLERDIALOG_H

#include <QDialog>

#include "allocationrendercontroller.h"

namespace Ui {
    class TraceControllerDialog;
}

class AllocationRenderController;

class TraceControllerDialog : public QDialog
{
    Q_OBJECT

public:
    TraceControllerDialog(AllocationRenderController *controller, int period);
    ~TraceControllerDialog();

    void setTimeLineMinMax(int min, int max);
    void setTimeLinePosition(int value);

    void stop();

signals:
    void renderPaceChanged(int value);
    void pausePlayback();
    void resumePlayback();
    void timeLineTracking(int value);
    void timeLineReleased(int value);

private slots:
    void playPausePressed(bool toggled);
    void renderPace(int value);
    void timeLineSliderMoved(int value);
    void timeLineSliderReleased();

private:
    Ui::TraceControllerDialog *ui;

    AllocationRenderController *m_parent;

    int m_max;
};

#endif // TRACECONTROLLERDIALOG_H
