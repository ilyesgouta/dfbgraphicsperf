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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QMenuBar>
#include <QMenu>

namespace Ui {
    class MainWindow;
}

#include "rendercontroller.h"
#include "renderallocationitem.h"
#include "rendertarget.h"

class MainWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void closeEvent(QCloseEvent *event);

private slots:
    void connectToServer();
    void stop();
    void exit();
    void about();
    void saveToFile();
    void playbackTrace();

    void newSurfacePool(ControllerScene *scene, char* name);
    void lostPackets(unsigned int lastValidNseq, unsigned int expectedNseq);
    void missingInformation(unsigned int nseq);
    void finished();
    void allocationChanged(const ControllerInfo& info);

    void tabChanged(int i);

private:
    Ui::MainWindow *ui;

    void updateStatus();

    QMenuBar *m_menu;

    QAction *m_connectAction;
    QAction *m_stopAction;
    QAction *m_saveToFileAction;
    QAction *m_playbackTraceAction;

    QMenu *m_fileMenu;
    QMenu *m_traceMenu;
    QMenu *m_helpMenu;

    QString serverIpAddr;
    int serverPort;

    QString m_status;

    ControllerInfo m_info;
    unsigned int m_lostPackets;

    RenderController *m_renderController;
    ControllerScene *m_connectedSender;
};

#endif // MAINWINDOW_H
