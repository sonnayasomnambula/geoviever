#ifndef GEOCOORDINATE_H
#define GEOCOORDINATE_H

#include <QGeoCoordinate>

class GeoCoordinate : public QGeoCoordinate
{
    friend class GeoCoordinate_split_Test;

public:
    static QGeoCoordinate fromString(const QString& s, bool* ok = nullptr);

private:
    static double extractLat(const QStringRef& s, bool* ok);
    static double extractLon(const QStringRef& s, bool* ok);
    static double extractAlt(const QStringRef& s, bool* ok);
    static double extractValue(const QStringRef& s, QChar positive, QChar negative, bool* ok);

    static QVector<QStringRef> split(const QStringRef& s, const QVector<QChar>& chars);

    template <typename T = bool>
    static T failed(bool* ok, T result = T(), bool status = false) {
        if (ok) *ok = status;
        return result;
    }
};

#endif // GEOCOORDINATE_H
