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
    m_info.usageRatio = m_info.allocated / (float)m_info.totalSize;
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
    m_info.usageRatio = m_info.allocated / (float)m_info.totalSize;
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

void RenderController::disconnect()
{
    if (m_runThread) {
        m_runThread = false;
        shutdown(m_udpSocket, SHUT_RDWR);

        close(m_udpSocket);
        m_udpSocket = -1;
    }

    if (m_receiver) {
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
}

RenderController::ReceiverThread::ReceiverThread(RenderController *parent)
{
    m_parent = parent;

    connect(m_parent, SIGNAL(packetReceived(char*,int)), m_parent, SLOT(packetReceivedSink(char*,int)));
}

void RenderController::ReceiverThread::run()
{
    char buf[4096];

    struct sockaddr_in addrIn;
    socklen_t addrLen = sizeof(addrIn);

    memset(&addrIn, 0, sizeof(addrIn));

    while (m_parent->m_runThread)
    {
        addrLen = sizeof(addrIn);
        int s = recvfrom(m_parent->m_udpSocket, buf, sizeof(buf), 0, (sockaddr*)&addrIn, &addrLen);

        if (s <= 0) {
            m_parent->m_controllerStatus = (s == 0) ? STATUS_IDLE : STATUS_ERROR;
            break;
        }

        emit m_parent->packetReceived(buf, s);
    }

    disconnect(m_parent, SIGNAL(packetReceived(char*,int)), m_parent, SLOT(packetReceivedSink(char*,int)));

    m_parent->m_controllerStatus = STATUS_IDLE;
}

void RenderController::packetReceivedSink(char* buf, int size)
{
    // Inspect the header for the sequence number
    CSPEPacketHeader *header = reinterpret_cast<CSPEPacketHeader*>(buf);

    if (header->nseq != m_expectedNseq) {
        m_currentNseq = header->nseq;

        emit lostPackets(m_currentNseq, m_expectedNseq);
        m_expectedNseq = m_currentNseq + 1;

        m_controllerStatus = STATUS_RECEIVING; //STATUS_SYNCING;
    } else {
        if (m_controllerStatus == STATUS_IDLE)
            m_controllerStatus = STATUS_RECEIVING; //STATUS_SYNCING;

        processPacket(buf, size);
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

void RenderController::processPartialPacket(char* buf, int size)
{
    CSPEPacketEvent *packet = reinterpret_cast<CSPEPacketEvent*>(buf);
    CSPEPartialInfo* partialInfo = &packet->payload.partial;

    ControllerScene *scene;

    size -= offsetof(CSPEPacketEvent, payload.partial.allocation);

    switch (packet->header.event) {
    case CSPE_ALLOCATION_EVENT:
        if (!m_controllerSceneMap.contains(partialInfo->allocation.pool_id))
        {
            scene = new ControllerScene(this, &partialInfo->allocation);

            m_controllerSceneMap.insert(partialInfo->allocation.pool_id, scene);

            scene->setSceneRect(0, 0, 800, 600);
            scene->setBackgroundBrush(QBrush(QColor(0, 0, 0)));

            emit newSurfacePool(scene, partialInfo->allocation.name);
        }

        RenderAllocation(m_controllerSceneMap.value(partialInfo->allocation.pool_id), &partialInfo->allocation);
        break;
    case CSPE_RELEASE_EVENT:
        scene = m_controllerSceneMap.value(partialInfo->allocation.pool_id);
        if (scene)
            ReleaseAllocation(scene, &partialInfo->allocation);
        break;
    default:
        break;
    }

    size -= sizeof(CSPEAllocationInfo);

    assert(size == 0);

    if (size)
        emit missingInformation(packet->header.nseq);
}

void RenderController::processPacket(char* buf, int size)
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
        processPartialPacket(buf, size);
        break;
    default:
        break;
    }
}
