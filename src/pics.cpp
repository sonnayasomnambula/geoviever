#include <QBuffer>
#include <QImageReader>
#include <QPixmap>

#include "exif/file.h" // TODO extract Orientation
#include "pics.h"

namespace Pics
{

QPixmap thumbnail(const QPixmap& pixmap, int size)
{
    QPixmap pic = (pixmap.width() > pixmap.height()) ? pixmap.scaledToHeight(size) : pixmap.scaledToWidth(size);
    return pic.copy((pic.width() - size) / 2, (pic.height() - size) / 2, size, size);
}

QString toBase64(const QPixmap& pixmap, const char* format)
{
    QByteArray raw;
    QBuffer buff(&raw);
    buff.open(QIODevice::WriteOnly);
    pixmap.save(&buff, format);

    QString base64("data:image/jpg;base64,");
    base64.append(QString::fromLatin1(raw.toBase64().data()));
    return base64;
}

QPixmap fromBase64(const QString& base64)
{
    static const int dataIndex = QString("data:image/jpg;base64,").size();
    QByteArray raw = QByteArray::fromBase64(base64.toLatin1().mid(dataIndex));
    QPixmap pix;
    pix.loadFromData(raw);
    return pix;
}

QPixmap fromImageReader(QImageReader* reader, int width, int height, Exif::Orientation orientation)
{
    if (orientation.isRotated())
        std::swap(width, height);

    QPixmap pic = fromImageReader(reader, width, height);

    QTransform transformation;

    if (orientation == Exif::Orientation::MirrorHorizontal) {
        transformation.scale(1, -1);
    } else if (orientation == Exif::Orientation::Rotate180) {
        transformation.rotate(180);
    } else if (orientation == Exif::Orientation::MirrorVertical) {
        transformation.scale(-1, 1);
    } else if (orientation == Exif::Orientation::MirrorHorizontalAndRotate270CW) {
        transformation.scale(1, -1).rotate(270);
    } else if (orientation == Exif::Orientation::Rotate90CW) {
        transformation.rotate(90);
    } else if (orientation == Exif::Orientation::MirrorHorizontalAndRotate90CW) {
        transformation.scale(1, -1).rotate(90);
    } else if (orientation == Exif::Orientation::Rotate270CW) {
        transformation.rotate(270);
    } else {
        return pic;
    }

    return pic.transformed(transformation);
}

QPixmap fromImageReader(QImageReader *reader, Exif::Orientation orientation)
{
    return fromImageReader(reader, 0, 0, orientation);
}

QPixmap fromImageReader(QImageReader *reader, int width, int height)
{
    if (width == 0 || height == 0)
        return QPixmap::fromImageReader(reader);

    QSize size = reader->size();

    double dw = 1.0 * width / size.width();
    double dh = 1.0 * height / size.height();
    QSize cropped_size = size * std::max(dw, dh);
    reader->setScaledSize(cropped_size);
    reader->setScaledClipRect(QRect((cropped_size.width() - width) / 2,
                                    (cropped_size.height() - height) / 2,
                                    width,
                                    height));
    return QPixmap::fromImageReader(reader);
}

} // namespace Pics

