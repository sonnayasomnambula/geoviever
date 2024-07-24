#include "tst.h"

bool operator ==(const ExifRational& L, const ExifRational& R) {
    return L.numerator == R.numerator && L.denominator == R.denominator;
}

void PrintTo(const QVector<ExifRational>& val, ::std::ostream* os) {
    *os << "{ ";
    for (int i = 0; i < val.size(); ++i) {
        *os << val[i].numerator << "/" << val[i].denominator << (i == val.size() - 1 ? " }" : "; ");
    }
}

void PrintTo(const QString& str, ::std::ostream* os) {
    *os << "\"" << qPrintable(str) << "\"";
}

void PrintTo(const QStringRef& str, std::ostream *os)
{
    *os << "\"" << qPrintable(str.toString()) << "\"";
}
