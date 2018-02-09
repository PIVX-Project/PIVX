// Copyright (c) 2016 Jonas Schnelli
// Copyright (c) 2018 The PIVX Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QRCODESCANNER_H
#define BITCOIN_QT_QRCODESCANNER_H

#include <QDialog>
#include <QAbstractVideoSurface>
#include <QList>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QCamera>
#include <QVBoxLayout>
#include <QCloseEvent>

extern "C" {
#include "quirc.h"
}


class QRCodeScannerThread : public QThread
{
    Q_OBJECT

private:
    QImage *nextImage;
    QMutex mutex;
    QWaitCondition imagePresent;

signals:
    void QRCodeFound(const QString &payload);

public:
    QRCodeScannerThread(QObject *parent = NULL) : QThread(parent), nextImage(0)
    {}

    void run() Q_DECL_OVERRIDE;
    void setNextImage(const QImage *aImage);
};

class CameraFrameGrabber : public QAbstractVideoSurface
{
    Q_OBJECT
public:
    explicit CameraFrameGrabber(QObject *parent = 0, QGraphicsPixmapItem *pmapItemIn = 0);
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const;
    bool present(const QVideoFrame &frame);

    QRCodeScannerThread *scanThread; //<!weak pointer link
    QGraphicsPixmapItem *pixmapItem; //<!weak pointer link
    QSizeF imageSize;

private:
    struct quirc *qr;
};

class QRCodeScanner : public QDialog
{
    Q_OBJECT
public:

    // static function to check if the scanning function is available
    static bool availability();

    explicit QRCodeScanner(QWidget *parent);
    ~QRCodeScanner();

    void setScannerActive(bool state);
    void resizeEvent(QResizeEvent * event);
    void closeEvent(QCloseEvent *event);

    QVBoxLayout *vlayout;
    QGraphicsScene *gscene;
    QCamera *cam;
    QGraphicsView* gview;
    CameraFrameGrabber *frameGrabber;
    QGraphicsPixmapItem *pixmapItem;
signals:
    void QRCodeFound(const QString &payload);
public slots:
    void passQRCodePayload(const QString &payload);
};

#endif // BITCOIN_QT_QRCODESCANNER_H
