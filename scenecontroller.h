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

#ifndef CONTROLLERSCENE_H
#define CONTROLLERSCENE_H

#include <QGraphicsScene>

#include <QMap>
#include <QHash>

#include <core/remote_tracing.h>

class AllocationRenderItem;

typedef struct ControllerInfo {
    unsigned int allocated;
    unsigned int totalSize;
    unsigned int peakUsage;
    unsigned int lowestUsage;
    float usageRatio;
} ControllerInfo;

class SceneController : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit SceneController(QObject *parent, DFBTracingBufferData* info);

    void setSceneRect(const QRectF &rect);
    void setSceneRect(qreal x, qreal y, qreal w, qreal h);

    float aspectRatio();

    void addItem(AllocationRenderItem *item);
    void removeItem(AllocationRenderItem *item);

    AllocationRenderItem* lookup(unsigned int offset);

    const ControllerInfo& getInfo();

signals:
    void allocationChanged(const ControllerInfo& info);

private:
    DFBTracingBufferData m_allocation;
    ControllerInfo m_info;

    QHash<unsigned int, AllocationRenderItem *> m_allocationItemsHash;

    float m_renderAspectRatio;
};

#endif // CONTROLLERSCENE_H