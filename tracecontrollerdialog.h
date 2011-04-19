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

#ifndef TRACECONTROLLERDIALOG_H
#define TRACECONTROLLERDIALOG_H

#include <QDialog>

namespace Ui {
    class TraceControllerDialog;
}

class RenderController;

class TraceControllerDialog : public QDialog
{
    Q_OBJECT

public:
    TraceControllerDialog(RenderController *controller, int period);
    ~TraceControllerDialog();

signals:
    void renderPaceChanged(int value);
    void pausePlayback();
    void resumePlayback();

private slots:
    void playPausePressed(bool toggled);
    void renderPace(int value);

private:
    Ui::TraceControllerDialog *ui;

    RenderController *m_parent;
};

#endif // TRACECONTROLLERDIALOG_H
