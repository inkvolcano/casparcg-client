// Which key presses in an Inspector field may start an undo step.

#include "../src/Common/InspectorUndoRules.h"

#include <QtCore/QString>
#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Inspector undo\n";

    using InspectorUndoRules::keyCanEdit;

    expectTrue(keyCanEdit(Qt::Key_A) && keyCanEdit(Qt::Key_5) && keyCanEdit(Qt::Key_Space), "typing can edit");
    expectTrue(keyCanEdit(Qt::Key_Backspace) && keyCanEdit(Qt::Key_Delete), "deleting text can edit");
    expectTrue(keyCanEdit(Qt::Key_Up) && keyCanEdit(Qt::Key_Down), "arrows step a spin box");
    expectTrue(keyCanEdit(Qt::Key_Return) && keyCanEdit(Qt::Key_Enter), "Enter confirms a field");
    expectTrue(!keyCanEdit(Qt::Key_F1) && !keyCanEdit(Qt::Key_F2) && !keyCanEdit(Qt::Key_F12), "playout keys never start one");
    expectTrue(!keyCanEdit(Qt::Key_Escape), "nor the panic key");
    expectTrue(!keyCanEdit(Qt::Key_Tab) && !keyCanEdit(Qt::Key_Backtab), "nor moving between fields");
    expectTrue(!keyCanEdit(Qt::Key_Shift) && !keyCanEdit(Qt::Key_Control) && !keyCanEdit(Qt::Key_Alt), "nor a bare modifier");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
