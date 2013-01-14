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

#include "allocationscenecontroller.h"
#include "allocationrenderitem.h"

AllocationSceneController::AllocationSceneController(QObject *parent, DFBTracingBufferData *data) : SceneController(parent, data)
{
    m_allocation = *data;
    m_allocationItemsHash.clear();

    memset(&m_info, 0, sizeof(m_info));

    m_info.lowestUsage = 0xffffffff;
}

void AllocationSceneController::setSceneRect(const QRectF &rect)
{
    QGraphicsScene::setSceneRect(rect);

    m_renderAspectRatio = (rect.width() * rect.height()) / m_allocation.poolSize;
}

void AllocationSceneController::setSceneRect(qreal x, qreal y, qreal w, qreal h)
{
    QGraphicsScene::setSceneRect(x, y, w, h);

    m_renderAspectRatio = (w * h) / m_allocation.poolSize;
}

float AllocationSceneController::aspectRatio()
{
    return m_renderAspectRatio;
}

void AllocationSceneController::addItem(QGraphicsItem *item)
{
    AllocationRenderItem* allocationItem = static_cast<AllocationRenderItem*>(item);

    m_allocationItemsHash.insert(allocationItem->allocation().offset, allocationItem);
    QGraphicsScene::addItem(item);

    m_info.allocated += allocationItem->allocation().size;
    m_info.totalSize = allocationItem->allocation().poolSize;
    m_info.usageRatio = (m_info.allocated / (float)m_info.totalSize) * 100;
    m_info.peakUsage = (m_info.peakUsage < m_info.allocated) ? m_info.allocated : m_info.peakUsage;
    m_info.lowestUsage = (m_info.lowestUsage > m_info.allocated) ? m_info.allocated : m_info.lowestUsage;

    emit statusChanged();
}

void AllocationSceneController::removeItem(QGraphicsItem *item)
{
    AllocationRenderItem* allocationItem = static_cast<AllocationRenderItem*>(item);

    m_allocationItemsHash.remove(allocationItem->allocation().offset);
    QGraphicsScene::removeItem(item);

    m_info.allocated -= allocationItem->allocation().size;
    m_info.totalSize = allocationItem->allocation().poolSize;
    m_info.usageRatio = (m_info.allocated / (float)m_info.totalSize) * 100;
    m_info.peakUsage = (m_info.peakUsage < m_info.allocated) ? m_info.allocated : m_info.peakUsage;
    m_info.lowestUsage = (m_info.lowestUsage > m_info.allocated) ? m_info.allocated : m_info.lowestUsage;

    emit statusChanged();
}

QGraphicsItem* AllocationSceneController::lookup(unsigned int offset)
{
    return m_allocationItemsHash.value(offset);
}

void AllocationSceneController::getStatus(QString& status)
{
    char buf[256];

    sprintf(buf, "Allocated video memory:\n"
                 "  Currently allocated: %d (ratio: %.2f%%)\n"
                 "  Peak usage: %d, Lowest usage: %d\n"
                 "Lost packets: %d\n",
                 m_info.allocated, m_info.usageRatio, m_info.peakUsage, m_info.lowestUsage);
}
