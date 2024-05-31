#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QString>
#include <QList>
#include <QSet>

namespace QtCompat
{

#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    const auto KeepEmptyParts = QString::KeepEmptyParts;
    const auto SkipEmptyParts = QString::SkipEmptyParts;
#else
    const auto KeepEmptyParts = Qt::KeepEmptyParts;
    const auto SkipEmptyParts = Qt::SkipEmptyParts;
#endif

    template <typename T>
    QList<T> toList(const QSet<T>& values)
    {
#if (QT_VERSION < QT_VERSION_CHECK(5,14,0))
        return values.toList();
#else
        return values.values();
#endif
    }

    template <typename T>
    QSet<T> toSet(const QList<T>& values)
    {
#if (QT_VERSION < QT_VERSION_CHECK(5,14,0))
        return values.toSet();
#else
        return QSet<T>(values.cbegin(), values.cend());
#endif
    }

} // namespace QtCompat

#endif // QTCOMPAT_H
