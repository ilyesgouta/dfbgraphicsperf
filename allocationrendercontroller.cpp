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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <assert.h>

#include "allocationrenderitem.h"
#include "allocationscenecontroller.h"
#include "allocationrendercontroller.h"

AllocationRenderController::AllocationRenderController(QString ipAddr, int port, bool saveToFile)
{
    m_ipAddr = ipAddr;
    m_port = port;

    m_currentNseq = m_expectedNseq = 0;

    m_saveToFile = saveToFile;

    m_receiver = 0;
    m_traceController = 0;

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

AllocationRenderController::AllocationRenderController(QString traceFile, int period) : m_renderingSemaphore(1)
{
    m_ipAddr = "";
    m_port = -1;

    m_saveToFile = false;

    m_receiver = 0;

    m_trace = traceFile;
    m_renderPeriod = period;

    m_currentNseq = m_expectedNseq = 0;
}

AllocationRenderController::~AllocationRenderController()
{
    if (m_runThread)
        disconnect();
}

void AllocationRenderController::saveTraceToFile(bool save)
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

bool AllocationRenderController::connect()
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

bool AllocationRenderController::renderTrace()
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

void AllocationRenderController::disconnect()
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

    QList<SceneController*> scenes = m_controllerSceneMap.values();
    for (QList<SceneController*>::iterator it = scenes.begin(); it != scenes.end(); ++it)
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

AllocationRenderController::ReceiverThread::ReceiverThread(AllocationRenderController *parent)
{
    m_parent = parent;

    QObject::connect(m_parent, SIGNAL(bufferAllocation(DFBTracingPacket)), m_parent, SLOT(allocationEvent(DFBTracingPacket)));
    QObject::connect(m_parent, SIGNAL(bufferRelease(DFBTracingPacket)), m_parent, SLOT(releaseEvent(DFBTracingPacket)));
}

AllocationRenderController::ReceiverThread::~ReceiverThread()
{
    QObject::disconnect(m_parent, SIGNAL(bufferAllocation(DFBTracingPacket)));
    QObject::disconnect(m_parent, SIGNAL(bufferRelease(DFBTracingPacket)));
}

void AllocationRenderController::ReceiverThread::run()
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

        m_parent->receivePacket(buf, s, mode);
    }

    m_parent->m_controllerStatus = STATUS_IDLE;

    if (m_parent->m_traceController)
        m_parent->m_traceController->stop();

    if (trace)
        fclose(trace);

    emit m_parent->tracePlaybackEnded();
}

void AllocationRenderController::changeRenderPace(int value)
{
    m_renderPeriod = value;
}

void AllocationRenderController::pauseTraceRendering()
{
    if (m_receiver && !m_isPaused) {
        m_renderingSemaphore.acquire();
        m_isPaused = true;
    }
}

void AllocationRenderController::resumeTraceRendering()
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

void AllocationRenderController::timeLineTracking(int value)
{
    m_isTracking = true;
}

void AllocationRenderController::timeLineReleased(int value)
{
    m_isTracking = false;

    if (!m_isPaused)
        m_renderingSemaphore.acquire();

    m_trackingTraceOffset = value;

    if (!m_isPaused)
        m_renderingSemaphore.release();
}

void AllocationRenderController::tracePlaybackEndedEvent()
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

void AllocationRenderController::renderAllocation(SceneController *scene, DFBTracingBufferData* data)
{
    AllocationRenderItem *allocationItem = new AllocationRenderItem(scene, data);

    allocationItem->setPosition();

    scene->addItem(allocationItem);
    scene->update();
}

void AllocationRenderController::releaseAllocation(SceneController *scene, DFBTracingBufferData* data)
{
    QGraphicsItem* item = scene->lookup(data->offset);

    AllocationRenderItem* allocationItem = static_cast<AllocationRenderItem*>(item);

    if (allocationItem) {
        scene->removeItem(allocationItem);
        scene->update();
    }
}

void AllocationRenderController::processSnapshotEvent(char* buf, int size)
{
    DFBTracingPacket *packet = reinterpret_cast<DFBTracingPacket*>(buf);

    if (packet->Payload.pool.count)
    {
        DFBTracingPoolData* pool = &packet->Payload.pool;
        SceneController *scene = 0;

        // Clear any QGraphicsItem objects already inserted
        // WARN: assumes all stats belong to the same poolId
        if (m_controllerSceneMap.contains(pool->stats[0].poolId)) {
            SceneController *scene = m_controllerSceneMap.value(pool->stats[0].poolId);
            assert(scene);
            scene->clear();
        }

        size -= sizeof(DFBTracingPacketHeader);
        size -= offsetof(DFBTracingPoolData, stats);

        assert(size > 0);

        for (unsigned int i = 0; (i < pool->count) && ((unsigned int)size >= sizeof(DFBTracingBufferData)); i++)
        {
            // Create a new ControllerScene if this is a new poolId
            if (!m_controllerSceneMap.contains(pool->stats[i].poolId))
            {
                scene = new AllocationSceneController(this, &pool->stats[i]);

                m_controllerSceneMap.insert(pool->stats[i].poolId, scene);

                emit newSurfacePool(scene, pool->stats[i].name);
            } else
                scene = m_controllerSceneMap.value(pool->stats[i].poolId);

            assert(scene);

            renderAllocation(scene, &pool->stats[i]);

            size -= sizeof(DFBTracingBufferData);
        }

        assert(size == 0);

        if (size)
            emit badPacket(packet->header.nSeq);

        m_controllerStatus = STATUS_RECEIVING;
    }
}

void AllocationRenderController::allocationEvent(DFBTracingPacket packet)
{
    DFBTracingBufferData* data = &packet.Payload.buffer;

    SceneController *scene;

    scene = m_controllerSceneMap.value(data->poolId);

    if (!m_controllerSceneMap.contains(data->poolId))
        assert(!scene);

    // Create a new ControllerScene if this is a new poolId
    if (!scene && !m_controllerSceneMap.contains(data->poolId))
    {
        scene = new AllocationSceneController(this, data);

        m_controllerSceneMap.insert(data->poolId, scene);

        emit newSurfacePool(scene, data->name);
    }

    renderAllocation(scene, data);
}

void AllocationRenderController::releaseEvent(DFBTracingPacket packet)
{
    DFBTracingBufferData* data = &packet.Payload.buffer;

    SceneController *scene = m_controllerSceneMap.value(data->poolId);

    if (scene)
        releaseAllocation(scene, data);
}

void AllocationRenderController::processBufferEvent(char* buf)
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

void AllocationRenderController::processPacket(char* buf, int size, TracePlaybackMode mode)
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

void AllocationRenderController::receivePacket(char* buf, int size, TracePlaybackMode mode)
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

        if (size != (sizeof(DFBTracingPacketHeader) + packet->header.size))
            emit missingInformation(packet->header.nSeq);
        else
            processPacket(buf, size, mode);
    }
}
