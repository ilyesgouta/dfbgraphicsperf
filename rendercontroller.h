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

#ifndef RENDERCONTROLLER_H
#define RENDERCONTROLLER_H

#include <QGraphicsScene>

#include <QMap>
#include <QHash>

#include <QThreadPool>
#include <QRunnable>

#include <time.h>
#include <fstream>

#include "renderallocationitem.h"

#include <core/cspe_packet.h>

using namespace std;

class ControllerScene : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit ControllerScene(QObject *parent, CSPEAllocationInfo* info);

    void setSceneRect(const QRectF &rect);
    void setSceneRect(qreal x, qreal y, qreal w, qreal h);

    float aspectRatio();

    void addItem(RenderAllocationItem *item);
    void removeItem(RenderAllocationItem *item);

    RenderAllocationItem* lookup(unsigned int offset);

signals:
    void totalAllocatedChanged(unsigned int size, unsigned int total);

private:
    CSPEAllocationInfo m_allocationInfo;

    QHash<unsigned int, RenderAllocationItem *> m_allocationItemsHash;

    float m_renderAspectRatio;

    unsigned int m_allocatedMemory;
};

class RenderController : public QObject
{
    Q_OBJECT
public:
    explicit RenderController(QString ipAddr, int port, bool saveToFile);
    ~RenderController();

    bool connect();
    void disconnect();

    void saveTraceToFile(bool save);

signals:
    void newSurfacePool(ControllerScene* scene, char* name);
    void lostPackets(unsigned int lastValidNseq, unsigned int expectedNseq);
    void missingInformation(unsigned int nseq);
    void finished();
    void packetReceived(char* buf, int size);

private slots:
    void packetReceivedSink(char* buf, int size);

private:
    typedef enum {
        STATUS_IDLE,
        STATUS_RECEIVING,
        STATUS_SYNCING,
        STATUS_ERROR
    } ControllerStatus;

    class ReceiverThread : public QThread {
    public:
        ReceiverThread(RenderController* parent);

        void run();

    private:
        RenderController *m_parent;
    };

    void processPacket(char* buf, int size);
    void processFullCapture(char* buf, int size);
    void processPartialPacket(char* buf, int size);

    void RenderAllocation(ControllerScene *scene, CSPEAllocationInfo* info);
    void ReleaseAllocation(ControllerScene *scene, CSPEAllocationInfo* info);

    QMap<unsigned int, ControllerScene *> m_controllerSceneMap;

    QString m_ipAddr;
    int m_port;
    int m_udpSocket;

    bool m_runThread;
    ReceiverThread *m_receiver;

    bool m_saveToFile;
    ofstream m_outputTrace;
    struct tm m_startingDate;

    unsigned int m_currentNseq, m_expectedNseq;

    ControllerStatus m_controllerStatus;
};

#endif // RENDERCONTROLLER_H
