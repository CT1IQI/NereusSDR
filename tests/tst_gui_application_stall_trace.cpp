// no-port-check: NereusSDR-original main-thread stall trace regressions.
// Modification history (NereusSDR):
//   2026-10-22 — J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
#include <QtTest>
#include <QEventLoop>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>

#include "gui/GuiApplication.h"

using namespace NereusSDR;

namespace {

QMutex g_linesMutex;
QStringList g_lines;
QtMessageHandler g_previousHandler = nullptr;

void captureHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (message.startsWith(QLatin1String("MAIN_STALL"))
        || message.startsWith(QLatin1String("MAIN_LATE"))) {
        QMutexLocker locker(&g_linesMutex);
        g_lines.append(message);
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, message);
    }
}

QStringList takeLines()
{
    QMutexLocker locker(&g_linesMutex);
    QStringList lines = g_lines;
    g_lines.clear();
    return lines;
}

// Builds the application with the trace variable as given, posts a queued
// call that sleeps 30 ms, runs the loop until it has run and a little after,
// and returns the trace lines it logged.
QStringList runSlowQueuedCall(const char* traceValue)
{
    if (traceValue != nullptr) {
        qputenv("NEREUS_TRACE_MAIN_STALLS", traceValue);
    } else {
        qunsetenv("NEREUS_TRACE_MAIN_STALLS");
    }
    takeLines();

    int argc = 1;
    char arg0[] = "tst_gui_application_stall_trace";
    char* argv[] = {arg0, nullptr};
    {
        GuiApplication app(argc, argv);
        QObject slowReceiver;
        slowReceiver.setObjectName(QStringLiteral("slowReceiver"));
        QMetaObject::invokeMethod(&slowReceiver, [] {
            QThread::msleep(30);
        }, Qt::QueuedConnection);
        QEventLoop loop;
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
        loop.exec();
    }
    qunsetenv("NEREUS_TRACE_MAIN_STALLS");
    return takeLines();
}

} // namespace

class TestGuiApplicationStallTrace : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        g_previousHandler = qInstallMessageHandler(captureHandler);
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(g_previousHandler);
    }

    void slowDeliveryLogsMainStall()
    {
        const QStringList lines = runSlowQueuedCall("1");
        QStringList stalls;
        for (const QString& line : lines) {
            if (line.startsWith(QLatin1String("MAIN_STALL"))) {
                stalls.append(line);
            }
        }
        QVERIFY2(!stalls.isEmpty(), qPrintable(lines.join(QLatin1Char('\n'))));
        const QRegularExpression pattern(QStringLiteral(
            "^MAIN_STALL ms=\\d+\\.\\d depth=1 type=MetaCall receiver=QObject "
            "name=slowReceiver parent=- wall=\\d\\d:\\d\\d:\\d\\d\\.\\d\\d\\d$"));
        bool matched = false;
        for (const QString& line : stalls) {
            const QRegularExpressionMatch match = pattern.match(line);
            if (match.hasMatch()) {
                matched = true;
                const double ms = line.section(QLatin1String("ms="), 1).section(QLatin1Char(' '), 0, 0).toDouble();
                QVERIFY2(ms >= 12.0, qPrintable(line));
            }
        }
        QVERIFY2(matched, qPrintable(stalls.join(QLatin1Char('\n'))));
    }

    void traceOffLogsNothing()
    {
        const QStringList lines = runSlowQueuedCall(nullptr);
        QVERIFY2(lines.isEmpty(), qPrintable(lines.join(QLatin1Char('\n'))));
    }
};

int main(int argc, char** argv)
{
    TestGuiApplicationStallTrace test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_gui_application_stall_trace.moc"
