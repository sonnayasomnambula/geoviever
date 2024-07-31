#ifndef THREADSAFE_HPP
#define THREADSAFE_HPP

#include <QHash>
#include <QSet>
#include <QMutex>

template <typename T>
class ThreadSafeSet : private QSet<T>
{
    using Super = QSet<T>;
    mutable QMutex mMutex;

public:
    bool insert(const T& value) {
        QMutexLocker lock(&mMutex);
        int sz = Super::size();
        Super::insert(value);
        return Super::size() > sz;
    }

    void remove(const T& value) {
        QMutexLocker lock(&mMutex);
        Super::remove(value);
    }

    void clear() {
        QMutexLocker lock(&mMutex);
        Super::clear();
    }

    T takeFirst() {
        QMutexLocker lock(&mMutex);
        auto first = Super::cbegin();
        if (first == Super::cend())
            return T();
        T value = *first;
        Super::erase(first);
        return value;
    }

    int size() const {
        QMutexLocker lock(&mMutex);
        return Super::size();
    }

    bool contains(const T& value) const {
        QMutexLocker lock(&mMutex);
        return Super::contains(value);
    }
};

template <typename KeyType, typename ValueType>
class ThreadSafeHash : private QHash<KeyType, ValueType>
{
    using Super = QHash<KeyType, ValueType>;
    mutable QMutex mMutex;

public:
    bool insert(const KeyType& key, const ValueType& value) {
        QMutexLocker lock(&mMutex);
        int sz = Super::size();
        Super::insert(key, value);
        return Super::size() > sz;
    }

    void remove(const KeyType& key) {
        QMutexLocker lock(&mMutex);
        Super::remove(key);
    }

    void clear() {
        QMutexLocker lock(&mMutex);
        Super::clear();
    }

    ValueType takeFirst() {
        QMutexLocker lock(&mMutex);
        if (Super::isEmpty()) return {} /*ValueType()*/;
        auto first = Super::cbegin();
        ValueType value = first.value();
        Super::erase(first);
        return value;
    }

    int size() const {
        QMutexLocker lock(&mMutex);
        return Super::size();
    }
};

#endif // THREADSAFE_HPP
