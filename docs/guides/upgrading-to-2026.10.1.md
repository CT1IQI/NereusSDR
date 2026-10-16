# Upgrading to NereusSDR 2026.10.1

Use this note when updating a station from 2026.10.0. It lists what changes
when you first launch, what to do on a Core, and a short list of things to
try. If you are coming from 0.5.2, read the
[2026.10.0 checklist](upgrading-to-2026.10.0.md) first. When reporting a
problem, include the installed version, operating system, radio model and
firmware, and whether you use a local or remote Core.

## Update the Core and the desktop together

The Core owns the radio, the station DSP and the accessories. The console runs
where you operate. Update both. Rotor control, the **CAT Control** page on the
phone and the Core speaker need a 2026.10.1 Core. An older Core grays them out
with a reason such as "This Core does not control a rotor. Updating the Core
may help.", and older Cores, desktops and phone apps still connect to each
other.

## What changes on first launch

Keep a backup of your settings before the first launch, and close the console
normally so the new version can migrate them once.

- **Two-tone is one setting.** The two-tone level (**Test > Two-Tone IMD**) is
  now a single value for the radio, and TX profiles no longer save any
  two-tone values. If your saved level was the old default of -6 dB, it
  becomes 0 dB. That is 6 dB more two-tone drive than before for anyone who
  never changed it, and it lets PureSignal calibrate. **If you use PureSignal,
  check your two-tone level and drive before the first calibration.** Any saved
  level other than -6 dB is left alone.
- **Audio choices move to the new Driver list.** Each device card now has one
  **Driver** list above **Device**, replacing the Driver API list and the
  Windows audio checkboxes. Your saved choices carry over once, and the old
  settings are kept. On a headless Linux Core, every profile moves to ALSA
  direct. After the first launch, open **File > Settings... > Audio** and
  confirm that your speaker and microphone picks are still the ones you
  expect.
- **The workspace look is the default.** Containers dock on all four edges,
  headers show or hide with an eye button, and there is no container **Lock**
  with this look. Locks you saved earlier open unlocked. Your saved layout is
  copied the first time you start, so nothing is lost.
- **Attenuator and preamp are remembered.** They are saved and sent to the
  radio each time you connect. Connecting never keys the radio.
- **Each receiver keeps more of its own settings.** L/R pan, binaural,
  squelch, APF, the FM settings, the DIGL and DIGU offsets and the RTTY mark
  and shift are now saved for each receiver, so they start saving from the
  first launch. AGC-T AUTO and its offset move from each band to each
  receiver: the first time, a receiver takes the value it had saved for the
  band it opens on. Receivers B, C and D now run on the settings they show,
  so one whose saved settings differ from the defaults may sound different
  from before.
- **FM squelch reads 0 to 100.** **Squelch Threshold** on
  **File > Settings... > DSP > FM** now shows 0 to 100, where 100 is the
  tightest, instead of dB. Your saved threshold is kept: the old starting
  value now reads 6, and with FM squelch on it now opens. A new receiver
  starts at 100.

## Core installs on a Raspberry Pi, Rock or other SBC

The Core runs as a service account with no groups of its own, so Linux sound
cards and serial adapters each need a small systemd drop-in.

- **Audio.** Every SBC install and every `.deb` package now ships or writes the
  audio drop-in. An older install that lacks it cannot open a sound card, so
  the Core speaker will not work until you add
  `/etc/systemd/system/nereusd.service.d/audio.conf` with `[Service]` and
  `SupplementaryGroups=audio`.
- **Serial.** A rotor on a GS-232 adapter, or serial CAT on the Core, needs
  `/etc/systemd/system/nereusd.service.d/serial.conf` with `[Service]` and
  `SupplementaryGroups=dialout`. The Pi 4 and Rock installers and
  the station image write it; the
  `.deb` packages do not. After adding either file, run
  `sudo systemctl daemon-reload` and `sudo systemctl restart nereusd`.
- **Rock real-time priority.** On a Rock, run the 2026.10.1 installer once and
  then restart the board. It adds a drop-in meant to keep an ssh login from
  costing the Core its real-time priority. See What to try.

The [SBC install guide](install-core-sbc.md) has the details, including the
sound card's own mixer and kernels that refuse real-time priority.
Building the PulseAudio engine from source on Linux needs
`libpulse-dev`.

## What to try

Each item is a short check you can do in a few minutes. Note the result,
including anything that surprised you.

1. **Rotor.** Open **File > Settings... > CAT & Network > Rotor**, choose your
   controller (**Yaesu GS-232A, serial port**, **Yaesu GS-232B, serial port**, **Hamlib rotctld, already running** or **Hamlib rotctld, started by the Core**), set the
   end stop and travel, and select **Save and connect**. Open the Rotor
   applet, turn to a heading, stop it, and try a preset. Right-click a spot
   and choose **Turn beam to** the call. On the iPhone or iPad, open the Rotor
   page and confirm it shows the same heading. Release a held turn button and
   confirm the rotor stops.
2. **CAT.** Open **Tools > CAT Control...** (or the CAT pages in **File >
   Settings... > CAT & Network**), assign a channel to a slice, enable one
   listener, and connect a logging or digital-mode program. Change frequency
   from the program and from NereusSDR and confirm both follow. Try the
   Hamlib rigctld port with a program that speaks it. On a headless Core, set
   CAT up from a connected desktop or from the phone's Tools tab. Confirm
   that a refused setting says why.
3. **Native audio.** Check that the **Driver** lists show your system's audio
   system and that devices appear and disappear as you plug them in. With a
   Core, pick the **Core speaker** in **Audio > Outputs** and confirm the
   level and mute match the phone's Sound panel. Disconnect and reconnect your
   microphone and confirm transmit audio returns.
4. **Workspace look.** Hide and show a container header with the eye button.
   Hold Shift (or press Ctrl+Shift+A) to arrange, dock a container on each
   edge, pop one out, pin it and return it. Save a workspace profile and
   switch to it with **Containers > Manage Workspace Profiles…**. Choose
   **Reset Default Layout** and confirm the S-Meter comes back.
5. **Receivers after a relaunch.** Give two receivers different squelch, L/R
   pan, APF and AGC-T AUTO settings, close NereusSDR normally, open it again
   and confirm each receiver comes back as you left it and sounds the way it
   did.
6. **Hermes Lite 2 through a Pi Core.** Key the radio from a window or the
   phone through a Core on a Raspberry Pi and hold it for half a minute.
   Confirm the radio stays keyed for the whole time.
7. **Rock real-time priority.** After installing on a Rock and restarting it,
   log in over ssh and run `cat /sys/fs/cgroup/cgroup.subtree_control`.
   Confirm that `cpu` is not in the list, and that the Core's log has no
   "Raised thread priority was refused" line from after the restart.
8. **Two-tone and PureSignal.** If you use PureSignal, run a two-tone test and
   a calibration at your new two-tone level and compare it with what you
   expect.

## Reporting

Report expected behavior, actual behavior, a minimal reproduction, desktop and
Core versions, platform, radio model and firmware, the connection path and a
support bundle. Keep credentials and pairing tokens out of public reports.
Bench results for physical serial ports, a physical rotor and logging
programs on Windows and Linux are still open. Say which hardware and program
you used.
