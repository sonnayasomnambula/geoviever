#include <QBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeView>

#include "exif/utils.h"

#include "coordeditdialog.h"
#include "model.h"

CoordEditDialog::CoordEditDialog(QWidget* parent)
    : Super(parent)
    , mView(new QTreeView(this))
    , mModel(new CoordEditModel(this))
    , mRevert(new QPushButton(tr("Revert"), this))
    , mApply(new QPushButton(tr("Apply"), this))
{
    mView->setModel(mModel);
    mView->setIndentation(0);

    mApply->setShortcut(Qt::Key_F2);

    connect(mApply, &QPushButton::clicked, this, &CoordEditDialog::apply);
    connect(mRevert, &QPushButton::clicked, this, &CoordEditDialog::revert);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto lay = new QVBoxLayout(this);
    auto blay = new QHBoxLayout;

    blay->setContentsMargins(11,6,11,6);
    blay->setSpacing(6);

    lay->setContentsMargins({});
    lay->setSpacing(0);

    blay->addWidget(mRevert);
    blay->addStretch();
    blay->addWidget(mApply);

    lay->addWidget(mView);
    lay->addLayout(blay);

    setWindowTitle(tr("Coords"));
}

QAbstractButton* CoordEditDialog::button(Button button) const
{
    switch (button)
    {
    case Button::Revert:
        return mRevert;
    case Button::Apply:
        return mApply;
    }

    return nullptr;
}
