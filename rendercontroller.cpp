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

#include "rendercontroller.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <assert.h>

#include "renderallocationitem.h"

ControllerScene::ControllerScene(QObject *parent, CSPEAllocationInfo* info) : QGraphicsScene(parent)
{
    m_allocationInfo = *info;
    m_allocationItemsHash.clear();

    memset(&m_info, 0, sizeof(m_info));

    m_info.lowestUsage = 0xffffffff;
}

void ControllerScene::setSceneRect(const QRectF &rect)
{
    QGraphicsScene::setSceneRect(rect);

    m_renderAspectRatio = (rect.width() * rect.height()) / m_allocationInfo.pool_size;
}

void ControllerScene::setSceneRect(qreal x, qreal y, qreal w, qreal h)
{
    QGraphicsScene::setSceneRect(x, y, w, h);

    m_renderAspectRatio = (w * h) / m_allocationInfo.pool_size;
}

float ControllerScene::aspectRatio()
{
    return m_renderAspectRatio;
}

void ControllerScene::addItem(RenderAllocationItem *item)
{
    m_allocationItemsHash.insert(item->allocationInfo().offset, item);
    QGraphicsScene::addItem(item);

    m_info.allocated += item->allocationInfo().size;
    m_info.totalSize = item->allocationInfo().pool_size;
    m_info.usageRatio = (m_info.allocated / (float)m_info.totalSize) * 100;
    m_info.peakUsage = (m_info.peakUsage < m_info.allocated) ? m_info.allocated : m_info.peakUsage;
    m_info.lowestUsage = (m_info.lowestUsage > m_info.allocated) ? m_info.allocated : m_info.lowestUsage;

    emit allocationChanged(m_info);
}

void ControllerScene::removeItem(RenderAllocationItem *item)
{
    m_allocationItemsHash.remove(item->allocationInfo().offset);
    QGraphicsScene::removeItem(item);

    m_info.allocated -= item->allocationInfo().size;
    m_info.totalSize = item->allocationInfo().pool_size;
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

        if (size < (long)sizeof(CSPEPacketEvent)) {
            fclose(trace);
            return;
        }

        fseek(trace, 0, SEEK_SET);

        trackingTraceOffset = m_parent->m_trackingTraceOffset;

        m_parent->m_traceController->setTimeLineMinMax(0, size / sizeof(CSPEPacketEvent));
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
                fseek(trace, -sizeof(CSPEPacketEvent), SEEK_CUR);

            s = fread(buf, sizeof(CSPEPacketEvent), 1, trace);
            s = (s == 1) ? sizeof(CSPEPacketEvent) : 0;

            if (mode == FAST_REWIND)
                fseek(trace, -sizeof(CSPEPacketEvent), SEEK_CUR);

            currentPosition = ftell(trace);
            currentPosition /= sizeof(CSPEPacketEvent);

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

void RenderController::RenderAllocation(ControllerScene *scene, CSPEAllocationInfo* info)
{
    RenderAllocationItem *allocationItem = new RenderAllocationItem(scene, info);

    allocationItem->setPosition();

    scene->addItem(allocationItem);
    scene->update();
}

void RenderController::ReleaseAllocation(ControllerScene *scene, CSPEAllocationInfo* info)
{
    RenderAllocationItem* item = scene->lookup(info->offset);

    if (item) {
        scene->removeItem(item);
        scene->update();
    }
}

void RenderController::processFullCapture(char* buf, int size)
{
    CSPEPacketEvent *packet = reinterpret_cast<CSPEPacketEvent*>(buf);

    if (packet->payload.full.count)
    {
        CSPEFullInfo* fullMap = &packet->payload.full;

        size -= offsetof(CSPEPacketEvent, payload.full.allocations);

        for (unsigned int i = 0; (i < fullMap->count) && ((unsigned int)size >= sizeof(CSPEAllocationInfo)); i++)
        {
            if (!m_controllerSceneMap.contains(fullMap->allocations[i].pool_id))
            {
                ControllerScene *scene = new ControllerScene(this, &fullMap->allocations[i]);
                m_controllerSceneMap.insert(fullMap->allocations[i].pool_id, scene);

                scene->setSceneRect(0, 0, 800, 600);
                scene->setBackgroundBrush(QBrush(QColor(0, 0, 0)));
                scene->setForegroundBrush(QBrush(QColor(0, 0, 0)));

                emit newSurfacePool(scene, fullMap->allocations[i].name);
            }

            RenderAllocation(m_controllerSceneMap.value(fullMap->allocations[i].pool_id), &fullMap->allocations[i]);
            size -= sizeof(CSPEAllocationInfo);
        }

        assert(size == 0);

        if (size)
            emit missingInformation(packet->header.nseq);

        m_controllerStatus = STATUS_RECEIVING;
    }
}

void RenderController::partialAllocationEvent(CSPEPacketEvent event)
{
    CSPEPartialInfo* partialInfo = &event.payload.partial;

    ControllerScene *scene;

    scene = m_controllerSceneMap.value(partialInfo->allocation.pool_id);

    if (!m_controllerSceneMap.contains(partialInfo->allocation.pool_id))
            assert(!scene);

    if (!scene && !m_controllerSceneMap.contains(partialInfo->allocation.pool_id))
    {
        scene = new ControllerScene(this, &partialInfo->allocation);

        m_controllerSceneMap.insert(partialInfo->allocation.pool_id, scene);

        scene->setSceneRect(0, 0, 800, 600);
        scene->setBackgroundBrush(QBrush(QColor(0, 0, 0)));

        emit newSurfacePool(scene, partialInfo->allocation.name);
    }

    RenderAllocation(scene, &partialInfo->allocation);
}

void RenderController::partialReleaseEvent(CSPEPacketEvent event)
{
    CSPEPartialInfo* partialInfo = &event.payload.partial;

    ControllerScene *scene = m_controllerSceneMap.value(partialInfo->allocation.pool_id);

    if (scene)
        ReleaseAllocation(scene, &partialInfo->allocation);
}

void RenderController::processPartialPacket(char* buf)
{
    CSPEPacketEvent *packet = reinterpret_cast<CSPEPacketEvent*>(buf);

    switch (packet->header.event) {
    case CSPE_ALLOCATION_EVENT:
        emit partialAllocation(*packet); // Copy the object before switching the the main thread
        break;
    case CSPE_RELEASE_EVENT:
        emit partialRelease(*packet); // Copy the object before switching the the main thread
        break;
    default:
        break;
    }
}

void RenderController::processPacket(char* buf, int size, TracePlaybackMode mode)
{
    CSPEPacketHeader *header = reinterpret_cast<CSPEPacketHeader*>(buf);

    m_currentNseq = header->nseq;
    m_expectedNseq = m_currentNseq + 1;

    if (m_saveToFile)
        m_outputTrace.write(buf, size);

    switch (m_controllerStatus) {
    case STATUS_SYNCING:
        if (header->event != CSPE_FULL_CAPTURE_EVENT)
            return;
        processFullCapture(buf, size);
        break;
    case STATUS_RECEIVING:
        if ((header->event != CSPE_ALLOCATION_EVENT) && (header->event != CSPE_RELEASE_EVENT))
            return;

        if (mode == FAST_REWIND) // force a release event if we're rewinding the trace
            header->event = CSPE_RELEASE_EVENT;

        processPartialPacket(buf);
        break;
    default:
        break;
    }
}

void RenderController::packetReceived(char* buf, int size, TracePlaybackMode mode)
{
    // Inspect the header for the sequence number
    CSPEPacketHeader *header = reinterpret_cast<CSPEPacketHeader*>(buf);

    if ((mode != FAST_REWIND) && (header->nseq != m_expectedNseq)) {
        m_currentNseq = header->nseq;

        emit lostPackets(m_currentNseq, m_expectedNseq);
        m_expectedNseq = m_currentNseq + 1;

        m_controllerStatus = STATUS_RECEIVING; //STATUS_SYNCING;
    } else {
        if (m_controllerStatus == STATUS_IDLE)
            m_controllerStatus = STATUS_RECEIVING; //STATUS_SYNCING;

        if (size != sizeof(CSPEPacketEvent))
            emit missingInformation(header->nseq);
        else
            processPacket(buf, size, mode);
    }
}
