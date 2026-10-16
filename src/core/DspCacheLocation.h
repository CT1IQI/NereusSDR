#pragma once
// no-port-check: NereusSDR-original. Where a process keeps its saved FFT plans.

#include <QString>

namespace NereusSDR::DspCacheLocation {

// The folder this process keeps its saved FFT plans (WDSP's wdspWisdom
// file) and its impulse cache in. Building the plans takes minutes, and a
// radio connection waits for them.
//
// Left alone it is the program's own configuration folder, which carries
// the program's name: the app and nereusd each have their own. A background
// Core started by the app is the app's own Core carried on, so the app
// names its folder in the entry that starts it (StationServiceManager) and
// nereusd takes it from --dsp-cache-dir. Without that the background Core
// built every plan again while the radio waited.
QString directory();

// Names the folder for this process; an empty value goes back to the
// program's own. Called at startup, before a radio connects.
void setDirectory(const QString& directory);

} // namespace NereusSDR::DspCacheLocation
