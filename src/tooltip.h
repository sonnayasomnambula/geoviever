#ifndef TOOLTIP_H
#define TOOLTIP_H

#include <QKeyEvent>
#include <QTableView>
#include <QTimerEvent>
#include <QLabel>

class TooltipUtils
{
public:
    static QRect adjustedRect(const QPoint& pos, const QSize& size, int shift);
};

template <class T>
class AbstractToolTip : public T, public TooltipUtils
{
    int mTimerId = 0;

public:
    AbstractToolTip(QWidget* parent = nullptr) : T(parent) {
        T::setWindowFlags(Qt::ToolTip);
    }

protected:
    void enterEvent(QEvent*) override {
        if (mTimerId)
        {
            T::killTimer(mTimerId);
            mTimerId = 0;
        }
    }

    void leaveEvent(QEvent*) override {
        mTimerId = T::startTimer(600);
    }

    void showEvent(QShowEvent*) override {
        mTimerId = T::startTimer(2100);
    }

    void hideEvent(QHideEvent*) override {
        if (mTimerId)
        {
            T::killTimer(mTimerId);
            mTimerId = 0;
        }
    }

    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape)
            T::hide();
    }

    void timerEvent(QTimerEvent* e) override {
        if (e->timerId() == mTimerId)
            T::hide();
    }
};

class GridToolTip : public AbstractToolTip<QTableView>
{
    class Model : public QAbstractTableModel
    {
        QStringList mData;
        int mRowCount = 0;
        int mColCount = 0;

    public:
        using QAbstractTableModel::QAbstractTableModel;

        int rowCount(const QModelIndex& parent = {}) const override;
        int columnCount(const QModelIndex& parent = {}) const override;
        QVariant data(const QModelIndex& index, int role) const override;

        void setFiles(const QStringList& files);

    } * mModel = new Model(this);

    void moveSelection(int dx, int dy);

protected:
    void keyPressEvent(QKeyEvent* e) override;

public:
    explicit GridToolTip(QWidget* parent = nullptr);
    void setFiles(const QStringList& files);
    void showAt(const QPoint& pos, int shift = 0);
};

class LabelTooltip : public AbstractToolTip<QLabel>
{
public:
    using AbstractToolTip::AbstractToolTip;
    void showAt(const QPoint& pos, int shift = 0);
};

#endif // TOOLTIP_H
