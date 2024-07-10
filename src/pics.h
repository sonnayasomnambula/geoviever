#ifndef PICS_H
#define PICS_H

class QImageReader;
class QPixmap;
class QString;
class QIcon;

namespace Exif { class Orientation; }

namespace Pics
{

QPixmap thumbnail(const QPixmap& pixmap, int size);

QString toBase64(const QPixmap& pixmap, const char* format);

QPixmap fromImageReader(QImageReader* reader, int width, int height, Exif::Orientation orientation);
QPixmap fromImageReader(QImageReader* reader, Exif::Orientation orientation);
QPixmap fromImageReader(QImageReader* reader, int width, int height);

QIcon createIcon(const QPixmap& pix1, const QPixmap& pix2);

} // namespace Pics

#endif // PICS_H
