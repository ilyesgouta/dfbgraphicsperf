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

#include <QMainWindow>

#include <QMenuBar>
#include <QMenu>
#include <QVBoxLayout>

namespace Ui {
class MainWindow;
}

#include "scenecontroller.h"
#include "allocationrendercontroller.h"

class MainWindow : public QMainWindow
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

    void newRenderTarget(SceneController *scene, char* name);
    void lostPackets(unsigned int lastValidNseq, unsigned int expectedNseq);
    void missingInformation(unsigned int nseq);
    void finished();
    void statusChanged();

    void tabChanged(int i);

private:
    Ui::MainWindow *ui;

    void updateStatus();

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

    unsigned int m_lostPackets;

    AllocationRenderController *m_renderController;
    SceneController *m_connectedSender;

    QVBoxLayout *m_vboxLayout;
};

#endif // MAINWINDOW2_H
