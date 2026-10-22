// =================================================================
// tests/tst_scoped_child_widget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   tests/tst_scoped_child_widget.cpp [@383f058], original header from
//   Longpath source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22  Ported for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted transformation via Claude Code. The four
//               helper cases are Longpath's; NereusSDR adds two cases that
//               close a real widget's window while its right-click menu
//               is open (RotorDialWidget, VfoWidget). Before the helper,
//               both ended the test process: the menu lived on the stack
//               and was freed twice.
// =================================================================
//
// --- From tst_scoped_child_widget.cpp ---
//
// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_scoped_child_widget.cpp  (Longpath)
// =================================================================
//
// Am 2026-08-30 stuerzte Longpath mit "pointer being freed was not
// allocated" ab: der Betreiber hatte ein Kontextmenue offen (ein
// STAPEL-lokales QMenu mitten in seinem eigenen exec()) und schloss
// dabei das Fenster. Die Aufraeumschleife im closeEvent sammelte das
// Menue als Top-Level-Fenster ein und rief deleteLater() darauf —
// "delete this" auf eine Stapeladresse.
//
// Damals wurde die Schleife entschaerft. Die Ursache — ein Qt-Kind auf
// dem Stapel, das seine eigene Ereignisschleife dreht — blieb. Dieser
// Prüfstand haelt fest, was ScopedChildWidget dagegen leistet:
// normales Aufraeumen, und Aufraeumen NACHDEM das Elternteil das Kind
// schon mitgenommen hat. Mit einem rohen Zeiger statt des QPointer
// faellt der zweite Fall in ein use-after-free.
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <memory>

#include "gui/ScopedChildWidget.h"
#include "gui/widgets/RotorDialWidget.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

// NereusSDR: how often and how long the close-while-open cases look for
// the menu before giving up (the menu is then closed so exec() returns).
constexpr int kPollIntervalMs = 10;
constexpr int kMaxPolls = 300;

// NereusSDR: opens `target`'s right-click menu the way Qt delivers it and,
// once the menu's own event loop is running, deletes `window` (the
// target's top level), the way a closed pan or a torn-down floating window
// takes its children with it. Returns true when the menu was seen open.
bool closeWindowWhileMenuIsOpen(QWidget* window, QWidget* target)
{
    QPointer<QWidget> watchWindow(window);
    bool sawMenu = false;
    int polls = 0;
    QTimer poll;
    poll.setInterval(kPollIntervalMs);
    QObject::connect(&poll, &QTimer::timeout, &poll, [&]() {
        ++polls;
        if (qobject_cast<QMenu*>(QApplication::activePopupWidget()) != nullptr) {
            sawMenu = true;
            poll.stop();
            delete watchWindow.data();
            return;
        }
        if (polls >= kMaxPolls) {
            poll.stop();
            if (QWidget* popup = QApplication::activePopupWidget()) {
                popup->close();
            }
        }
    });
    poll.start();

    const QPoint local(10, 10);
    QContextMenuEvent ev(QContextMenuEvent::Mouse, local, target->mapToGlobal(local));
    QApplication::sendEvent(target, &ev);
    poll.stop();
    return sawMenu;
}

}  // namespace

class TstScopedChildWidget : public QObject {
    Q_OBJECT

private slots:

    // From Longpath tests/tst_scoped_child_widget.cpp:33-44 [@383f058]
    void normalScopeExitDestroysTheChild()
    {
        QWidget parent;
        QPointer<QMenu> watch;
        {
            ScopedChildWidget<QMenu> owner(&parent);
            watch = owner.get();
            QVERIFY(watch != nullptr);
            QVERIFY(bool(owner));
            QCOMPARE(watch->parentWidget(), &parent);
        }
        QVERIFY2(watch.isNull(), "The child outlived the scope");
    }

    // From Longpath tests/tst_scoped_child_widget.cpp:46-63 [@383f058]
    void theParentMayTakeTheChildFirst()
    {
        QPointer<QMenu> watch;
        {
            auto parent = std::make_unique<QWidget>();
            ScopedChildWidget<QMenu> owner(parent.get());
            watch = owner.get();
            QVERIFY(watch != nullptr);

            // Genau der Fall von 2026-08-30: das Elternteil stirbt,
            // waehrend der Halter noch im Rahmen steht.
            parent.reset();

            QVERIFY2(watch.isNull(), "Qt did not destroy the child with its parent");
            QVERIFY2(!owner, "The owner still holds a widget that is gone");
            // Und jetzt der Rahmenausgang: er darf NICHT ein zweites Mal
            // freigeben. Mit einem rohen Zeiger endet der Lauf hier.
        }
        QVERIFY(watch.isNull());
    }

    // Der Halter ist zugleich die Auskunft "hat mein Elternteil das
    // ueberlebt?" — genau die Frage, die jede Stelle nach exec() stellt,
    // bevor sie wieder an `this` fasst.
    // From Longpath tests/tst_scoped_child_widget.cpp:65-76 [@383f058]
    void theOwnerAnswersWhetherTheParentSurvived()
    {
        auto parent = std::make_unique<QWidget>();
        ScopedChildWidget<QMenu> owner(parent.get());
        QVERIFY2(bool(owner), "A freshly built owner must hold its widget");
        parent.reset();
        QVERIFY2(!owner, "After the parent dies the owner must report empty");
    }

    // From Longpath tests/tst_scoped_child_widget.cpp:78-87 [@383f058]
    void theChildStaysUsableWhileTheParentLives()
    {
        QWidget parent;
        ScopedChildWidget<QMenu> owner(&parent);
        QMenu& menu = *owner.get();
        QAction* a = menu.addAction(QStringLiteral("Test"));
        QVERIFY(a != nullptr);
        QCOMPARE(menu.actions().size(), 1);
        QCOMPARE(menu.actions().first(), a);
    }

    // NereusSDR: a real right-click menu whose window is closed while it is
    // open. With the menu on the stack this ended the process (double free).
    void rotorDialSurvivesItsWindowClosingDuringTheMenu()
    {
        auto window = std::make_unique<QWidget>();
        QPointer<QWidget> watchWindow(window.get());
        auto* dial = new RotorDialWidget(window.get());
        dial->resize(200, 200);
        window->resize(240, 240);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QWidget* raw = window.release();  // the poll deletes it mid-menu
        const bool sawMenu = closeWindowWhileMenuIsOpen(raw, dial);
        if (!watchWindow.isNull()) {
            delete watchWindow.data();
        }
        QVERIFY2(sawMenu, "The dial's right-click menu never opened");
        QVERIFY(watchWindow.isNull());
    }

    // NereusSDR: the same for the VFO flag's right-click menu, a
    // NereusSDR-original menu converted to the same owner.
    void vfoFlagSurvivesItsWindowClosingDuringTheMenu()
    {
        auto window = std::make_unique<QWidget>();
        QPointer<QWidget> watchWindow(window.get());
        auto* flag = new VfoWidget(window.get());
        window->resize(400, 300);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));

        QWidget* raw = window.release();  // the poll deletes it mid-menu
        const bool sawMenu = closeWindowWhileMenuIsOpen(raw, flag);
        if (!watchWindow.isNull()) {
            delete watchWindow.data();
        }
        QVERIFY2(sawMenu, "The VFO flag's right-click menu never opened");
        QVERIFY(watchWindow.isNull());
    }
};

QTEST_MAIN(TstScopedChildWidget)
#include "tst_scoped_child_widget.moc"
