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

#ifndef RENDERALLOCATIONITEM_H
#define RENDERALLOCATIONITEM_H

#include <QGraphicsItem>
#include <QGraphicsScene>

#include <directfb.h>
#include <directfb_strings.h>

#include <core/remote_tracing.h>

#include "scenecontroller.h"
#include "allocationrendercontroller.h"

class AllocationRenderItem : public QGraphicsItem
{
public:
    explicit AllocationRenderItem(SceneController *scene, DFBTracingBufferData *data);

    QRectF boundingRect () const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    int elder();
    void setPosition();

    const DFBTracingBufferData& allocation() { return m_allocation; }

signals:

public slots:

private:
    int m_age;

    QGraphicsTextItem *m_text;

    SceneController *m_scene;

    DFBTracingBufferData m_allocation;

    float m_renderAspectRatio;
};

#endif // RENDERALLOCATIONITEM_H
