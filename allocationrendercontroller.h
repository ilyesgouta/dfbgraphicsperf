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

#ifndef RENDERCONTROLLER_H
#define RENDERCONTROLLER_H

#include <QSemaphore>
#include <QThreadPool>
#include <QRunnable>

#include <time.h>
#include <fstream>

using namespace std;

#include "allocationrenderitem.h"
#include "tracecontrollerdialog.h"

class ControllerScene;
class TraceControllerDialog;

class AllocationRenderController : public QObject
{
    Q_OBJECT
public:
    explicit AllocationRenderController(QString ipAddr, int port, bool saveToFile);
    explicit AllocationRenderController(QString traceFile, int period_ms);
    ~AllocationRenderController();

    bool connect();
    bool renderTrace();

    void disconnect();

    void saveTraceToFile(bool save);

signals:
    void newSurfacePool(ControllerScene* scene, char* name);
    void lostPackets(unsigned int lastValidNseq, unsigned int expectedNseq);
    void badPacket(unsigned int nseq);
    void missingInformation(unsigned int nseq);
    void finished();

    void bufferAllocation(DFBTracingPacket packet);
    void bufferRelease(DFBTracingPacket packet);

    void tracePlaybackEnded();

private slots:
    void changeRenderPace(int value);
    void pauseTraceRendering();
    void resumeTraceRendering();
    void timeLineTracking(int value);
    void timeLineReleased(int value);

    void allocationEvent(DFBTracingPacket type);
    void releaseEvent(DFBTracingPacket type);

    void tracePlaybackEndedEvent();

private:
    typedef enum {
        STATUS_IDLE,
        STATUS_RECEIVING,
        STATUS_SYNCING,
        STATUS_ERROR
    } ControllerStatus;

    typedef enum {
        NORMAL,
        FAST_FORWARD,
        FAST_REWIND
    } TracePlaybackMode;

    class ReceiverThread : public QThread {
    public:
        ReceiverThread(AllocationRenderController* parent);
        ~ReceiverThread();

        void run();

    private:
        AllocationRenderController *m_parent;
    };

    void receivePacket(char* buf, int size, TracePlaybackMode mode);
    void processPacket(char* buf, int size, TracePlaybackMode mode);

    void processSnapshotEvent(char* buf, int size);
    void processBufferEvent(char* buf);

    void renderAllocation(ControllerScene *scene, DFBTracingBufferData* data);
    void releaseAllocation(ControllerScene *scene, DFBTracingBufferData* data);

    QMap<unsigned int, ControllerScene *> m_controllerSceneMap;

    QString m_ipAddr;
    int m_port;
    int m_udpSocket;

    QString m_trace;
    int m_renderPeriod; // in ms

    TraceControllerDialog *m_traceController;
    long m_trackingTraceOffset;
    bool m_isTracking;

    bool m_runThread;
    ReceiverThread *m_receiver;

    QSemaphore m_renderingSemaphore;
    bool m_isPaused;

    bool m_saveToFile;
    ofstream m_outputTrace;
    struct tm m_startingDate;

    unsigned int m_currentNseq, m_expectedNseq;

    ControllerStatus m_controllerStatus;
};

#endif // RENDERCONTROLLER_H
