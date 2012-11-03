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

#include "rendercontroller.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <assert.h>

#include "renderallocationitem.h"

ControllerScene::ControllerScene(QObject *parent, DFBTracingBufferData *info) : QGraphicsScene(parent)
{
    m_allocation = *info;
    m_allocationItemsHash.clear();

    memset(&m_info, 0, sizeof(m_info));

    m_info.lowestUsage = 0xffffffff;
}

void ControllerScene::setSceneRect(const QRectF &rect)
{
    QGraphicsScene::setSceneRect(rect);

    m_renderAspectRatio = (rect.width() * rect.height()) / m_allocation.poolSize;
}

void ControllerScene::setSceneRect(qreal x, qreal y, qreal w, qreal h)
{
    QGraphicsScene::setSceneRect(x, y, w, h);

    m_renderAspectRatio = (w * h) / m_allocation.poolSize;
}

float ControllerScene::aspectRatio()
{
    return m_renderAspectRatio;
}

void ControllerScene::addItem(RenderAllocationItem *item)
{
    m_allocationItemsHash.insert(item->allocation().offset, item);
    QGraphicsScene::addItem(item);

    m_info.allocated += item->allocation().size;
    m_info.totalSize = item->allocation().poolSize;
    m_info.usageRatio = (m_info.allocated / (float)m_info.totalSize) * 100;
    m_info.peakUsage = (m_info.peakUsage < m_info.allocated) ? m_info.allocated : m_info.peakUsage;
    m_info.lowestUsage = (m_info.lowestUsage > m_info.allocated) ? m_info.allocated : m_info.lowestUsage;

    emit allocationChanged(m_info);
}

void ControllerScene::removeItem(RenderAllocationItem *item)
{
    m_allocationItemsHash.remove(item->allocation().offset);
    QGraphicsScene::removeItem(item);

    m_info.allocated -= item->allocation().size;
    m_info.totalSize = item->allocation().poolSize;
    m_info.usageRatio = (m_info.allocated / (float)m_info.totalSize) * 100;
    m_info.peakUsage = (m_info.peakUsage < m_info.allocated) ? m_info.allocated : m_info.peakUsage;
    m_info.lowestUsage = (m_info.lowestUsage > m_info.allocated) ? m_info.allocated : m_info.lowestUsage;

    emit allocationChanged(m_info);
}

RenderAllocationItem* ControllerScene::lookup(unsigned int offset)
{
    return m_allocationItemsHash.value(offset);
}

const ControllerInfo& ControllerScene::getInfo()
{
    return (m_info);
}

RenderController::RenderController(QString ipAddr, int port, bool saveToFile)
{
    m_ipAddr = ipAddr;
    m_port = port;

    m_currentNseq = m_expectedNseq = 0;

    m_saveToFile = saveToFile;

    m_receiver = 0;

    time_t t = time(NULL);
    localtime_r(&t, &m_startingDate);

    if (m_saveToFile) {
        char buf[64];
        sprintf(buf, "packettrace-%04d%02d%02d%02d%02d", m_startingDate.tm_year,
                                                         m_startingDate.tm_mon,
                                                         m_startingDate.tm_mday,
                                                         m_startingDate.tm_hour,
                                                         m_startingDate.tm_min);

        m_outputTrace.open(buf, ios_base::out | ios_base::app);
    }
}

RenderController::RenderController(QString traceFile, int period) : m_renderingSemaphore(1)
{
    m_ipAddr = "";
    m_port = -1;

    m_saveToFile = false;

    m_receiver = 0;

    m_trace = traceFile;
    m_renderPeriod = period;

    m_currentNseq = m_expectedNseq = 0;
}

RenderController::~RenderController()
{
    if (m_runThread)
        disconnect();
}

void RenderController::saveTraceToFile(bool save)
{
    if (!m_saveToFile && save) {
        char buf[64];
        sprintf(buf, "packettrace-%04d%02d%02d%02d%02d", m_startingDate.tm_year,
                                                         m_startingDate.tm_mon,
                                                         m_startingDate.tm_mday,
                                                         m_startingDate.tm_hour,
                                                         m_startingDate.tm_min);

        m_outputTrace.open(buf, ios_base::out | ios_base::app);
    }

    if (m_saveToFile && !save)
        m_outputTrace.close();

    m_saveToFile = save;
}

bool RenderController::connect()
{
    struct sockaddr_in addrIn;

    if (m_port < 0)
        return false;

    m_udpSocket = socket(AF_INET,SOCK_DGRAM, 0);

    if (m_udpSocket < 0)
        return false;

    memset(&addrIn, 0, sizeof(addrIn));

    addrIn.sin_family = AF_INET;
    addrIn.sin_port = htons(m_port);
    addrIn.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_udpSocket, (const sockaddr*)&addrIn, sizeof(addrIn)) < 0)
    {
        close(m_udpSocket);
        return false;
    }

    m_controllerSceneMap.clear();

    m_controllerStatus = STATUS_IDLE;

    m_runThread = true;

    m_receiver = new ReceiverThread(this);
    m_receiver->start();

    return true;
}

bool RenderController::renderTrace()
{
    m_port = -1;

    m_controllerSceneMap.clear();

    m_controllerStatus = STATUS_IDLE;

    m_traceController = new TraceControllerDialog(this, m_renderPeriod);

    if (!m_traceController)
        return false;

    QObject::connect(m_traceController, SIGNAL(renderPaceChanged(int)), this, SLOT(changeRenderPace(int)));
    QObject::connect(m_traceController, SIGNAL(pausePlayback()), this, SLOT(pauseTraceRendering()));
    QObject::connect(m_traceController, SIGNAL(resumePlayback()), this, SLOT(resumeTraceRendering()));
    QObject::connect(m_traceController, SIGNAL(timeLineTracking(int)), this, SLOT(timeLineTracking(int)));
    QObject::connect(m_traceController, SIGNAL(timeLineReleased(int)), this, SLOT(timeLineReleased(int)));

    m_traceController->show();

    m_trackingTraceOffset = -1;

    m_isPaused = true;
    m_isTracking = false;

    return true;
}

void RenderController::disconnect()
{
    if (m_runThread) {
        m_runThread = false;
        shutdown(m_udpSocket, SHUT_RDWR);

        close(m_udpSocket);
        m_udpSocket = -1;
    }

    if (m_receiver) {
        m_runThread = false;
        m_renderingSemaphore.release();

        m_receiver->wait();

        delete m_receiver;
        m_receiver = 0;
    }

    QList<ControllerScene*> scenes = m_controllerSceneMap.values();
    for (QList<ControllerScene*>::iterator it = scenes.begin(); it != scenes.end(); ++it)
        delete (*it);

    m_controllerSceneMap.clear();

    m_controllerStatus = STATUS_IDLE;

    m_outputTrace.close();

    if (m_traceController) {
        m_traceController->close();

        delete m_traceController;
        m_traceController = 0;
    }
}

RenderController::ReceiverThread::ReceiverThread(RenderController *parent)
{
    m_parent = parent;

    QObject::connect(m_parent, SIGNAL(partialAllocation(CSPEPacketEvent)), m_parent, SLOT(partialAllocationEvent(CSPEPacketEvent)));
    QObject::connect(m_parent, SIGNAL(partialRelease(CSPEPacketEvent)), m_parent, SLOT(partialReleaseEvent(CSPEPacketEvent)));
}

RenderController::ReceiverThread::~ReceiverThread()
{
    QObject::disconnect(m_parent, SIGNAL(partialAllocation(CSPEPacketEvent)));
    QObject::disconnect(m_parent, SIGNAL(partialRelease(CSPEPacketEvent)));
}

void RenderController::ReceiverThread::run()
{
    FILE* trace = NULL;
    int trackingTraceOffset, currentPosition = 0;

    struct sockaddr_in addrIn;
    socklen_t addrLen;
    char buf[2048];

    int s;

    TracePlaybackMode mode = NORMAL;

    // m_port > 0 -> read from the network
    if (m_parent->m_port > 0)
        memset(&addrIn, 0, sizeof(addrIn));
    else {
        trace = fopen(m_parent->m_trace.toStdString().c_str(), "rb");
        if (!trace)
            return;

        fseek(trace, 0, SEEK_END);
        long size = ftell(trace);

        if (size < (long)sizeof(DFBTracingPacket)) {
            fclose(trace);
            return;
        }

        fseek(trace, 0, SEEK_SET);

        trackingTraceOffset = m_parent->m_trackingTraceOffset;

        m_parent->m_traceController->setTimeLineMinMax(0, size / sizeof(DFBTracingPacket));
    }

    while (m_parent->m_runThread)
    {
        if (m_parent->m_port > 0) {
            addrLen = sizeof(addrIn);
            s = recvfrom(m_parent->m_udpSocket, buf, sizeof(buf), 0, (sockaddr*)&addrIn, &addrLen);
        } else
        {
            m_parent->m_renderingSemaphore.acquire();

            if ((mode == NORMAL) && (trackingTraceOffset != m_parent->m_trackingTraceOffset)) {
                trackingTraceOffset = m_parent->m_trackingTraceOffset;
                mode = (currentPosition < trackingTraceOffset) ? FAST_FORWARD : FAST_REWIND;
            }

            if (mode == FAST_REWIND)
                fseek(trace, -sizeof(DFBTracingPacket), SEEK_CUR);

            s = fread(buf, sizeof(DFBTracingPacket), 1, trace);
            s = (s == 1) ? sizeof(DFBTracingPacket) : 0;

            if (mode == FAST_REWIND)
                fseek(trace, -sizeof(DFBTracingPacket), SEEK_CUR);

            currentPosition = ftell(trace);
            currentPosition /= sizeof(DFBTracingPacket);

            if ((mode != NORMAL) && (currentPosition == trackingTraceOffset))
                mode = NORMAL;

            m_parent->m_renderingSemaphore.release();

            if (!s && feof(trace))
                break;

            if (mode == NORMAL) {
                if (!m_parent->m_isTracking)
                    m_parent->m_traceController->setTimeLinePosition(currentPosition);

                usleep(m_parent->m_renderPeriod * 1000);
            }
        }

        if (s <= 0) {
            m_parent->m_controllerStatus = (s == 0) ? STATUS_IDLE : STATUS_ERROR;
            break;
        }

        m_parent->packetReceived(buf, s, mode);
    }

    m_parent->m_controllerStatus = STATUS_IDLE;

    if (m_parent->m_traceController)
        m_parent->m_traceController->stop();

    if (trace)
        fclose(trace);

    emit m_parent->tracePlaybackEnded();
}

void RenderController::changeRenderPace(int value)
{
    m_renderPeriod = value;
}

void RenderController::pauseTraceRendering()
{
    if (m_receiver && !m_isPaused) {
        m_renderingSemaphore.acquire();
        m_isPaused = true;
    }
}

void RenderController::resumeTraceRendering()
{
    if (!m_receiver)
    {
        m_runThread = true;
        m_isPaused = true;

        m_renderingSemaphore.acquire(); // Request the semaphore: we're the initiator

        QObject::connect(this, SIGNAL(tracePlaybackEnded()), this, SLOT(tracePlaybackEndedEvent()));

        m_receiver = new ReceiverThread(this);
        m_receiver->start();
    }

    if (m_isPaused) {
        m_renderingSemaphore.release();
        m_isPaused = false;
    }
}

void RenderController::timeLineTracking(int value)
{
    m_isTracking = true;
}

void RenderController::timeLineReleased(int value)
{
    m_isTracking = false;

    if (!m_isPaused)
        m_renderingSemaphore.acquire();

    m_trackingTraceOffset = value;

    if (!m_isPaused)
        m_renderingSemaphore.release();
}

void RenderController::tracePlaybackEndedEvent()
{
    if (m_receiver)
    {
        m_runThread = false;
        m_renderingSemaphore.release();

        m_receiver->wait();

        delete m_receiver;
        m_receiver = 0;

        QObject::disconnect(this, SIGNAL(tracePlaybackEnded()));
    }
}

void RenderController::renderAllocation(ControllerScene *scene, DFBTracingBufferData* data)
{
    RenderAllocationItem *allocationItem = new RenderAllocationItem(scene, data);

    allocationItem->setPosition();

    scene->addItem(allocationItem);
    scene->update();
}

void RenderController::releaseAllocation(ControllerScene *scene, DFBTracingBufferData* data)
{
    RenderAllocationItem* item = scene->lookup(data->offset);

    if (item) {
        scene->removeItem(item);
        scene->update();
    }
}

void RenderController::processSnapshotEvent(char* buf, int size)
{
    DFBTracingPacket *packet = reinterpret_cast<DFBTracingPacket*>(buf);

    if (packet->Payload.pool.count)
    {
        DFBTracingPoolData* fullMap = &packet->Payload.pool;

        size -= offsetof(DFBTracingPacket, Payload.pool.stats);

        for (unsigned int i = 0; (i < fullMap->count) && ((unsigned int)size >= sizeof(DFBTracingBufferData)); i++)
        {
            if (!m_controllerSceneMap.contains(fullMap->stats[i].poolId))
            {
                ControllerScene *scene = new ControllerScene(this, &fullMap->stats[i]);
                m_controllerSceneMap.insert(fullMap->stats[i].poolId, scene);

                scene->setSceneRect(0, 0, 800, 600);
                scene->setBackgroundBrush(QBrush(QColor(0, 0, 0)));
                scene->setForegroundBrush(QBrush(QColor(0, 0, 0)));

                emit newSurfacePool(scene, fullMap->stats[i].name);
            }

            renderAllocation(m_controllerSceneMap.value(fullMap->stats[i].poolId), &fullMap->stats[i]);
            size -= sizeof(DFBTracingBufferData);
        }

        assert(size == 0);

        if (size)
            emit missingInformation(packet->header.nSeq);

        m_controllerStatus = STATUS_RECEIVING;
    }
}

void RenderController::allocationEvent(DFBTracingPacket packet)
{
    DFBTracingBufferData* data = &packet.Payload.buffer;

    ControllerScene *scene;

    scene = m_controllerSceneMap.value(data->poolId);

    if (!m_controllerSceneMap.contains(data->poolId))
            assert(!scene);

    if (!scene && !m_controllerSceneMap.contains(data->poolId))
    {
        scene = new ControllerScene(this, data);

        m_controllerSceneMap.insert(data->poolId, scene);

        scene->setSceneRect(0, 0, 800, 600);
        scene->setBackgroundBrush(QBrush(QColor(0, 0, 0)));

        emit newSurfacePool(scene, data->name);
    }

    renderAllocation(scene, data);
}

void RenderController::releaseEvent(DFBTracingPacket packet)
{
    DFBTracingBufferData* data = &packet.Payload.buffer;

    ControllerScene *scene = m_controllerSceneMap.value(data->poolId);

    if (scene)
        releaseAllocation(scene, data);
}

void RenderController::processBufferEvent(char* buf)
{
    DFBTracingPacket *packet = reinterpret_cast<DFBTracingPacket*>(buf);

    switch (packet->header.type) {
    case DTE_POOL_BUFFER_ALLOCATION:
        emit bufferAllocation(*packet); // Copy the object before switching the the main thread
        break;
    case DTE_POOL_BUFFER_RELEASE:
        emit bufferRelease(*packet); // Copy the object before switching the the main thread
        break;
    default:
        break;
    }
}

void RenderController::processPacket(char* buf, int size, TracePlaybackMode mode)
{
    DFBTracingPacket *packet = reinterpret_cast<DFBTracingPacket*>(buf);

    m_currentNseq = packet->header.nSeq;
    m_expectedNseq = m_currentNseq + 1;

    if (m_saveToFile)
        m_outputTrace.write(buf, size);

    switch (m_controllerStatus) {
    case STATUS_SYNCING:
        if (packet->header.type != DTE_POOL_FULL_SNAPSHOT)
            return;

        processSnapshotEvent(buf, size);
        break;
    case STATUS_RECEIVING:
        if ((packet->header.type != DTE_POOL_BUFFER_ALLOCATION)
            && (packet->header.type != DTE_POOL_BUFFER_RELEASE))
            return;

        if (mode == FAST_REWIND) // Force a release event if we're rewinding the trace
            packet->header.type = DTE_POOL_BUFFER_RELEASE;

        processBufferEvent(buf);
        break;
    default:
        break;
    }
}

void RenderController::packetReceived(char* buf, int size, TracePlaybackMode mode)
{
    // Inspect the header for the sequence number
    DFBTracingPacket *packet = reinterpret_cast<DFBTracingPacket*>(buf);

    if ((mode != FAST_REWIND) && (packet->header.nSeq != m_expectedNseq)) {
        m_currentNseq = packet->header.nSeq;

        emit lostPackets(m_currentNseq, m_expectedNseq);
        m_expectedNseq = m_currentNseq + 1;

        m_controllerStatus = STATUS_RECEIVING; //STATUS_SYNCING;
    } else {
        if (m_controllerStatus == STATUS_IDLE)
            m_controllerStatus = STATUS_RECEIVING; //STATUS_SYNCING;

        if (size != sizeof(DFBTracingPacket))
            emit missingInformation(packet->header.nSeq);
        else
            processPacket(buf, size, mode);
    }
}
