#include <QRegularExpression>

#include <limits>

#include "geocoordinate.h"

QGeoCoordinate GeoCoordinate::fromString(const QString& s, bool* ok)
{
    QGeoCoordinate coord;
    QVector<QStringRef> sl = s.splitRef(", ");
    QStringRef lat, lon, alt;
    if (sl.size() < 2)
        sl = s.splitRef(' ');

    if (sl.size() >= 2)
    {
        lat = sl[0];
        lon = sl[1];
    }

    if (sl.size() == 3)
        alt = sl[2];

    double val;

    bool hasLat = false;
    val = extractLat(lat, &hasLat);
    if (hasLat)
        coord.setLatitude(val);

    bool hasLon = false;
    val = extractLon(lon, &hasLon);
    if (hasLon)
        coord.setLongitude(val);

    bool hasAlt = false;
    val = extractAlt(alt, &hasAlt);
    if (hasAlt)
        coord.setAltitude(val);

    if (ok) *ok = hasLat && hasLon;
    return coord;
}

double GeoCoordinate::extractLat(const QStringRef& s, bool* ok)
{
    return extractValue(s, 'N', 'S', ok);
}

double GeoCoordinate::extractLon(const QStringRef& s, bool* ok)
{
    return extractValue(s, 'E', 'W', ok);
}

double GeoCoordinate::extractAlt(const QStringRef& s, bool* ok)
{
    if (s.isEmpty())
        return failed(ok);

    return s.endsWith('m') ?
               s.left(s.size() - 1).toDouble(ok) :
               s.toDouble(ok);
}

double GeoCoordinate::extractValue(const QStringRef& s, QChar positive, QChar negative, bool* ok)
{
    if (s.isEmpty())
        return failed(ok);

    double sign = 1.0;

    QStringRef sr = s;
    if (sr.endsWith(positive))
    {
        sign = 1.0;
        sr = sr.chopped(1).trimmed();
    }
    else if (sr.endsWith(negative))
    {
        sign = -1.0;
        sr = sr.chopped(1).trimmed();
    }

    QVector<QStringRef> parts = split(sr, { L' ', L'°', L'\'', L'"' });

    QStringRef dd, mm, ss;

    if (parts.size() >= 1)
        dd = parts[0];

    if (parts.size() >= 2)
        mm = parts[1];

    if (parts.size() == 3)
        ss = parts[2];

    double value = std::numeric_limits<double>::quiet_NaN();

    {
        bool hasDD = false;
        double d = dd.toDouble(&hasDD);
        if (hasDD)
            value = d;
        else
            return failed(ok, value);

        if (value < 0)
        {
            value = -value;
            sign = -1.0;
        }
    }

    {
        bool hasMM = false;
        double m = mm.toDouble(&hasMM);
        if (hasMM)
            value += m / 60;
    }

    {
        bool hasSS = false;
        double s = ss.toDouble(&hasSS);
        if (hasSS)
            value += s / 3600;
    }

    return failed(ok, sign * value, true); // TODO rename failed or make an alias
}

// there is no QStringRef::split(QRegularExpression) function
QVector<QStringRef> GeoCoordinate::split(const QStringRef& s, const QVector<QChar>& chars)
{
    QVector<QStringRef> parts;
    int b = 0;
    for (int i = 0; i < s.size(); ++i)
    {
        QChar ch = s[i];
        if (chars.contains(ch))
        {
            if (b < i)
            {
                QStringRef part = s.mid(b, i - b);
                parts.append(part);
            }

            b = i + 1;
        }
    }

    if (b < s.size())
        parts.append(s.mid(b));

    return parts;
}
