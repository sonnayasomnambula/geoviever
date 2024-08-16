#include <QCheckBox>
#include <QMessageBox>
#include <QSettings>

#include "abstractsettings.h"
#include "dialog.h"

struct DialogSettings : AbstractSettings
{
    struct {
        Tag<bool> overwriteSilently = "dialog/overwriteSilently";
    } dialog;
};

bool Dialog::canOverwrite(int size, QWidget* parent)
{
    DialogSettings settings;
    if (!settings.dialog.overwriteSilently.isNull())
        return settings.dialog.overwriteSilently;

    using QMBox = QMessageBox;
    QMBox box(QMBox::Question, "", tr("Overwrite %n file(s)?", nullptr, size), QMBox::Yes | QMBox::No, parent);
    box.setCheckBox(new QCheckBox(tr("Do not ask me next time")));
    bool can = box.exec() == QMBox::Yes;
    if (box.checkBox()->isChecked())
        settings.dialog.overwriteSilently = can;
    return can;
}
