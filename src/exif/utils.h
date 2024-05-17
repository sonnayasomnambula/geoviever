#ifndef EXIF_UTILS_H
#define EXIF_UTILS_H


#include <QVariant>
#include <QVector>
#include <QPair>

#include <libexif/exif-tag.h>
#include <libexif/exif-utils.h>


class QGeoCoordinate;
class QPixmap;
class QString;


namespace Exif {

namespace Utils {

QVector<ExifRational> toDMS(double degrees, unsigned precision = 10000);
QVector<ExifRational> toSingleRational(double value, unsigned precision = 1000);
QByteArray toLatitudeRef(double lat);
QByteArray toLongitudeRef(double lon);
QByteArray toAltitudeRef(double alt);

QGeoCoordinate fromLatLon(const QVector<ExifRational>& lat, const QByteArray& latRef,
                          const QVector<ExifRational>& lon, const QByteArray& lonRef);
double fromSingleRational(const QVector<ExifRational>& rational, const QByteArray& ref);
QPointF fromLatLon(const QVariantList& latVal, const QByteArray& latRef,
                   const QVariantList& lonVal, const QByteArray& lonRef);

} // namespace Utils


} // namespace Exif

#endif // EXIF_UTILS_H
