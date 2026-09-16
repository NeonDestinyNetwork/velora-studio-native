# Velora Studio (Native PRISM / OBS Custom Edition)

Custom broadcast client powered by the **PRISM Live Studio / OBS Studio** engine, tailored for the **Velora Network**.

---

## 🌟 Key Features

- **Velora Cosmic Aesthetic**: Deep dark obsidian (`#040817`) theme with Velora Gold (`#D4AF37`) accents and glow effects.
- **Native WHIP & RTMP Support**: Pre-configured Velora Live Ingest endpoints (`https://publish.velora.tv/live/<key>?direction=whip`).
- **Hardware Optimization**: Built-in 2.0s GOP keyframe locking, 0 B-frames, and 48kHz Opus audio for zero-warning broadcast compliance.
- **Full Studio Power**: Multi-scene composition, screen/game capture, audio mixers, VSTs, and Velora live chat dock.

---

## 🚀 Building Velora Studio on GitHub Actions (Automated)

1. Push this folder to your GitHub repository (e.g. `github.com/veloratv/velora-studio`).
2. Go to the **Actions** tab in GitHub.
3. Run the **`Build Velora Studio (PRISM Base - Windows)`** workflow.
4. Download the compiled `Velora-Studio-Windows-x64` zip containing the ready-to-run Windows client.

---

## 🛠 Local Build Requirements (Windows)

- Visual Studio 2022 (with C++ Desktop Development workload)
- Qt 6.3.1 (`win64_msvc2019_64`)
- CMake 3.24+ & Ninja
