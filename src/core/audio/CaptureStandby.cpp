// =================================================================
// src/core/audio/CaptureStandby.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See CaptureStandby.h.
//
// Modification history (NereusSDR):
//   2026-10-16: the interface only; nothing is held or started yet.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureStandby.h"

#include <QTimer>

namespace NereusSDR {

CaptureStandby::CaptureStandby(CaptureSupervisor::Options options, int warmWaitMs,
                               QObject* parent)
    : QObject(parent)
    , m_supervisor(std::make_unique<CaptureSupervisor>(std::move(options)))
    , m_bound(new QTimer(this))
{
    Q_UNUSED(warmWaitMs);
}

CaptureStandby::~CaptureStandby()
{
    m_supervisor->shutdown();
}

void CaptureStandby::whenWarm(QObject* context, std::function<void()> work)
{
    Q_UNUSED(context);
    Q_UNUSED(work);
}

void CaptureStandby::release(const QString& why)
{
    Q_UNUSED(why);
}

} // namespace NereusSDR
