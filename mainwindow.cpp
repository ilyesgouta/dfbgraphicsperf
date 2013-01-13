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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>

#include <assert.h>

#include "allocationrendercontroller.h"
#include "rendertarget.h"

#define UNUSED_PARAM(a) (a) = (a)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("DFbGraphicsPerf");

    m_vboxLayout = new QVBoxLayout();

    m_vboxLayout->addWidget(ui->tabWidget);
    m_vboxLayout->addWidget(ui->label);

    ui->centralwidget->setLayout(m_vboxLayout);

    m_fileMenu = ui->menubar->addMenu("&File");
    m_traceMenu = ui->menubar->addMenu("&Trace");
    m_helpMenu = ui->menubar->addMenu("&?");

    m_connectAction = m_fileMenu->addAction("&Connect...");
    connect(m_connectAction, SIGNAL(triggered()), this, SLOT(connectToServer()));

    m_stopAction = m_fileMenu->addAction("&Stop");
    connect(m_stopAction, SIGNAL(triggered()), this, SLOT(stop()));

    m_fileMenu->addSeparator();

    m_playbackTraceAction = m_fileMenu->addAction("&Playback a trace...");
    connect(m_playbackTraceAction, SIGNAL(triggered()), this, SLOT(playbackTrace()));

    m_fileMenu->addSeparator();

    QAction *action = m_fileMenu->addAction("&Exit");
    connect(action, SIGNAL(triggered()), this, SLOT(exit()));

    m_saveToFileAction = m_traceMenu->addAction("&Save trace to file");
    m_saveToFileAction->setCheckable(true);
    connect(m_saveToFileAction, SIGNAL(triggered()), this, SLOT(saveToFile()));

    action = m_helpMenu->addAction("&?");
    connect(action, SIGNAL(triggered()), this, SLOT(about()));

    m_connectAction->setEnabled(true);
    m_stopAction->setEnabled(false);
    m_playbackTraceAction->setEnabled(true);

    m_renderController = 0;

    memset(&m_info, 0, sizeof(m_info));

    qRegisterMetaType<DFBTracingEventType>("CSPEPacketEvent");
}

MainWindow::~MainWindow()
{
    delete ui;

    delete m_fileMenu;
    delete m_helpMenu;
}

void MainWindow::connectToServer()
{
    if (m_renderController)
        return;

    bool ok;
    QInputDialog *input = new QInputDialog(this);

    QString result = input->getText(this, "Local port",
                                          "Please enter the local UDP port",
                                          QLineEdit::Normal,
                                          "127.0.0.1:5000", &ok);

    delete input;

    if (!ok || result.isEmpty())
        return;

    serverIpAddr = result.section(':', 0, 0);
    serverPort = result.section(':', 1, 1).toInt(&ok);

    if (!ok)
        serverPort = 5555;

    QStringList list = serverIpAddr.split(".");
    if (list.length() != 4)
        return;

    m_renderController = new AllocationRenderController(serverIpAddr, serverPort, m_saveToFileAction->isChecked());

    connect(m_renderController, SIGNAL(newSurfacePool(ControllerScene*, char*)), this, SLOT(newSurfacePool(ControllerScene*, char*)));
    connect(m_renderController, SIGNAL(missingInformation(unsigned int)), this, SLOT(missingInformation(unsigned int)));
    connect(m_renderController, SIGNAL(lostPackets(unsigned int, unsigned int)), this, SLOT(lostPackets(unsigned int, unsigned int)));
    connect(m_renderController, SIGNAL(finished()), this, SLOT(finished()));

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

    m_connectAction->setEnabled(false);
    m_stopAction->setEnabled(true);
    m_playbackTraceAction->setEnabled(false);

    m_lostPackets = 0;
    m_connectedSender = 0;

    ui->label->setText("Initializing...");
    m_renderController->connect();
}

void MainWindow::playbackTrace()
{
    if (m_renderController)
        return;

    QString traceName = QFileDialog::getOpenFileName(this, "Select a trace:");

    if (!traceName.length())
        return;

    m_renderController = new AllocationRenderController(traceName, 240);

    connect(m_renderController, SIGNAL(newRenderTarget(ControllerScene*, char*)), this, SLOT(newRenderTarget(ControllerScene*, char*)));
    connect(m_renderController, SIGNAL(missingInformation(unsigned int)), this, SLOT(missingInformation(unsigned int)));
    connect(m_renderController, SIGNAL(lostPackets(unsigned int, unsigned int)), this, SLOT(lostPackets(unsigned int, unsigned int)));
    connect(m_renderController, SIGNAL(finished()), this, SLOT(finished()));

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

    m_connectAction->setEnabled(false);
    m_stopAction->setEnabled(true);
    m_playbackTraceAction->setEnabled(false);

    m_lostPackets = 0;
    m_connectedSender = 0;

    ui->label->setText("Initializing...");
    m_renderController->renderTrace();
}

void MainWindow::newRenderTarget(ControllerScene* scene, char* name)
{
    char buf[256];
    bool ret;

    RenderTarget* renderTarget = new RenderTarget();
    renderTarget->setScene(scene);

    scene->setSceneRect(0, 0, ui->tabWidget->size().width(), ui->tabWidget->size().height());
    scene->setBackgroundBrush(QBrush(QColor(0, 0, 0)));

    renderTarget->setFixedSize(ui->tabWidget->size().width(), ui->tabWidget->size().height());
    renderTarget->setAlignment(Qt::AlignTop);
    renderTarget->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    if (m_connectedSender) {
        ret = disconnect(m_connectedSender, SIGNAL(statusChanged(const ControllerInfo&)), this, SLOT(statusChanged(const ControllerInfo&)));
        assert(ret == true);
    }

    ret = connect(scene, SIGNAL(statusChanged(const ControllerInfo&)), this, SLOT(statusChanged(const ControllerInfo&)), Qt::UniqueConnection);
    assert(ret == true);

    m_connectedSender = scene;

    int idx = ui->tabWidget->addTab(renderTarget, name);
    ui->tabWidget->setCurrentIndex(idx);

    sprintf(buf, "New surface pool: %s", name);
    ui->label->setText(buf);
}

void MainWindow::stop()
{
    if (m_renderController)
    {
        m_renderController->disconnect();

        int count = ui->tabWidget->count();
        for (int i = 0; i < count; i++) {
            RenderTarget *target = static_cast<RenderTarget*>(ui->tabWidget->widget(0));
            ui->tabWidget->removeTab(0);
            delete target;
        }

        delete m_renderController;
        m_renderController = 0;
    }

    m_connectAction->setEnabled(true);
    m_stopAction->setEnabled(false);
    m_playbackTraceAction->setEnabled(true);
}

void MainWindow::missingInformation(unsigned int nseq)
{
    printf("missing information at packet #%d\n", nseq);
}

void MainWindow::lostPackets(unsigned int lastValidNseq, unsigned int expectedNseq)
{
    m_lostPackets += lastValidNseq - expectedNseq;

    updateStatus();
}

void MainWindow::statusChanged(const ControllerInfo& info)
{
    m_info = info;

    updateStatus();
}

void MainWindow::updateStatus()
{
    char buf[256];

    sprintf(buf, "Allocated video memory:\n"
                 "  Currently allocated: %d (ratio: %.2f%%)\n"
                 "  Peak usage: %d, Lowest usage: %d\n"
                 "Lost packets: %d\n",
                 m_info.allocated, m_info.usageRatio, m_info.peakUsage, m_info.lowestUsage, m_lostPackets);

    ui->label->setText(buf);
}

void MainWindow::tabChanged(int i)
{
    bool ret;

    if (i < 0)
        return;

    RenderTarget *target = static_cast<RenderTarget*>(ui->tabWidget->widget(i));

    if (target && target->scene())
    {
        if (m_connectedSender) {
            ret = disconnect(m_connectedSender, SIGNAL(statusChanged(const ControllerInfo&)), this, SLOT(statusChanged(const ControllerInfo&)));
            assert(ret == true);
        }

        m_info = static_cast<ControllerScene*>(target->scene())->getInfo();
        updateStatus();

        ret = connect(target->scene(), SIGNAL(statusChanged(const ControllerInfo&)), this, SLOT(statusChanged(const ControllerInfo&)), Qt::UniqueConnection);
        assert(ret == true);

        m_connectedSender = static_cast<ControllerScene*>(target->scene());
    }
}

void MainWindow::finished()
{
    ui->label->setText("Reception ended.");
}

void MainWindow::about()
{
    QMessageBox::information(this, "DFbGraphicsPerf", "DFbGraphicsPerf, (C) Ilyes Gouta (ilyes.gouta@gmail.com), 2011-2012.\n"
                                                        "Released under the GNU General Public License v3.");
}

void MainWindow::exit()
{
    close();
}

void MainWindow::saveToFile()
{
    if (m_renderController)
        m_renderController->saveTraceToFile(m_saveToFileAction->isChecked());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    UNUSED_PARAM(event);
    stop();
}
