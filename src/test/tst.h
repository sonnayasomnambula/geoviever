#ifndef TST_H
#define TST_H

#include <QVector>
#include <QString>

#include <libexif/exif-utils.h>

bool operator ==(const ExifRational& L, const ExifRational& R);
void PrintTo(const QVector<ExifRational>& val, ::std::ostream* os);
void PrintTo(const QString& str, ::std::ostream* os);
void PrintTo(const QStringRef& str, ::std::ostream* os);

#endif // TST_H
