#include "exif/utils.h"

#include <QDebug>
#include <QGeoCoordinate>
#include <QPointF>
#include <QString>

#include <cmath>
#include <string>


QDebug operator<<(QDebug debug, const ExifRational& urational)
{
    debug << "ExifRational(" << urational.numerator << '/' << urational.denominator << ')';
    return debug;
}


/// latitude and longitude are stored in degrees, minutes and seconds
/// each one as rational value (numerator and denominator)
/// \param degrees      number of degrees as floating point to convert
/// \param precision    integer value used as ExifRational denominator
QVector<ExifRational> Exif::Utils::toDMS(double degrees, unsigned precision)
{
    quint32 d = degrees;
    quint32 m = static_cast<quint32>(degrees * 60) % 60;
    quint32 s = static_cast<quint32>(std::round(degrees * 60 * 60 * precision)) % (60 * precision);
    return { { d, 1 }, { m, 1 }, { s, precision } };
}


/// altitude is stored as single rational value (numerator and denominator)
/// \param value      floating point to convert
/// \param precision    integer value used as ExifRational denominator
QVector<ExifRational> Exif::Utils::toSingleRational(double value, unsigned precision)
{
    // FIXME what about altitude below sea level?
    return { { std::round(value * precision), precision } };
}

QByteArray Exif::Utils::toLatitudeRef(double lat)
{
    return lat >= 0 ? "N" : "S";
}

QByteArray Exif::Utils::toLongitudeRef(double lon)
{
    return lon >= 0 ? "E" : "W";
}

QByteArray Exif::Utils::toAltitudeRef(double /*alt*/)
{
    Q_ASSERT("rewrite, must return Utils::SeaLevel");
    return ""; // FIXME ALTITUDE_REF is BYTE, not ASCII; see tag description in exif-tag.c
}

QGeoCoordinate Exif::Utils::fromLatLon(const QVector<ExifRational>& lat, const QByteArray& latRef, const QVector<ExifRational>& lon, const QByteArray& lonRef)
{
    if (lat.size() != 3 || lon.size() != 3) {
        qWarning() << "Exif: unsupported latlon format" << lat << latRef << lon << lonRef;
        return {};
    }

    class DMS {
        double d, m, s;
    public:
        explicit DMS(const QVector<ExifRational>& value) :
            d(1.0 * value[0].numerator / value[0].denominator),
            m(1.0 * value[1].numerator / value[1].denominator),
            s(1.0 * value[2].numerator / value[2].denominator)
        {}
        double join() { return d + m / 60 + s / 60 / 60; }
    };

    double llat = DMS(lat).join();
    double llon = DMS(lon).join();

    if (latRef == "S")
        llat = -llat;
    if (lonRef == "W")
        llon = -llon;

    return QGeoCoordinate(llat, llon);
}

double Exif::Utils::fromSingleRational(const QVector<ExifRational>& rational, const QByteArray& ref)
{
    if (rational.size() != 1) {
        qWarning() << "Exif: unsupported altitude format" << rational << ref;
        return 0.;
    }

    double alt = 1.0 * rational.first().numerator / rational.first().denominator;
    if (!ref.isEmpty()) // TODO check this
        alt = -alt;

    return alt;
}

QPointF Exif::Utils::fromLatLon(const QVariantList &latVal, const QByteArray &latRef, const QVariantList &lonVal, const QByteArray &lonRef)
{
    if (latVal.size() != 3 || lonVal.size() != 3) {
        qWarning() << "Exif: unsupported latlon format" << latVal << latRef << lonVal << lonRef;
        return {}; // 3
    }

    double lat = latVal[0].toDouble() + latVal[1].toDouble() / 60 + latVal[2].toDouble() / 60 / 60;
    double lon = lonVal[0].toDouble() + lonVal[1].toDouble() / 60 + lonVal[2].toDouble() / 60 / 60;

    if (latRef == "S")
        lat = -lat;
    if (lonRef == "W")
        lon = -lon;

    return QPointF(lat, lon);
}
