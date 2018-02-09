// Copyright (c) 2016 Jonas Schnelli
// Copyright (c) 2018 The PIVX Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qrcodescanner.h"

#include <QGraphicsView>
#include <QCameraViewfinder>
#include <QtDebug>

#if QT_VERSION >= 0x050300
#include <QCameraInfo>
#endif

#if QT_VERSION >= 0x050500
#include <QCameraViewfinderSettings>
#endif

const static int CAMIMG_WIDTH  = 640;
const static int CAMIMG_HEIGHT = 480;
const static int MAX_FPS = 8;

#ifdef Q_OS_WIN
const static bool NEEDS_H_MIRRORING = true;
const static bool NEEDS_V_MIRRORING = false;
#else
const static bool NEEDS_H_MIRRORING = false;
const static bool NEEDS_V_MIRRORING = false;
#endif

void QRCodeScannerThread::setNextImage(const QImage *aImage)
{
    mutex.lock();
    if (nextImage)
    {
        delete nextImage;
        nextImage = NULL;
    }
    // copy image to the one-level-queue
    nextImage = new QImage(*aImage);

    // knock-knock on out thread
    imagePresent.wakeAll();
    mutex.unlock();
}

void QRCodeScannerThread::run()
{
    forever {
        bool fSuccess = false;
        mutex.lock();
        imagePresent.wait(&mutex);

        //copy out image to unlock asap
        QImage *image = NULL;
        if (nextImage)
        {
            QImage cImage = nextImage->copy();
            image = new QImage(cImage);
        }
        mutex.unlock();

        if (image)
        {
            //check for QRCode
            struct quirc *qr = quirc_new();
            if (qr) {
                int w, h, count;
                int depth = 4;
                if (quirc_resize(qr, image->width(), image->height()) >= 0)
                {
                    uint8_t *scr = quirc_begin(qr, &w, &h);
                    memset(scr, 0, w*h);
                    for (int ii = 0; ii < image->height(); ii++) {
                        unsigned char* scan = (unsigned char *)image->scanLine(ii);
                        for (int jj = 0; jj < image->width(); jj++) {

                            QRgb* rgbpixel = reinterpret_cast<QRgb*>(scan + jj*depth);
                            //*scr = qGray(*rgbpixel);
                            memset(scr, qGray(*rgbpixel), 1);
                            scr++;
                        }
                    }
                    quirc_end(qr);
                    count = quirc_count(qr);
                    if (count > 0)
                    {
                        qWarning("found codes : %d", count);
                        struct quirc_code code;
                        struct quirc_data data;

                        quirc_extract(qr, 0, &code);
                        quirc_decode_error_t err = quirc_decode(&code, &data);

                        if (!err)
                        {
                            if (!fSuccess) {
                                emit QRCodeFound(QString(QLatin1String((const char *)data.payload)));
                                fSuccess = true;
                            }
                        }
                        else
                             qWarning("quirc error : %s", quirc_strerror(err));
                    }
                }
                quirc_destroy(qr);
            }

            delete image; image = 0;
        }
    }
}

CameraFrameGrabber::CameraFrameGrabber(QObject *parent, QGraphicsPixmapItem *pixmapItemIn) :
QAbstractVideoSurface(parent), pixmapItem(pixmapItemIn)
{
    scanThread = new QRCodeScannerThread();
    scanThread->start();

    qr = quirc_new();
}

QList<QVideoFrame::PixelFormat> CameraFrameGrabber::supportedPixelFormats(QAbstractVideoBuffer::HandleType handleType) const
{
    Q_UNUSED(handleType);
    return QList<QVideoFrame::PixelFormat>()
    << QVideoFrame::Format_ARGB32
    << QVideoFrame::Format_ARGB32_Premultiplied
    << QVideoFrame::Format_RGB32
    << QVideoFrame::Format_RGB24
    << QVideoFrame::Format_RGB565
    << QVideoFrame::Format_RGB555
    << QVideoFrame::Format_ARGB8565_Premultiplied
    << QVideoFrame::Format_BGRA32
    << QVideoFrame::Format_BGRA32_Premultiplied
    << QVideoFrame::Format_BGR32
    << QVideoFrame::Format_BGR24
    << QVideoFrame::Format_BGR565
    << QVideoFrame::Format_BGR555
    << QVideoFrame::Format_BGRA5658_Premultiplied
    << QVideoFrame::Format_AYUV444
    << QVideoFrame::Format_AYUV444_Premultiplied
    << QVideoFrame::Format_YUV444
    << QVideoFrame::Format_YUV420P
    << QVideoFrame::Format_YV12
    << QVideoFrame::Format_UYVY
    << QVideoFrame::Format_YUYV
    << QVideoFrame::Format_NV12
    << QVideoFrame::Format_NV21
    << QVideoFrame::Format_IMC1
    << QVideoFrame::Format_IMC2
    << QVideoFrame::Format_IMC3
    << QVideoFrame::Format_IMC4
    << QVideoFrame::Format_Y8
    << QVideoFrame::Format_Y16
    << QVideoFrame::Format_Jpeg
    << QVideoFrame::Format_CameraRaw
    << QVideoFrame::Format_AdobeDng;
}

bool CameraFrameGrabber::present(const QVideoFrame &frame)
{
    if (frame.isValid()) {
        QVideoFrame cloneFrame(frame);
        cloneFrame.map(QAbstractVideoBuffer::ReadOnly);
        const QImage image(cloneFrame.bits(),
                                    cloneFrame.width(),
                                    cloneFrame.height(),
                                    QVideoFrame::imageFormatFromPixelFormat(cloneFrame.pixelFormat()));

        QImage copyI;
        if (NEEDS_H_MIRRORING || NEEDS_V_MIRRORING)
        {
            QTransform transform;
            transform.rotate(180);
            copyI = image.mirrored(NEEDS_H_MIRRORING, NEEDS_V_MIRRORING).transformed(transform).copy();
        }
        else
        {
            copyI = image.copy();
        }
        const QImage small = copyI.scaled(CAMIMG_WIDTH,CAMIMG_HEIGHT,Qt::KeepAspectRatio);
        imageSize = small.size();

        scanThread->setNextImage(&small);
        if (pixmapItem)
        {
            const QPixmap pixmap = QPixmap::fromImage(small);
            pixmapItem->setPixmap(pixmap);
        }

        cloneFrame.unmap();
        return true;
    }
    return false;
}

bool QRCodeScanner::availability()
{
#if QT_VERSION >= 0x050300
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    return !cameras.empty();
#else
    return true;
#endif
}

QRCodeScanner::~QRCodeScanner()
{
    delete gview;
    delete gscene;
    delete frameGrabber;
    delete pixmapItem;
    delete cam;
    delete vlayout;
}

QRCodeScanner::QRCodeScanner(QWidget *parent)
{
    resize(CAMIMG_WIDTH,CAMIMG_HEIGHT);

    // create graphics scene, view and single pixmap container
    gscene = new QGraphicsScene(this);
    gview = new QGraphicsView(gscene);
    pixmapItem = new QGraphicsPixmapItem();
    gscene->addItem(pixmapItem);

    // create a camera
    cam = new QCamera();

#if QT_VERSION >= 0x050500
#ifndef Q_OS_WIN
    // if compile agaist Qt5, reduce framerate
    QCameraViewfinderSettings settings;
    settings.setMaximumFrameRate(MAX_FPS);
    cam->setViewfinderSettings(settings);
#endif
#endif

    // create the framegrabber viewfinder
    frameGrabber = new CameraFrameGrabber(this, pixmapItem);
    cam->setViewfinder(frameGrabber);
    cam->start();
    // create the window/layout
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    gview->setSizePolicy(sizePolicy);

    vlayout = new QVBoxLayout();
    vlayout->setSpacing(0);
    vlayout->setContentsMargins(0,0,0,0);
    setLayout(vlayout);
    gview->move(0,0);
    vlayout->addWidget(gview);

    // set background color
    QPalette pal;
    pal.setColor(QPalette::Background, Qt::black);
    setPalette(pal);
    gview->setPalette(pal);
    gview->setBackgroundBrush(QBrush(Qt::black, Qt::SolidPattern));

    connect(frameGrabber->scanThread, SIGNAL(QRCodeFound(const QString&)), this, SLOT(passQRCodePayload(const QString&)));
}

void QRCodeScanner::resizeEvent(QResizeEvent * event)
{
    // resize elements to window size
    vlayout->setGeometry(QRect(0, 0, this->width(), this->height()));;
    gview->fitInView(QRect(0, 0, this->width(), this->height()));
    gscene->setSceneRect(QRect(0, 0, this->width(), this->height()));

    //scale QGraphicsPixmapItem to exact size factor
    double sfact = 1.0/CAMIMG_WIDTH*this->width();
    if (frameGrabber && frameGrabber->pixmapItem)
        frameGrabber->pixmapItem->setScale(sfact);

    // fix height
    if (frameGrabber->imageSize.height() > 0)
        this->resize(this->width(), frameGrabber->imageSize.height()*sfact);
}

void QRCodeScanner::closeEvent(QCloseEvent *event)
{
    // disable the scanner when user closes the window
    setScannerActive(false);
    event->accept();
}

void QRCodeScanner::setScannerActive(bool state)
{
    if (state)
        cam->start();
    else
        cam->stop();
}

void QRCodeScanner::passQRCodePayload(const QString &payload)
{
    emit QRCodeFound(payload);
}
