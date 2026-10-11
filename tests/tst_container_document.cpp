// no-port-check: NereusSDR-original presentation document contract tests.
// Modification history (NereusSDR):
//   2026-10-11 - locked is written only when true, and a document without
//                 the key loads unlocked. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-11 - screen round trip, absent and malformed, and a floating
//                 container's unreadable geometry. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11 - geometryPane round trip, absent and malformed. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Container edge and workspace arrangement round trips for
//                 the workspace preview by J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <limits>
#include <optional>
#include "gui/containers/ContainerDocumentCodec.h"

using namespace NereusSDR;

class TstContainerDocument : public QObject {
    Q_OBJECT
private:
    static WorkspaceDocument sample()
    {
        WorkspaceDocument doc;
        doc.revision = std::numeric_limits<quint64>::max();
        doc.mainContainerId = QStringLiteral("main");
        doc.extensions = {{"futureWorkspace", QJsonArray{1, "opaque", QJsonObject{{"flag", true}}}}};
        ContainerDocument container;
        container.id = doc.mainContainerId;
        container.name = QStringLiteral("RX | A\n日本語");
        container.layout = ContentLayout::VerticalStack;
        container.dockMode = DockMode::Floating;
        container.anchor = AxisLock::BottomLeft;
        container.header = HeaderMode::Hidden;
        container.geometry = QRect(-400, 220, 730, 540);
        container.visible = false;
        container.locked = true;
        container.autoHeight = true;
        container.popOutShell = true;
        container.config = {{"rxPolicy", "both"}, {"splitter", "opaque bytes"}};
        container.extensions = {{"futureContainer", QJsonObject{{"newMode", 4}}}};
        ContentEntry entry;
        entry.id = QStringLiteral("meter-1");
        entry.typeId = QStringLiteral("unknown.future.meter");
        entry.name = QStringLiteral("Mic | complete\nface");
        entry.context = {{"rxSource", 3}, {"sliceId", "slice-C"}, {"sessionId", "session-one"},
                         {"mmio", QJsonObject{{"endpoint", "amp"}, {"expression", "a|b"}}}};
        entry.config = {{"legacyRecord", "line|with|tails"}, {"futureStyle", QJsonArray{true, 2, "blue"}}};
        entry.extensions = {{"futureContent", QJsonObject{{"value", QJsonValue::Null}}}};
        entry.canvasRect = QRectF(-3.25, 2.5, 0.665, 0.99);
        entry.paintOrder = -3;
        entry.visible = false;
        entry.returnLocation = ReturnLocation{QStringLiteral("deleted-destination"),
                                              QStringLiteral("before"), QStringLiteral("after"),
                                              {{"futurePlacement", QJsonArray{1, "x"}}}};
        container.contents.append(entry);
        entry.id = QStringLiteral("meter-2");
        entry.name = container.contents.first().name; // Names need not be unique; IDs do.
        entry.paintOrder = 7;
        entry.returnLocation.reset();
        container.contents.prepend(entry);
        doc.containers.append(container);
        return doc;
    }
private slots:
    void roundTripPreservesEveryValue()
    {
        const WorkspaceDocument doc = sample();
        const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(int(DockMode::PanelDocked), 0);
        QCOMPARE(int(DockMode::OverlayDocked), 1);
        QCOMPARE(int(DockMode::Floating), 2);
        QCOMPARE(int(AxisLock::BottomLeft), 7);
    }
    void lockedIsWrittenOnlyWhenTrue()
    {
        const auto containerObject = [](const WorkspaceDocument& doc) {
            return QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object()["containers"].toArray().first().toObject();
        };
        WorkspaceDocument doc = sample();
        QVERIFY(doc.containers[0].locked);
        QJsonObject written = containerObject(doc);
        QVERIFY(written.contains("locked"));
        QCOMPARE(written["locked"], QJsonValue(true));
        DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(decoded.document.containers[0].locked);

        doc.containers[0].locked = false;
        written = containerObject(doc);
        QVERIFY(!written.contains("locked"));
        QVERIFY(written.contains("autoHeight"));
        // Without the key the container loads unlocked.
        decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers[0].locked);
        QCOMPARE(decoded.document, doc);

        // A document that carries "locked": false, as older builds wrote it,
        // still loads, unlocked.
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["locked"] = false;
        containers[0] = first;
        root["containers"] = containers;
        decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson(QJsonDocument::Compact));
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
    }
    void freeCanvasKeepsPrecisePlacementAndLegacyRecord()
    {
        auto doc=sample();
        doc.containers[0].layout=static_cast<ContentLayout>(2);
        auto& entry=doc.containers[0].contents[0];
        entry.extensions["freeCanvasRect"]=QJsonArray{-17.1234567890123,22.9876543210987,460.123456789012,113.987654321098};
        const auto decoded=ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
        QVERIFY2(decoded.ok,qPrintable(decoded.error));
        QCOMPARE(decoded.document,doc);
        QCOMPARE(int(ContentLayout::LegacyCanvas),0);
        QCOMPARE(int(ContentLayout::VerticalStack),1);
    }
    void unknownFieldsKeepTheirOriginalNesting()
    {
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        const QJsonObject original = QJsonDocument::fromJson(json).object();
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY(decoded.ok);
        QCOMPARE(QJsonDocument::fromJson(ContainerDocumentCodec::encode(decoded.document)).object(), original);
        QVERIFY(original.contains("futureWorkspace"));
        const QJsonObject container = original["containers"].toArray().first().toObject();
        QVERIFY(container.contains("futureContainer"));
        const QJsonObject entry = container["contents"].toArray().at(1).toObject();
        QVERIFY(entry.contains("futureContent"));
        QVERIFY(entry["returnLocation"].toObject().contains("futurePlacement"));
    }
    void duplicateIdsAreRejected()
    {
        WorkspaceDocument doc = sample();
        doc.containers.append(doc.containers.first());
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        QVERIFY(!ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc)).ok);
        doc = sample();
        doc.containers[0].contents[1].id = doc.containers[0].contents[0].id;
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        ContainerDocument other;
        other.id = "other";
        other.contents.append(doc.containers[0].contents[0]);
        doc.containers.append(other);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
    }
    void invalidValuesAreRejected()
    {
        WorkspaceDocument doc = sample();
        doc.mainContainerId = "missing";
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.containers[0].header = static_cast<HeaderMode>(7);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.containers[0].contents[0].canvasRect.setWidth(-1);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.containers[0].contents[0].canvasRect.setX(std::numeric_limits<double>::infinity());
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
        doc = sample();
        doc.extensions.insert("revision", 99); // Never silently discard a colliding extension.
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
    }
    void malformedOrFutureDataIsRejected_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("truncated") << QByteArray("{\"schemaVersion\":1");
        QTest::newRow("not-object") << QByteArray("[]");
        QTest::newRow("missing-version") << QByteArray("{}");
        QTest::newRow("future") << QByteArray("{\"schemaVersion\":2,\"future-format\":[1,2]}");
        QTest::newRow("fractional-version") << QByteArray("{\"schemaVersion\":1.5}");
        QTest::newRow("invalid-revision") << QByteArray("{\"schemaVersion\":1,\"revision\":\"-1\",\"mainContainerId\":\"\",\"containers\":[]}");
        QTest::newRow("wrong-containers") << QByteArray("{\"schemaVersion\":1,\"revision\":\"0\",\"mainContainerId\":\"\",\"containers\":{}}");
    }
    void malformedOrFutureDataIsRejected()
    {
        QFETCH(QByteArray, json);
        const DocumentResult result = ContainerDocumentCodec::decode(json);
        QVERIFY(!result.ok);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.document.containers.isEmpty());
    }
    void wrongKnownFieldTypeIsNotDefaulted()
    {
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        QJsonArray containers = root["containers"].toArray();
        QVERIFY(!containers.isEmpty());
        QJsonObject container = containers.first().toObject();
        container["visible"] = "False";
        containers[0] = container;
        root["containers"] = containers;
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
    }
    void geometryPaneRoundTripsForAFreeContainer()
    {
        WorkspaceDocument doc = sample();
        doc.containers[0].dockMode = DockMode::OverlayDocked;
        doc.containers[0].geometryPane = QSize(1185, 872);
        const QByteArray json = ContainerDocumentCodec::encode(doc);
        const QJsonObject container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["geometryPane"].toArray(), (QJsonArray{1185, 872}));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(decoded.document.containers.first().geometryPane, std::optional<QSize>(QSize(1185, 872)));
        QVERIFY(!decoded.document.containers.first().extensions.contains("geometryPane"));
    }
    void geometryPaneAbsentStaysAbsent()
    {
        // A document from before the field: read as empty, written without it.
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("geometryPane"));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers.first().geometryPane.has_value());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
        // Written for free containers only.
        WorkspaceDocument floating = sample();
        floating.containers[0].geometryPane = QSize(1000, 700);
        QCOMPARE(floating.containers[0].dockMode, DockMode::Floating);
        QVERIFY(!ContainerDocumentCodec::encode(floating).contains("geometryPane"));
    }
    void malformedGeometryPaneIsDroppedForThatContainerOnly_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("string") << QJsonValue(QStringLiteral("1000x700"));
        QTest::newRow("three numbers") << QJsonValue(QJsonArray{1000, 700, 1});
        QTest::newRow("zero width") << QJsonValue(QJsonArray{0, 700});
        QTest::newRow("fraction") << QJsonValue(QJsonArray{1000.5, 700});
        QTest::newRow("object") << QJsonValue(QJsonObject{{"w", 1000}});
    }
    void malformedGeometryPaneIsDroppedForThatContainerOnly()
    {
        QFETCH(QJsonValue, value);
        WorkspaceDocument doc = sample();
        doc.containers[0].dockMode = DockMode::OverlayDocked;
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::OverlayDocked;
        other.geometry = QRect(10, 20, 300, 200);
        other.geometryPane = QSize(900, 600);
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["geometryPane"] = value;
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable geometryPane")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers[0].geometryPane.has_value());
        QVERIFY(!decoded.document.containers[0].extensions.contains("geometryPane"));
        QCOMPARE(decoded.document.containers[1].geometryPane, std::optional<QSize>(QSize(900, 600)));
    }
    void screenRoundTripsForAWindow()
    {
        WorkspaceDocument doc = sample();
        QCOMPARE(doc.containers[0].dockMode, DockMode::Floating);
        doc.containers[0].screen = SavedScreen{QStringLiteral("DELL U2720Q"), QStringLiteral("SN-7"),
                                               QRect(1920, 0, 2560, 1440)};
        const QByteArray json = ContainerDocumentCodec::encode(doc);
        const QJsonObject container = QJsonDocument::fromJson(json).object()["containers"].toArray().first().toObject();
        QCOMPARE(container["screen"].toObject(),
                 (QJsonObject{{"name", "DELL U2720Q"}, {"serial", "SN-7"},
                              {"geometry", QJsonArray{1920, 0, 2560, 1440}}}));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(decoded.document.containers.first().screen, doc.containers[0].screen);
        QVERIFY(!decoded.document.containers.first().extensions.contains("screen"));
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void screenAbsentStaysAbsent()
    {
        // A document from before the field: read as empty, written without it.
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("\"screen\""));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QVERIFY(!decoded.document.containers.first().screen.has_value());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
        // Written for floating and popped-out containers only.
        WorkspaceDocument docked = sample();
        docked.containers[0].dockMode = DockMode::OverlayDocked;
        docked.containers[0].popOutShell = false;
        docked.containers[0].screen = SavedScreen{QStringLiteral("A"), QString(), QRect(0, 0, 1920, 1080)};
        QVERIFY(!ContainerDocumentCodec::encode(docked).contains("\"screen\""));
        docked.containers[0].popOutShell = true;
        QVERIFY(ContainerDocumentCodec::encode(docked).contains("\"screen\""));
    }
    void malformedScreenIsDroppedForThatContainerOnly_data()
    {
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("string") << QJsonValue(QStringLiteral("DELL U2720Q"));
        QTest::newRow("array") << QJsonValue(QJsonArray{1920, 0, 2560, 1440});
        QTest::newRow("no geometry") << QJsonValue(QJsonObject{{"name", "A"}, {"serial", ""}});
        QTest::newRow("geometry of three") << QJsonValue(QJsonObject{{"name", "A"}, {"serial", ""},
                                                                      {"geometry", QJsonArray{0, 0, 1920}}});
        QTest::newRow("empty geometry") << QJsonValue(QJsonObject{{"name", "A"}, {"serial", ""},
                                                                   {"geometry", QJsonArray{0, 0, 0, 0}}});
        QTest::newRow("name not a string") << QJsonValue(QJsonObject{{"name", 7}, {"serial", ""},
                                                                      {"geometry", QJsonArray{0, 0, 1920, 1080}}});
    }
    void malformedScreenIsDroppedForThatContainerOnly()
    {
        QFETCH(QJsonValue, value);
        WorkspaceDocument doc = sample();
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::Floating;
        other.geometry = QRect(2100, 200, 400, 300);
        other.screen = SavedScreen{QStringLiteral("B"), QString(), QRect(1920, 0, 1920, 1080)};
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["screen"] = value;
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable screen")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QVERIFY(!decoded.document.containers[0].screen.has_value());
        QVERIFY(!decoded.document.containers[0].extensions.contains("screen"));
        QCOMPARE(decoded.document.containers[1].screen, other.screen);
    }
    void unreadableGeometryCostsAWindowItsPlaceAndNothingElse()
    {
        WorkspaceDocument doc = sample();
        ContainerDocument other;
        other.id = QStringLiteral("other");
        other.dockMode = DockMode::Floating;
        other.geometry = QRect(2100, 200, 400, 300);
        doc.containers.append(other);
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(doc)).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject first = containers.first().toObject();
        first["geometry"] = QStringLiteral("100,200,300,400");
        containers[0] = first;
        root["containers"] = containers;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("unreadable geometry; it takes a default place")));
        const DocumentResult decoded = ContainerDocumentCodec::decode(QJsonDocument(root).toJson());
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document.containers.size(), 2);
        QVERIFY(!decoded.document.containers[0].geometry.isValid());
        WorkspaceDocument expected = doc;
        expected.containers[0].geometry = QRect();
        QCOMPARE(decoded.document, expected);
        QCOMPARE(decoded.document.containers[1].geometry, other.geometry);
        // A docked or free container's geometry is still a known field
        // that is never defaulted.
        first["dockMode"] = int(DockMode::OverlayDocked);
        containers[0] = first;
        root["containers"] = containers;
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
    }
    void documentWithoutEdgesStaysByteIdentical()
    {
        // A document from before edges: every container on the right, no
        // arrangement. Neither member is written, so it re-encodes exactly.
        const QByteArray json = ContainerDocumentCodec::encode(sample());
        QVERIFY(!json.contains("\"edge\""));
        QVERIFY(!json.contains("\"arrangement\""));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document.containers.first().edge, DockEdge::Right);
        QVERIFY(decoded.document.arrangement.isEmpty());
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void edgeAndArrangementRoundTrip()
    {
        QCOMPARE(int(DockEdge::Left), 0);
        QCOMPARE(int(DockEdge::Right), 1);
        QCOMPARE(int(DockEdge::Top), 2);
        QCOMPARE(int(DockEdge::Bottom), 3);
        WorkspaceDocument doc = sample();
        doc.containers[0].dockMode = DockMode::PanelDocked;
        doc.containers[0].edge = DockEdge::Bottom;
        EdgeStripLayout left;
        left.thickness = 300;
        left.extents = {{"main", 420}, {"other", 210}};
        left.extensions = {{"futureStrip", QJsonArray{1, 2}}};
        doc.arrangement.strips.insert(DockEdge::Left, left);
        EdgeStripLayout top;
        top.thickness = 112;
        doc.arrangement.strips.insert(DockEdge::Top, top);
        doc.arrangement.gridAlignment = true;
        doc.arrangement.extensions = {{"futureArrangement", "kept"}};
        const QByteArray json = ContainerDocumentCodec::encode(doc);
        const QJsonObject root = QJsonDocument::fromJson(json).object();
        QCOMPARE(root["containers"].toArray().first().toObject()["edge"].toString(), QStringLiteral("Bottom"));
        QVERIFY(root["arrangement"].toObject()["strips"].toObject().contains("Left"));
        const DocumentResult decoded = ContainerDocumentCodec::decode(json);
        QVERIFY2(decoded.ok, qPrintable(decoded.error));
        QCOMPARE(decoded.document, doc);
        QCOMPARE(ContainerDocumentCodec::encode(decoded.document), json);
    }
    void unknownEdgeIsRejected()
    {
        QJsonObject root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        QJsonArray containers = root["containers"].toArray();
        QJsonObject container = containers.first().toObject();
        for (const QJsonValue& bad : {QJsonValue("Middle"), QJsonValue(2), QJsonValue("left")}) {
            container["edge"] = bad;
            containers[0] = container;
            root["containers"] = containers;
            QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
        }
        root = QJsonDocument::fromJson(ContainerDocumentCodec::encode(sample())).object();
        root["arrangement"] = QJsonObject{{"strips", QJsonObject{{"Middle", QJsonObject{{"thickness", 10}}}}}};
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
        root["arrangement"] = QJsonObject{{"strips", QJsonObject{{"Left", QJsonObject{{"thickness", -4}}}}}};
        QVERIFY(!ContainerDocumentCodec::decode(QJsonDocument(root).toJson()).ok);
        WorkspaceDocument doc = sample();
        doc.containers[0].edge = static_cast<DockEdge>(9);
        QVERIFY(!ContainerDocumentCodec::validate(doc).isEmpty());
    }
};
QTEST_GUILESS_MAIN(TstContainerDocument)
#include "tst_container_document.moc"
