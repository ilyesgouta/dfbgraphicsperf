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

#include "scenecontroller.h"
#include "allocationrenderitem.h"

SceneController::SceneController(QObject *parent, DFBTracingBufferData *info) : QGraphicsScene(parent)
{
    m_allocation = *info;
    m_allocationItemsHash.clear();

    memset(&m_info, 0, sizeof(m_info));

    m_info.lowestUsage = 0xffffffff;
}

void SceneController::setSceneRect(const QRectF &rect)
{
    QGraphicsScene::setSceneRect(rect);

    m_renderAspectRatio = (rect.width() * rect.height()) / m_allocation.poolSize;
}

void SceneController::setSceneRect(qreal x, qreal y, qreal w, qreal h)
{
    QGraphicsScene::setSceneRect(x, y, w, h);

    m_renderAspectRatio = (w * h) / m_allocation.poolSize;
}

float SceneController::aspectRatio()
{
    return m_renderAspectRatio;
}

void SceneController::addItem(AllocationRenderItem *item)
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

void SceneController::removeItem(AllocationRenderItem *item)
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

AllocationRenderItem* SceneController::lookup(unsigned int offset)
{
    return m_allocationItemsHash.value(offset);
}

const ControllerInfo& SceneController::getInfo()
{
    return (m_info);
}
