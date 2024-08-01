#ifndef EVENTWATCHER_H
#define EVENTWATCHER_H

#include <QEvent>
#include <QObject>

/**
 * A class to catch events in QObject instances.
 *
 * @code
 * connect(EventWatcher::watch(mLineEdit, QEvent::FocusIn), &EventWatcher::caught,
 *      []{ qDebug() << "QEvent::FocusIn caught!"; });
 * @endcode
 */
class EventWatcher : public QObject
{
    Q_OBJECT

public:
    static EventWatcher* watch(QObject* object, QEvent::Type type);

signals:
    void caught(const QEvent* event);

private:
    using QObject::QObject;
    bool eventFilter(QObject* watched, QEvent* event) override;

    QEvent::Type mType = QEvent::None;
    QObject* mWatched = nullptr;
};

#endif // EVENTWATCHER_H
