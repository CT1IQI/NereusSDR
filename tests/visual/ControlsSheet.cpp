// no-port-check: NereusSDR-original visual preview tool; controls comparison sheet.
// =================================================================
// tests/visual/ControlsSheet.cpp  (NereusSDR)
// =================================================================
//
// See ControlsSheet.h. Every pixel on the sheet is a grab of a real Qt
// widget: the control cells, the popups, the panel crops of the main
// window and the TX EQ dialog, and the QLabels that carry the titles and
// hold those grabs. The zoom image enlarges cut-outs of the same grabs
// 5x, nearest-neighbour.
//
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "ControlsSheet.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEnterEvent>
#include <QFrame>
#include <QGroupBox>
#include <QHoverEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionButton>
#include <QStyleOptionSlider>
#include <QTabWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "LongpathControlStyles.h"
#include "PreviewData.h"
#include "PreviewScenes.h"
#include "gui/MainWindow.h"
#include "gui/StyleConstants.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/styles/QssTranslator.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/styles/WorkspaceStyle.h"
#include "gui/widgets/MeterSlider.h"

namespace NereusSDR::Preview {

namespace {

constexpr int kLookCount = 3;
constexpr int kToday = 0;
constexpr int kPreview = 1;
constexpr int kLongpath = 2;

constexpr int kSettleMs = 700;
constexpr int kShortSettleMs = 250;

// Control cells: the control sits kCellPadPx inside a cell that fills with
// the look's window colour.
constexpr int kCellPadPx = 6;
constexpr int kSliderValue = 60;
constexpr int kSliderMax = 100;
constexpr int kHSliderWidth = 140;
constexpr int kHSliderHeight = 18;     // RxApplet / TxApplet slider rows
constexpr int kVSliderWidth = 22;
constexpr int kVSliderHeight = 100;
constexpr int kToggleWidth = 120;
constexpr int kToggleHeight = 22;
constexpr int kFieldWidth = 140;
constexpr int kFieldHeight = 24;
constexpr int kSpinWidth = 100;
constexpr int kTabsWidth = 240;
constexpr int kTabsHeight = 100;
constexpr int kScrollBarHeight = 110;
constexpr int kScrollBarPage = 30;
constexpr int kScrollBarValue = 20;
constexpr int kGroupWidth = 200;
constexpr int kGroupHeight = 80;
constexpr int kButtonWidth = 84;
constexpr int kButtonHeight = 24;
constexpr int kMeterSliderWidth = 160;
constexpr float kMeterSliderGain = 0.6f;
constexpr float kMeterSliderLevel = 0.45f;
constexpr int kSpinValue = 42;
constexpr int kComboHoverRow = 1;

// Panel crops: the slider rows' band, widened to the applet, plus this
// much above and below.
constexpr int kBandPadPx = 4;

// Zoom: this much logical source around each handle or indicator, 5x.
constexpr int kZoomSourceWidth = 32;
constexpr int kZoomSourceHeight = 24;
constexpr int kZoomFactor = 5;

// Sheet layout (logical px).
constexpr int kSheetMarginPx = 16;
constexpr int kRowLabelWidth = 170;
constexpr int kShotGapPx = 10;
constexpr int kColumnGapPx = 24;
constexpr int kRowGapPx = 14;
constexpr int kMinColumnWidth = 260;
constexpr auto kSheetBg = "#141418";
constexpr auto kSheetRule = "#2a2a30";
constexpr auto kSheetTitleColour = "#e4e4e8";
constexpr auto kSheetTextColour = "#c4c4ca";
constexpr auto kSheetCaptionColour = "#8a8a92";

QString lookKey(int look)
{
    switch (look) {
    case kToday: return QStringLiteral("today");
    case kPreview: return QStringLiteral("preview");
    default: return QStringLiteral("longpath");
    }
}

QString lookTitle(int look)
{
    switch (look) {
    case kToday: return QStringLiteral("Today (shipped, preview switch off)");
    case kPreview: return QStringLiteral("Preview now (translator + componentize)");
    default: return QStringLiteral("Longpath (its own stylesheets)");
    }
}

// Real QSlider / QCheckBox / QRadioButton (no Q_OBJECT, so stylesheets see
// the Qt class) that can say where the style puts the handle or indicator.
class ProbeSlider : public QSlider
{
public:
    using QSlider::QSlider;
    QRect handleRect() const
    {
        QStyleOptionSlider o;
        initStyleOption(&o);
        return style()->subControlRect(QStyle::CC_Slider, &o, QStyle::SC_SliderHandle, this);
    }
};

class ProbeCheckBox : public QCheckBox
{
public:
    using QCheckBox::QCheckBox;
    QRect indicatorRect() const
    {
        QStyleOptionButton o;
        initStyleOption(&o);
        return style()->subElementRect(QStyle::SE_CheckBoxIndicator, &o, this);
    }
};

class ProbeRadioButton : public QRadioButton
{
public:
    using QRadioButton::QRadioButton;
    QRect indicatorRect() const
    {
        QStyleOptionButton o;
        initStyleOption(&o);
        return style()->subElementRect(QStyle::SE_RadioButtonIndicator, &o, this);
    }
};

struct Shot {
    QString caption;
    QImage image;          // device pixels, devicePixelRatio set
    QRect focus;           // logical, in image coordinates; empty: not zoomed
    QSize sizeHint;        // the control's own size hint in this look
    QSize minimumHint;
    QJsonObject source;    // where a panel crop came from
};

struct Row {
    QString key;
    QString title;
    QString note;
    std::array<QVector<Shot>, kLookCount> looks;
};

struct Rows {
    QVector<Row> rows;

    Row& row(const QString& key, const QString& title, const QString& note = {})
    {
        for (Row& r : rows) {
            if (r.key == key) {
                return r;
            }
        }
        rows.append(Row{key, title, note, {}});
        return rows.last();
    }
};

struct LookSheets {
    QString sliderH;
    QString sliderV;
    QString checkBox;
    QString radio;
    QString combo;
    QString lineEdit;
    QString spinBox;
    QString tabs;
    QString scrollBar;
    QString groupBox;
    QString menu;
    QString buttons;
};

struct AppLook {
    QPalette palette;
    QString qss;
};

QApplication* app()
{
    return qobject_cast<QApplication*>(QCoreApplication::instance());
}

// Today's strings. Call with the switch off: the Style:: builders return
// their original text then.
LookSheets todaySheets()
{
    LookSheets s;
    s.sliderH = Style::sliderHStyle();
    s.sliderV = Style::sliderVStyle();
    s.checkBox = QString::fromLatin1(Style::kCheckBoxStyle);
    s.radio = QString::fromLatin1(Style::kRadioButtonStyle);
    s.combo = QString::fromLatin1(Style::kComboStyle);
    s.lineEdit = QString::fromLatin1(Style::kLineEditStyle);
    s.spinBox = QString::fromLatin1(Style::kSpinBoxStyle);
    // The same string as SpotHubDialog.cpp's tab widget.
    s.tabs = QStringLiteral(
        "QTabWidget::pane { border: 1px solid #203040; }"
        "QTabBar::tab { background: #1a1a2e; color: #808890; border: 1px solid #203040; "
        "  padding: 6px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #0f0f1a; color: #00b4d8; border-bottom: none; }");
    // The same string as AppletPanelWidget.cpp's scroll area.
    s.scrollBar = QStringLiteral(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical {"
        "  background: %1; width: 8px; border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %2; border-radius: 4px; min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    ).arg(Style::kPanelBg, Style::kGroove);
    s.groupBox = QString::fromLatin1(Style::kGroupBoxStyle);
    s.menu = QString::fromLatin1(kPopupMenu);
    s.buttons = Style::buttonBaseStyle() + Style::greenCheckedStyle();
    return s;
}

// The preview takes today's strings through the scoped pass; the scroll
// bar is ContainerContentHost's preview string (Longpath's bar at the
// platform thickness).
LookSheets previewSheets(const LookSheets& today, int barPx)
{
    LookSheets s = today;
    QString bars = QString::fromLatin1(Workspace::kScrollBarStyle);
    bars.replace(QLatin1String("width: 10px; margin: 0;"),
                 QStringLiteral("width: %1px; margin: 0;").arg(barPx));
    bars.replace(QLatin1String("height: 10px; margin: 0;"),
                 QStringLiteral("height: %1px; margin: 0;").arg(barPx));
    s.scrollBar = bars;
    return s;
}

LookSheets longpathSheets()
{
    namespace LP = LongpathLook;
    LookSheets s;
    s.sliderH = LP::sliderHStyle();
    s.sliderV = LP::sliderVStyle();
    s.checkBox = QString::fromLatin1(LP::kCheckBoxStyle);
    s.radio = QString::fromLatin1(LP::kRadioButtonStyle);
    s.combo = QString::fromLatin1(LP::kComboStyle);
    s.lineEdit = LP::formFieldStyle();
    s.spinBox = LP::formFieldStyle();
    s.tabs = QString::fromLatin1(LP::kTabStyle);
    s.scrollBar = QString::fromLatin1(Workspace::kScrollBarStyle);
    s.groupBox = QString::fromLatin1(LP::kGroupBoxStyle);
    // Longpath's call sites set kPopupMenu; its ThemeFilter translates the
    // legacy values when the menu polishes.
    s.menu = LP::themed(QString::fromLatin1(LP::kPopupMenu));
    s.buttons = LP::buttonBaseStyle() + LP::blueCheckedStyle();
    return s;
}

void applyLook(int look, const AppLook& today)
{
    QApplication* a = app();
    WorkspacePreview::setEnabledForTest(look == kPreview);
    if (look == kLongpath) {
        // Longpath's startup: Fusion's palette, then its own palette and
        // baseline.
        a->setPalette(a->style()->standardPalette());
        LongpathLook::applyDarkPalette(*a);
        LongpathLook::applyAppBaselineQss(*a);
    } else {
        a->setPalette(today.palette);
        a->setStyleSheet(today.qss);
        if (look == kPreview) {
            applyWorkspacePalette(*a);
        }
    }
    pump(kSettleMs);
}

QImage grabImage(QWidget* w, const QRect& r = QRect(QPoint(0, 0), QSize(-1, -1)))
{
    return w->grab(r).toImage();
}

QSize logicalSizeOf(const QImage& img)
{
    const double dpr = img.devicePixelRatio();
    return QSize(static_cast<int>(std::lround(img.width() / dpr)),
                 static_cast<int>(std::lround(img.height() / dpr)));
}

QRect toDevice(const QRect& logical, double dpr)
{
    return QRect(static_cast<int>(std::lround(logical.x() * dpr)),
                 static_cast<int>(std::lround(logical.y() * dpr)),
                 static_cast<int>(std::lround(logical.width() * dpr)),
                 static_cast<int>(std::lround(logical.height() * dpr)));
}

QJsonArray rectArray(const QRect& r)
{
    return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}

QJsonArray sizeArray(const QSize& s)
{
    return QJsonArray{s.width(), s.height()};
}

// ---- control cells ---------------------------------------------------------

class Column
{
public:
    Column()
        : m_top(std::make_unique<QWidget>())
    {
        m_top->setObjectName(QStringLiteral("controlsSheetColumn"));
        m_top->setAttribute(Qt::WA_DontShowOnScreen, true);
        m_top->setAutoFillBackground(true);
    }

    QWidget* top() const { return m_top.get(); }

    // A cell for a control of `inner` logical size.
    QWidget* cell(const QSize& inner)
    {
        auto* c = new QWidget(m_top.get());
        c->setAutoFillBackground(true);
        c->setGeometry(kCellPadPx, m_y, inner.width() + 2 * kCellPadPx, inner.height() + 2 * kCellPadPx);
        m_y += c->height() + kCellPadPx;
        m_width = qMax(m_width, c->width());
        return c;
    }

    void place(QWidget* control, const QSize& size)
    {
        control->move(kCellPadPx, kCellPadPx);
        control->setFixedSize(size);
    }

    void finish()
    {
        m_top->resize(m_width + 2 * kCellPadPx, m_y);
        m_top->show();
        pump(kSettleMs);
    }

private:
    std::unique_ptr<QWidget> m_top;
    int m_y{kCellPadPx};
    int m_width{0};
};

void hover(QWidget* w, const QPoint& local)
{
    w->setAttribute(Qt::WA_Hover, true);
    w->setAttribute(Qt::WA_UnderMouse, true);
    const QPointF p(local);
    QEnterEvent enter(p, w->mapTo(w->window(), p), w->mapToGlobal(p));
    QCoreApplication::sendEvent(w, &enter);
    QHoverEvent hoverMove(QEvent::HoverMove, p, w->mapToGlobal(p), p);
    QCoreApplication::sendEvent(w, &hoverMove);
    // QPushButton takes its hover from a mouse move it hit-tests.
    QMouseEvent mouseMove(QEvent::MouseMove, p, w->mapToGlobal(p), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &mouseMove);
    w->update();
}

void addShot(Rows& rows, int look, const QString& key, const QString& title, const QString& caption,
             QWidget* cell, QWidget* control, const QRect& focusInControl, const QString& note = {})
{
    Shot s;
    s.caption = caption;
    s.image = grabImage(cell);
    if (!focusInControl.isEmpty()) {
        const QPoint centre = control->mapTo(cell, focusInControl.center());
        s.focus = QRect(centre.x() - kZoomSourceWidth / 2, centre.y() - kZoomSourceHeight / 2,
                        kZoomSourceWidth, kZoomSourceHeight);
    }
    s.sizeHint = control->sizeHint();
    s.minimumHint = control->minimumSizeHint();
    rows.row(key, title, note).looks[look].append(s);
}

void addImageShot(Rows& rows, int look, const QString& key, const QString& title, const QString& caption,
                  const QImage& image, const QJsonObject& source = {}, const QString& note = {})
{
    Shot s;
    s.caption = caption;
    s.image = image;
    s.source = source;
    rows.row(key, title, note).looks[look].append(s);
}

void captureControls(SceneContext& ctx, int look, const LookSheets& sheets, Rows& rows)
{
    Column col;

    // Horizontal slider: resting, hover, pressed, disabled.
    std::array<QWidget*, 4> hCells{};
    std::array<ProbeSlider*, 4> hSliders{};
    for (std::size_t i = 0; i < hSliders.size(); ++i) {
        hCells[i] = col.cell(QSize(kHSliderWidth, kHSliderHeight));
        hSliders[i] = new ProbeSlider(Qt::Horizontal, hCells[i]);
        hSliders[i]->setRange(0, kSliderMax);
        hSliders[i]->setValue(kSliderValue);
        hSliders[i]->setStyleSheet(sheets.sliderH);
        col.place(hSliders[i], QSize(kHSliderWidth, kHSliderHeight));
    }
    hSliders[3]->setEnabled(false);

    QWidget* vCell = col.cell(QSize(kVSliderWidth, kVSliderHeight));
    auto* vSlider = new ProbeSlider(Qt::Vertical, vCell);
    vSlider->setRange(0, kSliderMax);
    vSlider->setValue(kSliderValue);
    vSlider->setStyleSheet(sheets.sliderV);
    col.place(vSlider, QSize(kVSliderWidth, kVSliderHeight));

    // Check box: off, on, disabled (on).
    std::array<QWidget*, 3> cbCells{};
    std::array<ProbeCheckBox*, 3> boxes{};
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        cbCells[i] = col.cell(QSize(kToggleWidth, kToggleHeight));
        boxes[i] = new ProbeCheckBox(QStringLiteral("Check box"), cbCells[i]);
        boxes[i]->setStyleSheet(sheets.checkBox);
        col.place(boxes[i], QSize(kToggleWidth, kToggleHeight));
    }
    boxes[1]->setChecked(true);
    boxes[2]->setChecked(true);
    boxes[2]->setEnabled(false);

    // Radio button: off, on (each in its own cell, so not exclusive).
    std::array<QWidget*, 2> rCells{};
    std::array<ProbeRadioButton*, 2> radios{};
    for (std::size_t i = 0; i < radios.size(); ++i) {
        rCells[i] = col.cell(QSize(kToggleWidth, kToggleHeight));
        radios[i] = new ProbeRadioButton(QStringLiteral("Radio"), rCells[i]);
        radios[i]->setStyleSheet(sheets.radio);
        col.place(radios[i], QSize(kToggleWidth, kToggleHeight));
    }
    radios[1]->setChecked(true);

    // Combo box: closed, and one to open.
    std::array<QWidget*, 2> comboCells{};
    std::array<QComboBox*, 2> combos{};
    for (std::size_t i = 0; i < combos.size(); ++i) {
        comboCells[i] = col.cell(QSize(kFieldWidth, kFieldHeight));
        combos[i] = new QComboBox(comboCells[i]);
        combos[i]->addItems({QStringLiteral("USB"), QStringLiteral("LSB"), QStringLiteral("CW"),
                             QStringLiteral("DIGU")});
        combos[i]->setStyleSheet(sheets.combo);
        col.place(combos[i], QSize(kFieldWidth, kFieldHeight));
    }

    // Line edit and spin box: resting, focused.
    std::array<QWidget*, 2> editCells{};
    std::array<QLineEdit*, 2> edits{};
    for (std::size_t i = 0; i < edits.size(); ++i) {
        editCells[i] = col.cell(QSize(kFieldWidth, kFieldHeight));
        edits[i] = new QLineEdit(QStringLiteral("14.225.000"), editCells[i]);
        edits[i]->setStyleSheet(sheets.lineEdit);
        edits[i]->setCursorPosition(0);
        col.place(edits[i], QSize(kFieldWidth, kFieldHeight));
    }
    std::array<QWidget*, 2> spinCells{};
    std::array<QSpinBox*, 2> spins{};
    for (std::size_t i = 0; i < spins.size(); ++i) {
        spinCells[i] = col.cell(QSize(kSpinWidth, kFieldHeight));
        spins[i] = new QSpinBox(spinCells[i]);
        spins[i]->setRange(0, kSliderMax);
        spins[i]->setValue(kSpinValue);
        spins[i]->setStyleSheet(sheets.spinBox);
        col.place(spins[i], QSize(kSpinWidth, kFieldHeight));
    }

    QWidget* tabsCell = col.cell(QSize(kTabsWidth, kTabsHeight));
    auto* tabs = new QTabWidget(tabsCell);
    for (const QString& name : {QStringLiteral("RX"), QStringLiteral("TX"), QStringLiteral("Audio")}) {
        auto* page = new QWidget;
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(QStringLiteral("%1 page").arg(name), page));
        tabs->addTab(page, name);
    }
    tabs->setStyleSheet(sheets.tabs);
    col.place(tabs, QSize(kTabsWidth, kTabsHeight));

    // Scroll bar: its own thickness in each look (the thickness is one of
    // the differences), fixed length.
    auto* bar = new QScrollBar(Qt::Vertical);
    bar->setStyleSheet(sheets.scrollBar);
    bar->ensurePolished();
    const int barWidth = bar->sizeHint().width();
    QWidget* barCell = col.cell(QSize(barWidth, kScrollBarHeight));
    bar->setParent(barCell);
    bar->show();
    bar->setRange(0, kSliderMax);
    bar->setPageStep(kScrollBarPage);
    bar->setValue(kScrollBarValue);
    col.place(bar, QSize(barWidth, kScrollBarHeight));

    QWidget* groupCell = col.cell(QSize(kGroupWidth, kGroupHeight));
    auto* group = new QGroupBox(QStringLiteral("Group box"), groupCell);
    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(new QLabel(QStringLiteral("Content"), group));
    group->setStyleSheet(sheets.groupBox);
    col.place(group, QSize(kGroupWidth, kGroupHeight));

    // Push buttons: resting, hovered, checked, disabled.
    std::array<QWidget*, 4> buttonCells{};
    std::array<QPushButton*, 4> buttons{};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        buttonCells[i] = col.cell(QSize(kButtonWidth, kButtonHeight));
        buttons[i] = new QPushButton(QStringLiteral("OPERATE"), buttonCells[i]);
        buttons[i]->setCheckable(true);
        buttons[i]->setStyleSheet(sheets.buttons);
        col.place(buttons[i], QSize(kButtonWidth, kButtonHeight));
    }
    buttons[2]->setChecked(true);
    buttons[3]->setEnabled(false);

    QWidget* meterCell = col.cell(QSize(kMeterSliderWidth, 16));
    auto* meter = new MeterSlider(meterCell);
    meter->setGain(kMeterSliderGain);
    meter->setLevel(kMeterSliderLevel);
    col.place(meter, QSize(kMeterSliderWidth, 16));

    col.finish();
    if (look == kPreview) {
        WorkspacePreview::themeSubtree(col.top());
        pump(kSettleMs);
    }

    // States that need the shown widget.
    hover(hSliders[1], hSliders[1]->handleRect().center());
    hover(hSliders[2], hSliders[2]->handleRect().center());
    {
        const QPointF p(hSliders[2]->handleRect().center());
        QMouseEvent press(QEvent::MouseButtonPress, p, hSliders[2]->mapToGlobal(p), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(hSliders[2], &press);
    }
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(col.top());
    QT_WARNING_POP
    hover(buttons[1], QPoint(kButtonWidth / 2, kButtonHeight / 2));
    pump(kShortSettleMs);

    const QString hKey = QStringLiteral("slider-h");
    const QString hTitle = QStringLiteral("Horizontal slider, 60 %");
    const std::array<QString, 4> hCaptions{QStringLiteral("resting"), QStringLiteral("hover"),
                                           QStringLiteral("pressed"), QStringLiteral("disabled")};
    for (std::size_t i = 0; i < hSliders.size(); ++i) {
        addShot(rows, look, hKey, hTitle, hCaptions[i], hCells[i], hSliders[i], hSliders[i]->handleRect());
    }
    addShot(rows, look, QStringLiteral("slider-v"), QStringLiteral("Vertical slider, 60 %"),
            QStringLiteral("resting"), vCell, vSlider, vSlider->handleRect());
    const std::array<QString, 3> cbCaptions{QStringLiteral("off"), QStringLiteral("on"),
                                            QStringLiteral("disabled (on)")};
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        addShot(rows, look, QStringLiteral("checkbox"), QStringLiteral("Check box"), cbCaptions[i],
                cbCells[i], boxes[i], boxes[i]->indicatorRect());
    }
    for (std::size_t i = 0; i < radios.size(); ++i) {
        addShot(rows, look, QStringLiteral("radio"), QStringLiteral("Radio button"),
                i == 0 ? QStringLiteral("off") : QStringLiteral("on"), rCells[i], radios[i],
                radios[i]->indicatorRect());
    }
    addShot(rows, look, QStringLiteral("combo"), QStringLiteral("Combo box"), QStringLiteral("closed"),
            comboCells[0], combos[0], {},
            QStringLiteral("Popup row colour unverified: offscreen, Qt paints the selected row without "
                           "the stylesheet's selection colour."));
    addShot(rows, look, QStringLiteral("line-edit"), QStringLiteral("Line edit"), QStringLiteral("resting"),
            editCells[0], edits[0], {});
    addShot(rows, look, QStringLiteral("spin-box"), QStringLiteral("Spin box"), QStringLiteral("resting"),
            spinCells[0], spins[0], {});
    addShot(rows, look, QStringLiteral("tabs"), QStringLiteral("Tab bar with its pane"),
            QStringLiteral("RX selected"), tabsCell, tabs, {});
    addShot(rows, look, QStringLiteral("scroll-bar"), QStringLiteral("Scroll bar"),
            QStringLiteral("%1 px thick").arg(barWidth), barCell, bar, {});
    addShot(rows, look, QStringLiteral("group-box"), QStringLiteral("Group box"), QStringLiteral("resting"),
            groupCell, group, {});
    // The menu and tooltip rows sit here; their shots come below.
    rows.row(QStringLiteral("menu"), QStringLiteral("Menu"));
    rows.row(QStringLiteral("tooltip"), QStringLiteral("Tooltip"));
    const std::array<QString, 4> buttonCaptions{QStringLiteral("resting"), QStringLiteral("hovered"),
                                                QStringLiteral("checked"), QStringLiteral("disabled")};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        addShot(rows, look, QStringLiteral("buttons"),
                QStringLiteral("Push button (checked: today green OPERATE, Longpath blue)"),
                buttonCaptions[i], buttonCells[i], buttons[i], {});
    }
    addShot(rows, look, QStringLiteral("meter-slider"), QStringLiteral("MeterSlider"),
            QStringLiteral("gain 0.6, level 0.45"), meterCell, meter, {},
            QStringLiteral("Paints itself; it does not take stylesheets, so all three are the same."));

    // Focus: one field at a time in the active column window.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(col.top());
    QT_WARNING_POP
    edits[1]->setFocus(Qt::OtherFocusReason);
    edits[1]->setCursorPosition(0);
    pump(kShortSettleMs);
    if (!edits[1]->hasFocus()) {
        ctx.notes.append(QStringLiteral("%1: the focused line edit did not take focus").arg(lookKey(look)));
    }
    addShot(rows, look, QStringLiteral("line-edit"), QString(), QStringLiteral("focused"), editCells[1],
            edits[1], {});
    spins[1]->setFocus(Qt::OtherFocusReason);
    pump(kShortSettleMs);
    if (!spins[1]->hasFocus()) {
        ctx.notes.append(QStringLiteral("%1: the focused spin box did not take focus").arg(lookKey(look)));
    }
    addShot(rows, look, QStringLiteral("spin-box"), QString(), QStringLiteral("focused"), spinCells[1],
            spins[1], {});
    spins[1]->clearFocus();
    pump(kShortSettleMs);

    // Combo popup with a row hovered.
    combos[1]->showPopup();
    pump(kSettleMs);
    QAbstractItemView* view = combos[1]->view();
    // A live popup is the active window; its selection then paints in the
    // Active palette group, as it does under the pointer.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(view->window());
    QT_WARNING_POP
    const QModelIndex hovered = view->model()->index(kComboHoverRow, 0);
    view->setCurrentIndex(hovered);
    hover(view->viewport(), view->visualRect(hovered).center());
    pump(kShortSettleMs);
    if (look == kToday) {
        ctx.notes.append(QStringLiteral(
            "combo popup: LSB is the view's current and selected row and the popup is the active "
            "window, yet the offscreen render paints that row without the stylesheet's selection "
            "colour (today: unhighlighted, preview and Longpath: black). Check the row colour against "
            "a live popup before judging it."));
    }
    addShot(rows, look, QStringLiteral("combo"), QString(), QStringLiteral("open"), comboCells[1], combos[1],
            {});
    addImageShot(rows, look, QStringLiteral("combo"), QString(), QStringLiteral("popup, LSB selected (see note)"),
                 grabImage(view->window()));
    combos[1]->hidePopup();
    pump(kSettleMs);

    // Menu with a checked and a disabled item, the first item active.
    auto menu = std::make_unique<QMenu>();
    menu->setStyleSheet(sheets.menu);
    QAction* first = menu->addAction(QStringLiteral("Active item"));
    QAction* checked = menu->addAction(QStringLiteral("Checked item"));
    checked->setCheckable(true);
    checked->setChecked(true);
    QAction* disabled = menu->addAction(QStringLiteral("Disabled item"));
    disabled->setEnabled(false);
    menu->addAction(QStringLiteral("Plain item"));
    if (look == kPreview) {
        WorkspacePreview::themeSubtree(menu.get());
    }
    menu->popup(col.top()->mapToGlobal(QPoint(0, 0)));
    pump(kSettleMs);
    menu->setActiveAction(first);
    pump(kShortSettleMs);
    addImageShot(rows, look, QStringLiteral("menu"), QString(), QStringLiteral("open, first item active"),
                 grabImage(menu.get()));
    menu->hide();
    pump(kShortSettleMs);

    // Tooltip, styled by the application's stylesheet in every look.
    QToolTip::showText(col.top()->mapToGlobal(QPoint(20, 20)), QStringLiteral("AGC max gain"), col.top());
    pump(kSettleMs);
    QWidget* tip = nullptr;
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && w->inherits("QTipLabel")) {
            tip = w;
        }
    }
    if (tip != nullptr) {
        addImageShot(rows, look, QStringLiteral("tooltip"), QString(), QStringLiteral("shown"), grabImage(tip));
    } else {
        ctx.notes.append(QStringLiteral("%1: the tooltip did not show").arg(lookKey(look)));
    }
    QToolTip::hideText();
    pump(kSettleMs);
}

// ---- real panels -----------------------------------------------------------

struct Panels {
    QPointer<QWidget> mainPanel;
    QPointer<RxApplet> rx;
    QPointer<TxApplet> tx;
    QPointer<PhoneCwApplet> phone;
    QPointer<TxEqDialog> eq;
    QVector<QPair<QPointer<QWidget>, QString>> sheets;   // today's stylesheets
    QString mainWindowSheet;
};

void snapshot(QWidget* root, Panels& panels)
{
    if (root == nullptr) {
        return;
    }
    panels.sheets.append({QPointer<QWidget>(root), root->styleSheet()});
    for (QWidget* w : root->findChildren<QWidget*>()) {
        panels.sheets.append({QPointer<QWidget>(w), w->styleSheet()});
    }
}

void restoreSheets(const Panels& panels)
{
    for (const auto& [w, sheet] : panels.sheets) {
        if (w.isNull()) {
            continue;
        }
        if (w->styleSheet() != sheet) {
            w->setStyleSheet(sheet);
        }
        // The scoped pass keeps its own record; clear it so the next pass
        // starts from today's sheet.
        w->setProperty(WorkspacePreview::kOriginalQssProperty, QVariant());
        w->setProperty(WorkspacePreview::kThemedQssProperty, QVariant());
    }
}

// Longpath on the real panels: every QSlider takes Longpath's slider
// stylesheet (as Longpath's applets and TxEqDialog set it); every other
// stylesheet goes through the legacy-value table, as Longpath's
// ThemeFilter does at polish.
void longpathPanels(const Panels& panels)
{
    for (const auto& [w, sheet] : panels.sheets) {
        if (w.isNull()) {
            continue;
        }
        if (auto* s = qobject_cast<QSlider*>(w.data())) {
            s->setStyleSheet(s->orientation() == Qt::Horizontal ? LongpathLook::sliderHStyle()
                                                                : LongpathLook::sliderVStyle());
        } else if (!sheet.isEmpty()) {
            w->setStyleSheet(LongpathLook::themed(sheet));
        }
    }
}

QScrollArea* scrollAreaOf(QWidget* w)
{
    for (QWidget* p = w != nullptr ? w->parentWidget() : nullptr; p != nullptr; p = p->parentWidget()) {
        if (auto* area = qobject_cast<QScrollArea*>(p)) {
            return area;
        }
    }
    return nullptr;
}

QRect rectIn(QWidget* top, QWidget* w)
{
    return QRect(w->mapTo(top, QPoint(0, 0)), w->size());
}

// The band of `sliders` across `applet`'s full width, clipped to the
// scroll viewport it sits in.
QRect sliderBand(QWidget* top, QWidget* applet, const QList<QSlider*>& sliders)
{
    QRect u;
    for (QSlider* s : sliders) {
        if (s != nullptr && s->isVisible()) {
            u |= rectIn(top, s);
        }
    }
    if (u.isEmpty()) {
        return {};
    }
    const QRect a = rectIn(top, applet);
    QRect band(a.left(), u.top() - kBandPadPx, a.width(), u.height() + 2 * kBandPadPx);
    if (QScrollArea* area = scrollAreaOf(applet)) {
        band &= rectIn(top, area->viewport());
    }
    return band;
}

void cropShot(SceneContext& ctx, Rows& rows, int look, const QString& key, const QString& title,
              QWidget* top, const QRect& band, const QString& sourceName, const QString& missing,
              const QString& caption = QStringLiteral("cropped"), QJsonObject source = {})
{
    if (band.isEmpty()) {
        ctx.notes.append(QStringLiteral("%1 %2: %3").arg(lookKey(look), key, missing));
        return;
    }
    const QImage img = grabImage(top, band);
    source[QStringLiteral("source")] = sourceName;
    source[QStringLiteral("logical_rect")] = rectArray(band);
    source[QStringLiteral("device_rect")] = rectArray(toDevice(band, img.devicePixelRatio()));
    addImageShot(rows, look, key, title, caption, img, source);
}

// The band of `sliders` across `applet`'s full width, in the applet's own
// coordinates. Grabbed from the applet itself, so the Main Panel's scroll
// viewport cannot clip it and no scrolling is needed.
QRect appletBand(QWidget* applet, const QList<QSlider*>& sliders)
{
    QRect u;
    for (QSlider* s : sliders) {
        if (s != nullptr && s->isVisibleTo(applet)) {
            u |= QRect(s->mapTo(applet, QPoint(0, 0)), s->size());
        }
    }
    if (u.isEmpty()) {
        return {};
    }
    return QRect(0, u.top() - kBandPadPx, applet->width(), u.height() + 2 * kBandPadPx)
        .intersected(applet->rect());
}

// One applet row: the band grabbed from the applet at its full width. The
// sidecar also gives the band in window coordinates and the part of it the
// Main Panel viewport shows, so today's column can be checked against the
// baseline window where the viewport shows it.
void appletShot(SceneContext& ctx, Rows& rows, int look, const QString& key, const QString& title,
                QWidget* applet, const QList<QSlider*>& sliders, const QString& missing)
{
    const QRect band = appletBand(applet, sliders);
    QJsonObject source;
    if (!band.isEmpty()) {
        const QRect inWindow = band.translated(applet->mapTo(ctx.window, QPoint(0, 0)));
        source[QStringLiteral("window_logical_rect")] = rectArray(inWindow);
        if (QScrollArea* area = scrollAreaOf(applet)) {
            const QRect shown = inWindow & rectIn(ctx.window, area->viewport());
            source[QStringLiteral("viewport_shown_window_rect")] = rectArray(shown);
            source[QStringLiteral("viewport_hidden_px")] = inWindow.width() - shown.width();
        }
    }
    cropShot(ctx, rows, look, key, title, applet, band,
             QString::fromLatin1(applet->metaObject()->className()) + QStringLiteral(" at full width"),
             missing, QStringLiteral("applet at full width"), source);
}

QList<QSlider*> slidersNamed(QWidget* root, const QStringList& accessibleOrObjectNames)
{
    QList<QSlider*> out;
    if (root == nullptr) {
        return out;
    }
    for (QSlider* s : root->findChildren<QSlider*>()) {
        if (accessibleOrObjectNames.contains(s->accessibleName())
            || accessibleOrObjectNames.contains(s->objectName())) {
            out.append(s);
        }
    }
    return out;
}

void capturePanels(SceneContext& ctx, int look, Panels& panels, Rows& rows)
{
    QWidget* window = ctx.window;
    pump(kSettleMs);

    // RX applet: its visible slider rows at full width, and the same rows
    // as the main window shows them through the Main Panel viewport.
    if (!panels.rx.isNull()) {
        QList<QSlider*> rxSliders;
        for (QSlider* s : panels.rx->findChildren<QSlider*>()) {
            if (s->isVisible()) {
                rxSliders.append(s);
            }
        }
        appletShot(ctx, rows, look, QStringLiteral("panel-rx"),
                   QStringLiteral("RX applet slider rows (the applet at full width)"), panels.rx, rxSliders,
                   QStringLiteral("no visible RX applet slider"));
        cropShot(ctx, rows, look, QStringLiteral("panel-rx-viewport"),
                 QStringLiteral("RX applet through the Main Panel viewport (real clipping, today's window layout)"),
                 window, sliderBand(window, panels.rx, rxSliders), QStringLiteral("main window"),
                 QStringLiteral("no visible RX applet slider"), QStringLiteral("viewport crop"));
    } else {
        ctx.notes.append(QStringLiteral("%1: no RxApplet").arg(lookKey(look)));
    }

    // TX applet: RF power and tune power.
    if (!panels.tx.isNull()) {
        appletShot(ctx, rows, look, QStringLiteral("panel-tx"),
                   QStringLiteral("TX applet RF power and tune power (the applet at full width)"), panels.tx,
                   slidersNamed(panels.tx, {QStringLiteral("TxRfPowerSlider"), QStringLiteral("Tune power")}),
                   QStringLiteral("RF power and tune power sliders not found"));
    }

    // Phone/CW applet, Phone page: microphone gain.
    if (!panels.phone.isNull()) {
        panels.phone->showPage(0);
        pump(kShortSettleMs);
        appletShot(ctx, rows, look, QStringLiteral("panel-mic"),
                   QStringLiteral("Phone/CW applet microphone gain (the applet at full width)"), panels.phone,
                   slidersNamed(panels.phone, {QStringLiteral("Microphone gain")}),
                   QStringLiteral("microphone gain slider not found"));
    }
    pump(kShortSettleMs);

    // TX EQ dialog: its vertical sliders.
    if (!panels.eq.isNull()) {
        QRect u;
        for (QSlider* s : panels.eq->findChildren<QSlider*>()) {
            if (s->isVisible() && s->orientation() == Qt::Vertical) {
                u |= rectIn(panels.eq, s);
            }
        }
        const QRect band = u.isEmpty() ? QRect()
                                       : u.adjusted(-kBandPadPx, -kBandPadPx, kBandPadPx, kBandPadPx)
                                             .intersected(panels.eq->rect());
        cropShot(ctx, rows, look, QStringLiteral("panel-eq"), QStringLiteral("TX EQ dialog sliders"),
                 panels.eq, band, QStringLiteral("TxEqDialog"), QStringLiteral("no visible EQ slider"));
    }
}

// ---- assembly ----------------------------------------------------------------

QString labelSheet(const char* colour, int px, bool bold = false)
{
    return QStringLiteral("QLabel { color: %1; font-size: %2px; %3 background: transparent; }")
        .arg(QLatin1String(colour))
        .arg(px)
        .arg(bold ? QStringLiteral("font-weight: bold;") : QString());
}

QLabel* textLabel(QWidget* parent, const QString& text, const char* colour, int px, bool bold = false)
{
    auto* l = new QLabel(text, parent);
    l->setStyleSheet(labelSheet(colour, px, bold));
    l->adjustSize();
    return l;
}

// Lays the rows out as a sheet of QLabels and grabs it. `placements`
// receives each image's rectangle on the sheet.
QImage assemble(const Rows& rows, const QString& title, const QString& subtitle, QJsonArray& placements)
{
    auto sheet = std::make_unique<QWidget>();
    sheet->setObjectName(QStringLiteral("controlsSheet"));
    sheet->setAttribute(Qt::WA_DontShowOnScreen, true);
    sheet->setAutoFillBackground(true);
    QPalette pal = sheet->palette();
    pal.setColor(QPalette::Window, QColor(QLatin1String(kSheetBg)));
    sheet->setPalette(pal);

    struct Placed {
        QLabel* caption;
        QLabel* image;
        int width;
        int height;
    };
    // Build every label first, then place them.
    std::vector<std::array<QVector<Placed>, kLookCount>> placed(rows.rows.size());
    std::array<int, kLookCount> columnWidth{kMinColumnWidth, kMinColumnWidth, kMinColumnWidth};
    int captionHeight = 0;
    for (std::size_t r = 0; r < rows.rows.size(); ++r) {
        for (int l = 0; l < kLookCount; ++l) {
            int width = 0;
            for (const Shot& s : rows.rows[r].looks[l]) {
                QLabel* cap = textLabel(sheet.get(), s.caption, kSheetCaptionColour, 10);
                auto* img = new QLabel(sheet.get());
                img->setPixmap(QPixmap::fromImage(s.image));
                const QSize logical = logicalSizeOf(s.image);
                img->setFixedSize(logical);
                const int w = qMax(logical.width(), cap->width());
                placed[r][l].append({cap, img, w, logical.height()});
                captionHeight = qMax(captionHeight, cap->height());
                width += (width > 0 ? kShotGapPx : 0) + w;
            }
            columnWidth[l] = qMax(columnWidth[l], width);
        }
    }

    QLabel* titleLabel = textLabel(sheet.get(), title, kSheetTitleColour, 15, true);
    titleLabel->move(kSheetMarginPx, kSheetMarginPx);
    QLabel* subtitleLabel = textLabel(sheet.get(), subtitle, kSheetTextColour, 11);
    subtitleLabel->move(kSheetMarginPx, titleLabel->geometry().bottom() + 4);
    int y = subtitleLabel->geometry().bottom() + 14;

    std::array<int, kLookCount> columnX{};
    int x = kSheetMarginPx + kRowLabelWidth + kColumnGapPx;
    for (int l = 0; l < kLookCount; ++l) {
        columnX[l] = x;
        QLabel* head = textLabel(sheet.get(), lookTitle(l), kSheetTitleColour, 12, true);
        head->move(x, y);
        x += columnWidth[l] + kColumnGapPx;
    }
    const int sheetWidth = x - kColumnGapPx + kSheetMarginPx;
    y += 24;

    for (std::size_t r = 0; r < rows.rows.size(); ++r) {
        const Row& row = rows.rows[r];
        auto* rule = new QFrame(sheet.get());
        rule->setStyleSheet(QStringLiteral("QFrame { background: %1; border: none; }").arg(QLatin1String(kSheetRule)));
        rule->setGeometry(kSheetMarginPx, y, sheetWidth - 2 * kSheetMarginPx, 1);
        y += kRowGapPx / 2;
        QLabel* name = textLabel(sheet.get(), row.title, kSheetTextColour, 11, true);
        name->setWordWrap(true);
        name->setFixedWidth(kRowLabelWidth);
        name->adjustSize();
        name->move(kSheetMarginPx, y + captionHeight);
        int labelBottom = name->geometry().bottom();
        if (!row.note.isEmpty()) {
            QLabel* note = textLabel(sheet.get(), row.note, kSheetCaptionColour, 10);
            note->setWordWrap(true);
            note->setFixedWidth(kRowLabelWidth);
            note->adjustSize();
            note->move(kSheetMarginPx, labelBottom + 4);
            labelBottom = note->geometry().bottom();
        }
        int rowHeight = 0;
        for (int l = 0; l < kLookCount; ++l) {
            int sx = columnX[l];
            for (int i = 0; i < placed[r][l].size(); ++i) {
                const Placed& p = placed[r][l][i];
                const Shot& s = row.looks[l][i];
                p.caption->move(sx, y);
                p.image->move(sx, y + captionHeight + 2);
                const double dpr = s.image.devicePixelRatio();
                QJsonObject at{
                    {QStringLiteral("row"), row.key},
                    {QStringLiteral("caption"), s.caption},
                    {QStringLiteral("look"), lookKey(l)},
                    {QStringLiteral("sheet_logical_rect"), rectArray(p.image->geometry())},
                    {QStringLiteral("sheet_device_rect"), rectArray(toDevice(p.image->geometry(), dpr))},
                };
                if (!s.source.isEmpty()) {
                    at.insert(QStringLiteral("crop_source"), s.source);
                }
                placements.append(at);
                rowHeight = qMax(rowHeight, captionHeight + 2 + p.height);
                sx += p.width + kShotGapPx;
            }
        }
        y = qMax(y + rowHeight, labelBottom) + kRowGapPx;
    }
    sheet->resize(sheetWidth, y + kSheetMarginPx);
    sheet->show();
    pump(kSettleMs);
    return grabImage(sheet.get());
}

// The zoom rows: each handle or indicator, 5x nearest-neighbour.
Rows zoomRows(const Rows& rows)
{
    Rows out;
    for (const Row& row : rows.rows) {
        for (int l = 0; l < kLookCount; ++l) {
            for (const Shot& s : row.looks[l]) {
                if (s.focus.isEmpty()) {
                    continue;
                }
                const double dpr = s.image.devicePixelRatio();
                const QRect device = toDevice(s.focus, dpr).intersected(s.image.rect());
                QImage cut = s.image.copy(device);
                QImage big = cut.scaled(cut.width() * kZoomFactor, cut.height() * kZoomFactor,
                                        Qt::IgnoreAspectRatio, Qt::FastTransformation);
                big.setDevicePixelRatio(dpr);
                Shot z;
                z.caption = s.caption;
                z.image = big;
                z.source = QJsonObject{{QStringLiteral("cell_device_rect"), rectArray(device)}};
                out.row(row.key, row.title).looks[l].append(z);
            }
        }
    }
    return out;
}

QJsonArray controlHints(const Rows& rows)
{
    QJsonArray out;
    for (const Row& row : rows.rows) {
        const int n = static_cast<int>(row.looks[kToday].size());
        for (int i = 0; i < n; ++i) {
            const Shot& t = row.looks[kToday][i];
            if (t.sizeHint.isEmpty()) {
                continue;
            }
            QJsonObject o{{QStringLiteral("row"), row.key}, {QStringLiteral("caption"), t.caption}};
            for (int l = 0; l < kLookCount; ++l) {
                if (i < row.looks[l].size()) {
                    const Shot& s = row.looks[l][i];
                    o.insert(lookKey(l), QJsonObject{{QStringLiteral("size_hint"), sizeArray(s.sizeHint)},
                                                     {QStringLiteral("minimum_size_hint"), sizeArray(s.minimumHint)},
                                                     {QStringLiteral("cell_logical"), sizeArray(logicalSizeOf(s.image))}});
                }
            }
            out.append(o);
        }
    }
    return out;
}

bool rowsAgree(const Rows& rows, QString& why)
{
    for (const Row& row : rows.rows) {
        for (int l = 1; l < kLookCount; ++l) {
            if (row.looks[l].size() != row.looks[kToday].size()) {
                why = QStringLiteral("row %1: %2 has %3 shots, today %4")
                          .arg(row.key, lookKey(l))
                          .arg(row.looks[l].size())
                          .arg(row.looks[kToday].size());
                return false;
            }
        }
    }
    return true;
}

} // namespace

void sceneControlsSheet(SceneContext& ctx)
{
    ctx.provenance = QStringLiteral("proposed");
    QApplication* a = app();
    if (a == nullptr || ctx.window == nullptr) {
        ctx.failure = QStringLiteral("controls-sheet: no application or window");
        return;
    }
    if (WorkspacePreview::enabled()) {
        ctx.failure = QStringLiteral("controls-sheet: must start with the preview switch off");
        return;
    }
    QStyleHints* hints = QGuiApplication::styleHints();
    const int flashTime = hints->cursorFlashTime();
    hints->setCursorFlashTime(0);   // a steady caret, so focused fields grab the same every run

    const AppLook today{a->palette(), a->styleSheet()};
    const LookSheets todayStrings = todaySheets();

    Panels panels;
    if (auto* manager = ctx.window->findChild<ContainerManager*>()) {
        panels.mainPanel = manager->panelContainer();
    }
    panels.rx = ctx.window->findChild<RxApplet*>();
    panels.tx = ctx.window->findChild<TxApplet*>();
    panels.phone = ctx.window->findChild<PhoneCwApplet*>();
    panels.mainWindowSheet = ctx.window->styleSheet();
    if (auto* action = ctx.window->findChild<QAction*>(QStringLiteral("toolsTxEqualizer"))) {
        action->trigger();
        pump(kSettleMs);
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* d = qobject_cast<TxEqDialog*>(w); d != nullptr && d->isVisible()) {
                panels.eq = d;
            }
        }
    }
    if (panels.eq.isNull()) {
        ctx.notes.append(QStringLiteral("the TX EQ dialog did not open; no EQ crops"));
    } else {
        for (const QJsonValue& v : stopTimersUnder(panels.eq)) {
            ctx.stoppedTimers.append(v);
        }
    }
    snapshot(panels.mainPanel, panels);
    snapshot(panels.eq, panels);

    Rows rows;
    for (int look = 0; look < kLookCount; ++look) {
        // Today's pass changes nothing: the window is as the baseline
        // scene left it.
        if (look != kToday) {
            applyLook(look, today);
            restoreSheets(panels);
        }
        if (look == kPreview) {
            ctx.window->setStyleSheet(QssTranslator::translateWithAmbiguousRole(
                                          panels.mainWindowSheet, QStringLiteral("border-subtle")).qss);
            WorkspacePreview::themeSubtree(panels.mainPanel);
            WorkspacePreview::themeSubtree(panels.eq);
        } else if (look == kLongpath) {
            ctx.window->setStyleSheet(LongpathLook::themed(panels.mainWindowSheet));
            longpathPanels(panels);
        }
        // Panels first: the RX crop in today's look is the baseline
        // window's own pixels.
        capturePanels(ctx, look, panels, rows);

        LookSheets sheets = todayStrings;
        if (look == kPreview) {
            QWidget probe;
            sheets = previewSheets(todayStrings,
                                   probe.style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, &probe));
        } else if (look == kLongpath) {
            sheets = longpathSheets();
        }
        captureControls(ctx, look, sheets, rows);
    }

    // Back to today's look, switch off.
    applyLook(kToday, today);
    restoreSheets(panels);
    ctx.window->setStyleSheet(panels.mainWindowSheet);
    pump(kSettleMs);
    if (!panels.eq.isNull()) {
        panels.eq->close();
    }
    hints->setCursorFlashTime(flashTime);

    // Panel rows go last on the sheet.
    QVector<Row> ordered;
    for (const Row& r : rows.rows) {
        if (!r.key.startsWith(QLatin1String("panel-"))) {
            ordered.append(r);
        }
    }
    for (const Row& r : rows.rows) {
        if (r.key.startsWith(QLatin1String("panel-"))) {
            ordered.append(r);
        }
    }
    rows.rows = ordered;
    for (Row& r : rows.rows) {
        if (r.key == QLatin1String("panel-rx-viewport")) {
            r.note = QStringLiteral("What the window shows: the RX applet is wider than the Main Panel "
                                    "viewport, so its right edge is cut. This window keeps today's layout in "
                                    "every column; strip-fit-preview measures the preview's own right strip.");
        } else if (r.key == QLatin1String("panel-rx") || r.key == QLatin1String("panel-tx")
                   || r.key == QLatin1String("panel-mic")) {
            r.note = QStringLiteral("Grabbed from the applet itself at its full width, so every column shows "
                                    "the whole applet.");
        }
    }
    QString why;
    if (!rowsAgree(rows, why)) {
        ctx.failure = QStringLiteral("controls-sheet: the looks captured different shots: %1").arg(why);
        return;
    }

    QJsonArray placements;
    CapturedImage sheet;
    sheet.name = QStringLiteral("controls-sheet");
    sheet.kind = QStringLiteral("sheet");
    sheet.image = assemble(rows, QStringLiteral("Controls: today, preview now, Longpath"),
                           QStringLiteral("Real Qt widgets at the same fixed sizes in every column. Panel rows are "
                                          "crops of the live main window and TX EQ dialog in each look."),
                           placements);
    sheet.dpr = sheet.image.devicePixelRatio();
    sheet.logicalSize = logicalSizeOf(sheet.image);
    sheet.extra[QStringLiteral("window")] = QStringLiteral("its own top-level QWidget, not the MainWindow");
    sheet.extra[QStringLiteral("placements")] = placements;
    sheet.extra[QStringLiteral("controls")] = controlHints(rows);
    sheet.extra[QStringLiteral("looks")] = QJsonObject{
        {QStringLiteral("today"), QStringLiteral("Fusion, applyDarkPalette, applyAppBaselineQss; today's "
                                                 "stylesheets; preview switch off")},
        {QStringLiteral("preview"), QStringLiteral("switch on, applyWorkspacePalette; today's stylesheets "
                                                   "through WorkspacePreview::themeSubtree (QssTranslator then "
                                                   "Workspace::Components::componentize)")},
        {QStringLiteral("longpath"), QStringLiteral("Fusion palette, Longpath applyDarkPalette and "
                                                    "applyAppBaselineQss; Longpath's control stylesheets "
                                                    "(tests/visual/LongpathControlStyles.h)")},
    };
    sheet.extra[QStringLiteral("column_provenance")] = QJsonObject{
        {QStringLiteral("today"), QStringLiteral("retained-current-app")},
        {QStringLiteral("preview"), QStringLiteral("proposed-look")},
        {QStringLiteral("longpath"), QStringLiteral("longpath-port")},
    };
    ctx.images.append(sheet);

    QJsonArray zoomPlacements;
    CapturedImage zoom;
    zoom.name = QStringLiteral("controls-sheet--zoom");
    zoom.kind = QStringLiteral("zoom");
    zoom.image = assemble(zoomRows(rows), QStringLiteral("Handles and indicators, 5x nearest-neighbour"),
                          QStringLiteral("Each cut-out is %1x%2 logical px of the control cell around the "
                                         "handle or indicator, enlarged 5x.")
                              .arg(kZoomSourceWidth)
                              .arg(kZoomSourceHeight),
                          zoomPlacements);
    zoom.dpr = zoom.image.devicePixelRatio();
    zoom.logicalSize = logicalSizeOf(zoom.image);
    zoom.extra[QStringLiteral("source")] = sheet.name;
    zoom.extra[QStringLiteral("enlargement")] = QStringLiteral("5x nearest-neighbour");
    zoom.extra[QStringLiteral("placements")] = zoomPlacements;
    ctx.images.append(zoom);
}

// ---- strip fit ---------------------------------------------------------------

namespace {

bool isApplet(const QWidget* w)
{
    return QString::fromLatin1(w->metaObject()->className()).endsWith(QLatin1String("Applet"));
}

// Applets under `body` that are not inside another applet.
QList<QWidget*> outerApplets(QWidget* body)
{
    QList<QWidget*> out;
    for (QWidget* w : body->findChildren<QWidget*>()) {
        if (!isApplet(w) || !w->isVisibleTo(body)) {
            continue;
        }
        bool nested = false;
        for (QWidget* p = w->parentWidget(); p != nullptr && p != body; p = p->parentWidget()) {
            if (isApplet(p)) {
                nested = true;
                break;
            }
        }
        if (!nested) {
            out.append(w);
        }
    }
    return out;
}

int minWidthOf(const QWidget* w)
{
    return qMax(w->minimumWidth(), w->minimumSizeHint().width());
}

} // namespace

QJsonObject measurePanelFit(SceneContext& ctx, const QString& look)
{
    QJsonObject out{{QStringLiteral("look"), look}};
    auto* manager = ctx.window->findChild<ContainerManager*>();
    QWidget* panel = manager != nullptr ? manager->panelContainer() : nullptr;
    if (panel == nullptr) {
        out[QStringLiteral("error")] = QStringLiteral("no Main Panel container");
        return out;
    }
    out[QStringLiteral("panel_rect")] = rectArray(rectIn(ctx.window, panel));
    QJsonArray areas;
    for (QScrollArea* area : panel->findChildren<QScrollArea*>()) {
        QWidget* body = area->widget();
        if (body == nullptr || !area->isVisible()) {
            continue;
        }
        QWidget* vp = area->viewport();
        QScrollBar* v = area->verticalScrollBar();
        QScrollBar* h = area->horizontalScrollBar();
        QStringList chain;
        for (QWidget* p = area->parentWidget(); p != nullptr && p != panel->parentWidget(); p = p->parentWidget()) {
            chain.append(QString::fromLatin1(p->metaObject()->className()));
        }
        QJsonArray applets;
        for (QWidget* a : outerApplets(body)) {
            const QPoint at = a->mapTo(vp, QPoint(0, 0));
            const int minW = minWidthOf(a);
            applets.append(QJsonObject{
                {QStringLiteral("class"), QString::fromLatin1(a->metaObject()->className())},
                {QStringLiteral("x_in_viewport"), at.x()},
                {QStringLiteral("width"), a->width()},
                {QStringLiteral("minimum_width"), minW},
                {QStringLiteral("size_hint_width"), a->sizeHint().width()},
                {QStringLiteral("clipped_left_px"), qMax(0, -at.x())},
                {QStringLiteral("clipped_right_px"), qMax(0, at.x() + a->width() - vp->width())},
                {QStringLiteral("minimum_over_viewport_px"), qMax(0, minW - vp->width())},
            });
        }
        areas.append(QJsonObject{
            {QStringLiteral("parents"), chain.join(QStringLiteral(" < "))},
            {QStringLiteral("area_width"), area->width()},
            {QStringLiteral("viewport_width"), vp->width()},
            {QStringLiteral("vertical_bar_visible"), v->isVisible()},
            {QStringLiteral("vertical_bar_width"), v->isVisible() ? v->width() : 0},
            {QStringLiteral("horizontal_policy"),
             area->horizontalScrollBarPolicy() == Qt::ScrollBarAlwaysOff ? QStringLiteral("always off")
                                                                          : QStringLiteral("as needed")},
            {QStringLiteral("horizontal_value"), h->value()},
            {QStringLiteral("horizontal_maximum"), h->maximum()},
            {QStringLiteral("body_width"), body->width()},
            {QStringLiteral("body_minimum_width"), minWidthOf(body)},
            {QStringLiteral("body_over_viewport_px"), qMax(0, body->width() - vp->width())},
            {QStringLiteral("applets"), applets},
        });
    }
    out[QStringLiteral("scroll_areas")] = areas;
    return out;
}

void appendPanelFitCrop(SceneContext& ctx, const QString& scene, const QJsonObject& fit)
{
    for (CapturedImage& img : ctx.images) {
        if (img.kind == QLatin1String("window")) {
            img.extra[QStringLiteral("panel_fit")] = fit;
        }
    }
    auto* manager = ctx.window->findChild<ContainerManager*>();
    QWidget* panel = manager != nullptr ? manager->panelContainer() : nullptr;
    if (panel == nullptr || !panel->isVisible()) {
        ctx.notes.append(QStringLiteral("strip fit: no visible Main Panel container to crop"));
        return;
    }
    const QRect r = rectIn(ctx.window, panel);
    CapturedImage c;
    c.name = scene + QStringLiteral("--panel");
    c.kind = QStringLiteral("crop");
    c.image = grabImage(ctx.window, r);
    c.dpr = c.image.devicePixelRatio();
    c.logicalSize = r.size();
    c.extra[QStringLiteral("source")] = scene;
    c.extra[QStringLiteral("logical_rect")] = rectArray(r);
    c.extra[QStringLiteral("device_rect")] = rectArray(toDevice(r, c.dpr));
    c.extra[QStringLiteral("panel_fit")] = fit;
    ctx.images.append(c);
}

} // namespace NereusSDR::Preview
