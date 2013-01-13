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

#include "allocationrendercontroller.h"
#include "allocationrenderitem.h"

#include <QPainter>

#define UNUSED_PARAM(a) (a) = (a)

DirectFBPixelFormatNames(pf_names);

AllocationRenderItem::AllocationRenderItem(SceneController *scene, DFBTracingBufferData *data)
{
    m_age = 0;
    m_scene = scene;
    m_allocation = *data;
    m_text = 0;
}

void AllocationRenderItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    int x1, y1, x2, y2;

    UNUSED_PARAM(option);
    UNUSED_PARAM(widget);

    x1 = (int)(m_allocation.offset * m_scene->aspectRatio()) % (int)m_scene->width();
    y1 = (int)(m_allocation.offset * m_scene->aspectRatio()) / (int)m_scene->width();

    x2 = (int)((m_allocation.offset + m_allocation.size) * m_scene->aspectRatio()) % (int)m_scene->width();
    y2 = (int)((m_allocation.offset + m_allocation.size) * m_scene->aspectRatio()) / (int)m_scene->width();

    int idx = (DFB_PIXELFORMAT_INDEX(m_allocation.format) * 255) / DFB_NUM_PIXELFORMATS;
    QColor color = QColor((idx + 32) % 256, (idx + 64) % 256, (idx + 128) % 256);

    if (y2 > y1) {
        painter->setPen(color);
        painter->drawLine(x1, y1, m_scene->width(), y1);
        painter->fillRect(0, y1 + 1, m_scene->width(), y2 - y1 - 1, color);
        painter->drawLine(0, y2, x2, y2);
    } else {
        painter->setPen(color);
        painter->drawLine(x1, y1, x2, y1);
    }
}

QRectF AllocationRenderItem::boundingRect () const
{
    QRectF rect;

    qreal height, width;
    int x1, y1, x2, y2;

    x1 = (int)(m_allocation.offset * m_scene->aspectRatio()) % (int)m_scene->width();
    y1 = (int)(m_allocation.offset * m_scene->aspectRatio()) / (int)m_scene->width();

    x2 = (int)((m_allocation.offset + m_allocation.size) * m_scene->aspectRatio()) % (int)m_scene->width();
    y2 = (int)((m_allocation.offset + m_allocation.size) * m_scene->aspectRatio())/ (int)m_scene->width();

    if (y2 > y1) {
        width = m_scene->width();
        height = (y2 - y1 + 1);
    } else {
        width = (x2 - x1);
        height = 1;
    }

    rect.setCoords(x1, y1, width, height);

    return rect;
}

void AllocationRenderItem::setPosition()
{
    char buf[256];

    int x1 = (int)(m_allocation.offset * m_scene->aspectRatio()) % (int)m_scene->width();
    int y1 = (int)(m_allocation.offset * m_scene->aspectRatio()) / (int)m_scene->width();

    setPos(0, 0);

    sprintf(buf, "%dx%d, %s", m_allocation.width, m_allocation.height, pf_names[DFB_PIXELFORMAT_INDEX(m_allocation.format)].name);

    m_text = m_scene->addText(buf);
    m_text->setPos(x1, y1);
    m_text->setParentItem(this);
}

int AllocationRenderItem::elder()
{
    return (m_age++);
}
