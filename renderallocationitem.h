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

#ifndef RENDERALLOCATIONITEM_H
#define RENDERALLOCATIONITEM_H

#include <QGraphicsItem>
#include <QGraphicsScene>

#include "rendercontroller.h"

#include <core/cspe_packet.h>

class ControllerScene;

class RenderAllocationItem : public QGraphicsItem
{
public:
    explicit RenderAllocationItem(ControllerScene *scene, CSPEAllocationInfo *info);

    QRectF boundingRect () const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    int elder();
    void setPosition();

    const CSPEAllocationInfo& allocationInfo() { return m_allocationInfo; }

signals:

public slots:

private:
    int m_age;

    QGraphicsTextItem *m_text;

    ControllerScene *m_scene;

    CSPEAllocationInfo m_allocationInfo;

    float m_renderAspectRatio;
};

#endif // RENDERALLOCATIONITEM_H
