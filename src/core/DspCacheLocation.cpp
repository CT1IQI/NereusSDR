// no-port-check: NereusSDR-original. Where a process keeps its saved FFT plans.

#include "core/DspCacheLocation.h"

#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

namespace NereusSDR::DspCacheLocation {

namespace {

QMutex s_mutex;
QString s_directory;

} // namespace

QString directory()
{
    {
        const QMutexLocker locker(&s_mutex);
        if (!s_directory.isEmpty()) {
            return s_directory;
        }
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

void setDirectory(const QString& directory)
{
    const QMutexLocker locker(&s_mutex);
    s_directory = directory.isEmpty() ? QString() : QDir::cleanPath(directory);
}

} // namespace NereusSDR::DspCacheLocation
