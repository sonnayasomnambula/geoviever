#ifndef TOOLTIP_H
#define TOOLTIP_H

#include <QTableView>

class ToolTip : public QTableView
{
    int mTimerId = 0;

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
    void enterEvent(QEvent*) override;
    void leaveEvent(QEvent*) override;
    void hideEvent(QHideEvent*) override;
    void keyPressEvent(QKeyEvent* e) override;
    void timerEvent(QTimerEvent* e) override;

public:
    enum { FilePathRole = Qt::UserRole };

    explicit ToolTip(QWidget* parent = nullptr);
    void setFiles(const QStringList& files);

    QAbstractItemModel* model() const { return mModel; }
};

#endif // TOOLTIP_H
