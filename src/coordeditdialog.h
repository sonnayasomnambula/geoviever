#ifndef COORDEDITDIALOG_H
#define COORDEDITDIALOG_H

#include <QAbstractItemModel>
#include <QDialog>
#include <QSharedPointer>

QT_BEGIN_NAMESPACE
class QTreeView;
class QAbstractButton;
QT_END_NAMESPACE

class CoordEditModel;

struct Photo;

class CoordEditDialog : public QDialog
{
    Q_OBJECT
    using Super = QDialog;

signals:
    void apply();
    void revert();

public:
    explicit CoordEditDialog(QWidget *parent = nullptr);

    void setCoords(const QString& path, const QPointF& coord);

    QTreeView* view() const { return mView; }
    CoordEditModel* model() const { return mModel; }

    enum class Button { Revert, Apply };
    QAbstractButton* button(Button button) const;

private:
    QTreeView* mView = nullptr;
    CoordEditModel* mModel = nullptr;

    QPushButton* mRevert = nullptr;
    QPushButton* mApply = nullptr;
};


#endif // COORDEDITDIALOG_H
