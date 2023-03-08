// Copyright (c) 2019 The PIVX Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_PFBorderImage_H
#define PIVX_PFBorderImage_H

#include <QPainter>
#include <QPixmap>
#include <QWidget>
#include <QFrame>

class PFBorderImage : public QFrame
{
    Q_OBJECT

public:
    PFBorderImage(QWidget *parent = nullptr) : QFrame(parent) {};
    ~PFBorderImage() {};

#if defined(Q_OS_MAC)

    void load(QString path) {
        if (img.isNull() && !path.isNull()) img = QPixmap(path);
    }

protected:

    void paintEvent(QPaintEvent *event) override {
        QSize currentSize = size();
        if (cachedSize != currentSize && checkSize(currentSize)) {
            cachedSize = currentSize;
            scaledImg = img.scaled(cachedSize);
        }
        QPainter painter(this);
        painter.drawPixmap(0, 0, scaledImg);
        QWidget::paintEvent(event);
    }

    bool checkSize(QSize currentSize) {
        // Do not scale the image if the window change are imperceptible for the human's eye.
        int pixelatedImageValue = 50;
        return (currentSize.width() > cachedSize.width() + pixelatedImageValue) || (currentSize.width() < cachedSize.width() - pixelatedImageValue)
                || (currentSize.height() > cachedSize.height() + pixelatedImageValue) || (currentSize.height() < cachedSize.height() - pixelatedImageValue);
    }

    QPixmap img;
    QPixmap scaledImg;
    QSize cachedSize;

#endif

};

#endif //PIVX_PFBorderImage_H
