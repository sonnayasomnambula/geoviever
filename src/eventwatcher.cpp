#include "eventwatcher.h"

EventWatcher* EventWatcher::watch(QObject* object, QEvent::Type type)
{
    auto watcher = new EventWatcher(object);
    watcher->mWatched = object;
    watcher->mType = type;
    object->installEventFilter(watcher);
    return watcher;
}

bool EventWatcher::eventFilter(QObject* watched, QEvent* event)
{
    if (mWatched == watched && mType == event->type())
        emit caught(event);
    return false;
}
