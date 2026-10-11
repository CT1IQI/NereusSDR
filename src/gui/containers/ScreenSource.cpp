// no-port-check: NereusSDR-original list of the screens the containers code consults.
// Modification history (NereusSDR):
//   2026-10-11 - Created by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
#include "ScreenSource.h"

#include <QGuiApplication>
#include <QScreen>

namespace NereusSDR {

namespace {
ScreenInfo infoOf(const QScreen* screen)
{
    if (screen == nullptr) {
        return {};
    }
    return {screen->name(), screen->serialNumber(), screen->geometry(), screen->availableGeometry()};
}
} // namespace

bool screenMatches(const ScreenInfo& screen, const SavedScreen& saved)
{
    if (!saved.serial.isEmpty() && saved.serial == screen.serial) {
        return true;
    }
    return saved.name == screen.name && saved.geometry == screen.geometry;
}

SavedScreen savedScreenOf(const ScreenInfo& screen)
{
    return {screen.name, screen.serial, screen.geometry};
}

ScreenSource::~ScreenSource() = default;

QVector<ScreenInfo> ScreenSource::screens() const
{
    QVector<ScreenInfo> list;
    const QList<QScreen*> present = QGuiApplication::screens();
    for (const QScreen* screen : present) {
        if (screen != nullptr) {
            list.append(infoOf(screen));
        }
    }
    return list;
}

ScreenInfo ScreenSource::primary() const
{
    return infoOf(QGuiApplication::primaryScreen());
}

std::optional<ScreenInfo> ScreenSource::matching(const SavedScreen& saved) const
{
    const QVector<ScreenInfo> present = screens();
    for (const ScreenInfo& screen : present) {
        if (screenMatches(screen, saved)) {
            return screen;
        }
    }
    return std::nullopt;
}

std::optional<ScreenInfo> ScreenSource::screenHolding(const QRect& rect) const
{
    std::optional<ScreenInfo> best;
    qint64 bestArea = 0;
    const QVector<ScreenInfo> present = screens();
    for (const ScreenInfo& screen : present) {
        const QRect shared = screen.geometry.intersected(rect);
        const qint64 area = shared.isValid() ? qint64(shared.width()) * shared.height() : 0;
        if (area > bestArea) {
            bestArea = area;
            best = screen;
        }
    }
    return best;
}

ScreenInfo ScreenSource::screenFor(const QRect& rect) const
{
    const std::optional<ScreenInfo> holding = screenHolding(rect);
    return holding ? *holding : primary();
}

bool ScreenSource::onAnyScreen(const QRect& rect) const
{
    return screenHolding(rect).has_value();
}

ScreenSource* ScreenSource::system()
{
    static ScreenSource source;
    return &source;
}

} // namespace NereusSDR
