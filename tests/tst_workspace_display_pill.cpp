// no-port-check: NereusSDR-original geometry and binding regression for
// the Display pill using the existing approved Longpath-derived workspace look.
// Modification history (NereusSDR):
//   2026-10-12 - J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include <QtTest/QtTest>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QRegularExpression>
#include <QSlider>
#include <QSpinBox>
#include <QStyleOptionSlider>
#include <QStyleOptionSpinBox>
#include <QTextDocument>

#include "core/AppSettings.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspaceComponents.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {
using Tab = SpectrumOverlayPanel::DisplayPillTab;

struct Harness {
    SpectrumWidget pan;
    SpectrumOverlayPanel* panel;
    Harness() : panel(new SpectrumOverlayPanel(&pan)) { pan.resize(900, 600); }
    QWidget* pill() const { return pan.findChild<QWidget*>(QStringLiteral("displayPill")); }
    template<typename T> T* control(const char* name) const {
        return pill()->findChild<T*>(QString::fromLatin1(name));
    }
    void layout() {
        pill()->adjustSize();
        pill()->layout()->setGeometry(pill()->rect());
        for (QWidget* child : pill()->findChildren<QWidget*>()) {
            if (child->layout()) { child->layout()->setGeometry(child->rect()); }
        }
    }
};

QRect handleOf(QSlider* slider) {
    QStyleOptionSlider option;
    option.initFrom(slider);
    option.orientation = slider->orientation();
    option.minimum = slider->minimum();
    option.maximum = slider->maximum();
    option.sliderPosition = slider->value();
    option.sliderValue = slider->value();
    option.singleStep = slider->singleStep();
    option.pageStep = slider->pageStep();
    option.upsideDown = slider->invertedAppearance();
    option.state |= QStyle::State_Horizontal;
    return slider->style()->subControlRect(QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, slider);
}

QSize textSize(QLabel* label) {
    const int width = label->contentsRect().width();
    if (label->textFormat() == Qt::RichText
        || (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(label->text()))) {
        QTextDocument document;
        document.setDocumentMargin(0);
        document.setDefaultFont(label->font());
        document.setHtml(label->text());
        document.setTextWidth(width);
        return document.size().toSize();
    }
    return label->fontMetrics().boundingRect(QRect(0, 0, width, 1000),
        Qt::AlignLeft | Qt::AlignTop | (label->wordWrap() ? Qt::TextWordWrap : 0), label->text()).size();
}
}

class TstWorkspaceDisplayPill : public QObject {
    Q_OBJECT
    QFont m_font;
    QPalette m_palette;
    QString m_sheet;
private slots:
    void init() {
        m_font = qApp->font();
        m_palette = qApp->palette();
        m_sheet = qApp->styleSheet();
        AppSettings::instance().clear();
        WorkspacePreview::setEnabledForTest(true);
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        applyWorkspacePalette(*qApp);
    }
    void cleanup() {
        WorkspacePreview::setEnabledForTest(false);
        qApp->setFont(m_font);
        qApp->setStyleSheet(m_sheet);
        qApp->setPalette(m_palette);
    }
    void workspaceControlsAndUserSwatchesUseTheExistingPipeline() {
        Harness h;
        h.pan.setFillColor(QColor(QStringLiteral("#00b4d8")));
        QSignalSpy writes(&h.pan, &SpectrumWidget::displaySettingChanged);
        h.panel->openDisplayPill();
        QVERIFY(h.pill()->property(WorkspacePreview::kThemedIslandProperty).toBool());
        auto* swatch = h.control<QPushButton>("pillDisplayFillColor");
        QVERIFY(swatch);
        QVERIFY(swatch->property(WorkspacePreview::kSkipProperty).toBool());
        QVERIFY(swatch->styleSheet().contains(QStringLiteral("#00b4d8")));
        auto* slider = h.control<QSlider>("pillDisplayFftFillAlpha");
        QVERIFY(slider);
        QVERIFY(slider->property(WorkspacePreview::kThemedQssProperty).isValid());
        QVERIFY(slider->height() >= Workspace::Components::kSliderHandleSpanPx);
        QCOMPARE(writes.count(), 0);
        h.pan.setFillColor(QColor(QStringLiteral("#c8d8e8")));
        QCOMPARE(writes.count(), 1);
        WorkspacePreview::themeSubtree(h.pill());
        QEvent show(QEvent::Show);
        QCoreApplication::sendEvent(swatch, &show);
        QVERIFY(swatch->styleSheet().contains(QStringLiteral("#c8d8e8")));
        QCOMPARE(writes.count(), 1);
    }
    void allPagesFitTheWorkspaceFontAndHandle_data() {
        QTest::addColumn<bool>("wide");
        QTest::newRow("normal") << false;
        QTest::newRow("wide") << true;
    }
    void allPagesFitTheWorkspaceFontAndHandle() {
        QFETCH(bool, wide);
        if (wide) {
            QFont font = qApp->font();
            font.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
            qApp->setFont(font);
        }
        Harness h;
        h.panel->openDisplayPill();
        h.pan.setPeakHoldDelayMs(10000);
        h.pan.setDbmCalOffset(-30.0f);
        QSignalSpy writes(&h.pan, &SpectrumWidget::displaySettingChanged);
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        const int descendants = h.pill()->findChildren<QWidget*>().size();
        for (const Tab tab : {Tab::Spectrum, Tab::Levels, Tab::Waterfall, Tab::View3D, Tab::Fft}) {
            h.panel->setDisplayPillTab(tab);
            WorkspacePreview::themeSubtree(h.pill());
            QCoreApplication::processEvents();
            h.layout();
            qInfo() << "workspace fit" << wide << "tab" << static_cast<int>(tab)
                    << "pill" << h.pill()->size() << "tab budget"
                    << h.pill()->property("pillTabTextBudget");
            const char* diagnostics[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                         "displayPage3D", "displayPageFft"};
            QWidget* diagnosticPage = h.control<QWidget>(diagnostics[static_cast<int>(tab)]);
            qInfo() << "intrinsic" << diagnosticPage->layout()->sizeHint()
                    << "minimum" << diagnosticPage->layout()->minimumSize()
                    << "pill minimum" << h.pill()->minimumSize();
            QCOMPARE(h.pill()->width(), 268);
            QVERIFY2(h.pill()->height() <= 480, qPrintable(QString::number(h.pill()->height())));
            const char* pages[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                   "displayPage3D", "displayPageFft"};
            QWidget* page = h.control<QWidget>(pages[static_cast<int>(tab)]);
            QList<QPushButton*> buttons = page->findChildren<QPushButton*>();
            buttons.append(h.pill()->findChildren<QPushButton*>(
                QRegularExpression(QStringLiteral("^displayTab"))));
            for (QPushButton* button : buttons) {
                if (button->text().isEmpty() || button->isHidden()) { continue; }
                QVERIFY2(button->contentsRect().width() >= button->fontMetrics().horizontalAdvance(button->text()),
                         qPrintable(QStringLiteral("%1 text %2px, control %3px, tab budget %4px")
                             .arg(button->objectName()).arg(button->fontMetrics().horizontalAdvance(button->text()))
                             .arg(button->width()).arg(h.pill()->property("pillTabTextBudget").toInt())));
                const QRect rect(button->mapTo(h.pill(), QPoint(0, 0)), button->size());
                QVERIFY2(h.pill()->rect().contains(rect), qPrintable(button->objectName()));
            }
            for (QSlider* slider : page->findChildren<QSlider*>()) {
                if (slider->isHidden() || slider->parentWidget()->isHidden()) { continue; }
                const QRect handle = handleOf(slider);
                QVERIFY2(slider->rect().contains(handle), qPrintable(slider->objectName()));
                QRect rect = handle;
                for (QWidget* child = slider; child != h.pill(); child = child->parentWidget()) {
                    rect.translate(child->pos());
                    QVERIFY2(child->parentWidget()->rect().contains(rect), qPrintable(slider->objectName()));
                }
            }
            for (QLabel* label : page->findChildren<QLabel*>()) {
                if (label->text().isEmpty() || label->isHidden() || label->parentWidget()->isHidden()) { continue; }
                const QFont font = label->font();
                const qreal pixels = font.pixelSize() > 0 ? font.pixelSize()
                    : font.pointSizeF() * QFontMetricsF(font, label).fontDpi() / 72.0;
                QVERIFY2(pixels >= 11.0, qPrintable(label->objectName()));
                const QSize needed = textSize(label);
                QVERIFY2(needed.width() <= label->contentsRect().width()
                         && needed.height() <= label->contentsRect().height(),
                    qPrintable(QStringLiteral("%1 needs %2x%3, has %4x%5")
                        .arg(label->objectName()).arg(needed.width()).arg(needed.height())
                        .arg(label->width()).arg(label->height())));
            }
        }
        QCOMPARE(writes.count(), 0);
        // Repeating a style refresh must not add overflow rows indefinitely.
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        QCOMPARE(h.pill()->findChildren<QWidget*>().size(), descendants);
    }
    void captureActualWorkspacePagesWhenRequested() {
        const QString directory = qEnvironmentVariable("NEREUS_WORKSPACE_DISPLAY_CAPTURE_DIR");
        if (directory.isEmpty()) { return; }
        QVERIFY(QDir().mkpath(directory));
        Harness h;
        h.panel->openDisplayPill();
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        QJsonArray receipts;
        const struct { Tab tab; const char* name; } pages[] = {
            {Tab::Spectrum, "spectrum"}, {Tab::Levels, "levels"},
            {Tab::Waterfall, "waterfall"}, {Tab::View3D, "view"}, {Tab::Fft, "fft"}
        };
        for (const bool disabled : {false, true}) {
            h.pan.setPanFillEnabled(!disabled);
            h.pan.setPeakHoldEnabled(!disabled);
            for (const auto& page : pages) {
                h.panel->setDisplayPillTab(page.tab);
                WorkspacePreview::themeSubtree(h.pill());
                QCoreApplication::processEvents();
                h.layout();
                QCOMPARE(h.pan.panFillEnabled(), !disabled);
                QCOMPARE(h.control<QPushButton>("pillDisplayPanFill")->isChecked(), !disabled);
                QCOMPARE(h.control<QSlider>("pillDisplayFftFillAlpha")->isEnabled(), !disabled);
                QVERIFY(h.pill()->height() <= 480);
                const QString stem = QString::fromLatin1(page.name)
                    + (disabled ? QStringLiteral("-disabled") : QStringLiteral("-normal"));
                QJsonArray fonts;
                for (QLabel* label : h.pill()->findChildren<QLabel*>()) {
                    if (label->text().isEmpty()) { continue; }
                    const QFont font = label->font();
                    const qreal pixels = font.pixelSize() > 0 ? font.pixelSize()
                        : font.pointSizeF() * QFontMetricsF(font, label).fontDpi() / 72.0;
                    QVERIFY2(pixels >= 11.0, "Capture requires the actual signed shared Font B implementation");
                    fonts.append(QJsonObject{{"object", label->objectName()}, {"pixels", pixels}});
                }
                const char* pageNames[] = {"displayPageSpectrum", "displayPageLevels", "displayPageWaterfall",
                                           "displayPage3D", "displayPageFft"};
                QJsonArray sliders;
                QWidget* actualPage = h.control<QWidget>(pageNames[static_cast<int>(page.tab)]);
                for (QSlider* slider : actualPage->findChildren<QSlider*>()) {
                    const QRect handle = handleOf(slider);
                    sliders.append(QJsonObject{{"object", slider->objectName()}, {"enabled", slider->isEnabled()},
                        {"height", slider->height()}, {"handleWidth", handle.width()},
                        {"handleHeight", handle.height()}, {"style", slider->styleSheet()}});
                }
                QImage image(h.pill()->size(), QImage::Format_ARGB32_Premultiplied);
                image.fill(h.pill()->palette().color(QPalette::Window));
                h.pill()->render(&image);
                QVERIFY(image.save(directory + QLatin1Char('/') + stem + QStringLiteral(".png")));
                receipts.append(QJsonObject{{"image", stem + QStringLiteral(".png")},
                    {"width", h.pill()->width()}, {"height", h.pill()->height()},
                    {"tabTextBudget", h.pill()->property("pillTabTextBudget").toInt()},
                    {"theme", "actual shared Longpath-derived workspace"}, {"fonts", fonts}, {"sliders", sliders}});
            }
        }
        QFile receipt(directory + QStringLiteral("/workspace-pill-receipt.json"));
        QVERIFY(receipt.open(QIODevice::WriteOnly));
        QVERIFY(receipt.write(QJsonDocument(receipts).toJson(QJsonDocument::Indented)) > 0);
    }

    void dynamicReasonsAndNumberBoxesRemainWhole() {
        Harness h;
        h.panel->openDisplayPill();
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        h.panel->setDisplayPillTab(Tab::Spectrum);
        h.layout();
        auto* source = h.control<QSlider>("pillDisplayFftFillAlpha");
        h.pan.setPanFillEnabled(false);
        h.pan.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        QSignalSpy writes(&h.pan, &SpectrumWidget::displaySettingChanged);
        QEvent enter(QEvent::Enter);
        QCoreApplication::sendEvent(source, &enter);
        WorkspacePreview::themeSubtree(h.pill());
        QCoreApplication::processEvents();
        h.layout();
        auto* page = h.control<QWidget>("displayPageSpectrum");
        auto* reason = page->findChild<QLabel*>(QStringLiteral("pillReason"));
        QCOMPARE(reason->text(), source->toolTip());
        QVERIFY(textSize(reason).height() <= reason->contentsRect().height());
        for (const char* name : {"pillDisplaySpectrumAverageTimeMsBox", "pillDisplayWaterfallAverageTimeMsBox"}) {
            auto* box = h.control<QSpinBox>(name);
            QStyleOptionSpinBox option;
            option.initFrom(box);
            option.rect = box->rect();
            option.subControls = QStyle::SC_All;
            option.buttonSymbols = box->buttonSymbols();
            option.frame = box->hasFrame();
            const QRect field = box->style()->subControlRect(QStyle::CC_SpinBox, &option,
                QStyle::SC_SpinBoxEditField, box);
            auto* edit = box->findChild<QLineEdit*>();
            QVERIFY(edit);
            QVERIFY2(edit->fontMetrics().horizontalAdvance(QStringLiteral("9999 ms")) + 4 <= field.width(), name);
        }
        QCOMPARE(writes.count(), 0);
    }
};
QTEST_MAIN(TstWorkspaceDisplayPill)
#include "tst_workspace_display_pill.moc"
