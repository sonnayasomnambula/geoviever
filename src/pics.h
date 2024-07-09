#ifndef PICS_H
#define PICS_H

class QImageReader;
class QPixmap;
class QString;

namespace Exif { class Orientation; }

namespace Pics
{

QPixmap thumbnail(const QPixmap& pixmap, int size);

QString toBase64(const QPixmap& pixmap, const char* format);
QPixmap fromBase64(const QString& base64);

QPixmap fromImageReader(QImageReader* reader, int width, int height, Exif::Orientation orientation);
QPixmap fromImageReader(QImageReader* reader, Exif::Orientation orientation);
QPixmap fromImageReader(QImageReader* reader, int width, int height);

QPixmap transparent(int w, int h);

} // namespace Pics

#endif // PICS_H
