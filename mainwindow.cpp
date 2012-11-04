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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QMessageBox>
#include <QInputDialog>

#include <assert.h>

MainWindow::MainWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MainWindow)
{
    QAction *action;

    ui->setupUi(this);

    setWindowTitle("DFBVideoMemoryViz");

    m_menu = new QMenuBar(this);

    m_fileMenu = m_menu->addMenu("&File");
    m_traceMenu = m_menu->addMenu("&Trace");
    m_helpMenu = m_menu->addMenu("&?");

    m_connectAction = m_fileMenu->addAction("&Connect...");
    connect(m_connectAction, SIGNAL(triggered()), this, SLOT(connectToServer()));

    m_stopAction = m_fileMenu->addAction("&Stop");
    connect(m_stopAction, SIGNAL(triggered()), this, SLOT(stop()));

    m_fileMenu->addSeparator();

    action = m_fileMenu->addAction("&Exit");
    connect(action, SIGNAL(triggered()), this, SLOT(exit()));

    m_saveToFileAction = m_traceMenu->addAction("&Save trace to file");
    m_saveToFileAction->setCheckable(true);
    connect(m_saveToFileAction, SIGNAL(triggered()), this, SLOT(saveToFile()));

    action = m_helpMenu->addAction("&?");
    connect(action, SIGNAL(triggered()), this, SLOT(about()));

    m_connectAction->setEnabled(true);
    m_stopAction->setEnabled(false);

    m_renderController = 0;
}

MainWindow::~MainWindow()
{
    delete ui;

    delete m_fileMenu;
    delete m_helpMenu;
    delete m_menu;
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

    m_renderController = new RenderController(serverIpAddr, serverPort, m_saveToFileAction->isChecked());

    connect(m_renderController, SIGNAL(newSurfacePool(ControllerScene*, char*)), this, SLOT(newSurfacePool(ControllerScene*, char*)));
    connect(m_renderController, SIGNAL(missingInformation(unsigned int)), this, SLOT(missingInformation(unsigned int)));
    connect(m_renderController, SIGNAL(lostPackets(unsigned int, unsigned int)), this, SLOT(lostPackets(unsigned int, unsigned int)));
    connect(m_renderController, SIGNAL(finished()), this, SLOT(finished()));

    connect(this, SIGNAL(statusUpdated()), this, SLOT(statusUpdate()));

    connect(ui->tabWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

    m_connectAction->setEnabled(false);
    m_stopAction->setEnabled(true);

    m_lostPackets = 0;
    m_allocatedMemory = 0;
    m_allocationRatio = 0;
    m_maxAllocated = 0;

    m_connectedSender = 0;

    ui->label->setText("Initializing...");
    m_renderController->connect();
}

void MainWindow::newSurfacePool(ControllerScene* scene, char* name)
{
    char buf[256];

    RenderTarget* renderTarget = new RenderTarget();
    renderTarget->setScene(scene);

    renderTarget->setFixedSize(ui->tabWidget->size().width(), ui->tabWidget->size().height());
    renderTarget->setAlignment(Qt::AlignTop);
    renderTarget->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    if (m_connectedSender)
        assert(disconnect(m_connectedSender, SIGNAL(totalAllocatedChanged(uint, uint)), this, SLOT(totalAllocated(uint, uint))));

    assert(connect(scene, SIGNAL(totalAllocatedChanged(uint, uint)), this, SLOT(totalAllocated(uint, uint)), Qt::UniqueConnection));

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
}

void MainWindow::missingInformation(unsigned int nseq)
{
    printf("missing information at packet #%d\n", nseq);
}

void MainWindow::lostPackets(unsigned int lastValidNseq, unsigned int expectedNseq)
{
    m_lostPackets += lastValidNseq - expectedNseq;

    emit statusUpdated();
}

void MainWindow::totalAllocated(unsigned int size, unsigned int total)
{
    m_allocatedMemory = size;
    m_allocationRatio = (int)(((float)size / total) * 100);

    if (size > m_maxAllocated)
        m_maxAllocated = size;

    emit statusUpdated();
}

void MainWindow::statusUpdate()
{
    char buf[256];
    sprintf(buf, "Lost packets: %d\nAllocated video memory:\n  Currently allocated: %d (ratio: %d%%)\n  Peak usage: %d", m_lostPackets, m_allocatedMemory, m_allocationRatio, m_maxAllocated);
    ui->label->setText(buf);
}

void MainWindow::tabChanged(int i)
{
    if (i < 0)
        return;

    RenderTarget *target = static_cast<RenderTarget*>(ui->tabWidget->widget(i));

    if (target && target->scene())
    {
        if (m_connectedSender)
            assert(disconnect(m_connectedSender, SIGNAL(totalAllocatedChanged(uint,uint)), this, SLOT(totalAllocated(uint, uint))));

        assert(connect(target->scene(), SIGNAL(totalAllocatedChanged(uint, uint)), this, SLOT(totalAllocated(uint, uint)), Qt::UniqueConnection));

        m_connectedSender = static_cast<ControllerScene*>(target->scene());
        m_maxAllocated = 0;
    }
}

void MainWindow::finished()
{
    ui->label->setText("Reception ended.");
}

void MainWindow::about()
{
    QMessageBox::information(this, "DFBVideoMemoryViz", "DirectFB Video Memory Viz, (C) Ilyes Gouta, 2011.");
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
    stop();
}