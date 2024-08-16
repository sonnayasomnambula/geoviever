#ifndef DIALOG_H
#define DIALOG_H

#include <QCoreApplication>

class QWidget;

class Dialog
{
    Q_DECLARE_TR_FUNCTIONS(Dialog)

public:
    static bool canOverwrite(int size, QWidget* parent);
};

#endif // DIALOG_H
