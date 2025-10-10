<div align="center">
  <img src="assets/logo.png" alt="Project Logo", height="200">

> A CLI tool that brings realistic mechanical keyboard sounds to every keystroke.

KeyVibe listens to your keyboard events and plays high‑quality switch sounds with low latency. It ships with multiple sound packs and supports your own custom packs. Run it interactively or as a background daemon with live reload of your preferences.

</div>

<div align="center">

[![Contributors](https://img.shields.io/github/contributors/S4NKALP/KeyVibe?style=for-the-badge&color=6366f1)](https://github.com/S4NKALP/KeyVibe/graphs/contributors)
[![Stars](https://img.shields.io/github/stars/S4NKALP/KeyVibe?style=for-the-badge&color=10b981)](https://github.com/S4NKALP/KeyVibe/stargazers)
[![Forks](https://img.shields.io/github/forks/S4NKALP/KeyVibe?style=for-the-badge&color=06b6d4)](https://github.com/S4NKALP/KeyVibe/network/members)
[![License](https://img.shields.io/github/license/S4NKALP/KeyVibe?style=for-the-badge&color=f59e0b)](LICENSE)
[![Issues](https://img.shields.io/github/issues/S4NKALP/KeyVibe?style=for-the-badge&color=ef4444)](https://github.com/S4NKALP/KeyVibe/issues)
[![Last Commit](https://img.shields.io/github/last-commit/S4NKALP/KeyVibe?style=for-the-badge&color=8b5cf6)](https://github.com/S4NKALP/KeyVibe/pulse)

[**Report Bug**](https://github.com/S4NKALP/KeyVibe/issues) • [**Request Feature**](https://github.com/S4NKALP/KeyVibe/discussions)

</div>

---

## ✨ Features

- Real-time key press/release sound playback
- Multiple bundled sound packs
- User sound packs under `~/.local/share/keyvibe/audio/`
- Background daemon mode with PID file and safe shutdown
- Live config reload on `~/.keyvibe.json` changes (daemon mode)

## 📋 Dependencies

- build-essential
- pkg-config
- libjson-c-dev
- libpulse-dev
- libsndfile1-dev
- libinput-dev
- libevdev-dev
- libudev-d

## 🚀 Installation

### Arch Linux

```bash
paru -S keyvibe
```

### 🛠️ Build From Source

```bash
git clone https://github.com/S4NKALP/KeyVibe.git
cd KeyVibe
make
sudo make install
```

## 💡 Usage

    Usage: keyvibe [OPTIONS]

    Options:
      -S, --sound SOUND_NAME        Select sound pack (default: eg-oreo)
      -V, --volume VOLUME           Set volume [0-100] (default: 50)
      -c --override-config          Use CLI values for this run
      -d --daemon                   Run as a background daemon
      -s --stop                     Stop the background daemon
      -m --mute                     Mute sound
      -u --unmute                   Unmute sound
      -l, --list                    List available sound packs
      -h, --help                    Show this help message
      -v, --verbose                 Enable verbose output

Notes:

- First run creates `~/.keyvibe.json`. Subsequent runs use it unless `-c` is supplied.
- In daemon mode, editing `~/.keyvibe.json` will automatically reload.

## 🎵 Sound Packs

System packs are installed under: `/usr/share/keyvibe/audio/`

User packs go under: `~/.local/share/keyvibe/audio/<pack-name>/`

Required files inside each `<pack-name>/` directory:

- `config.json` (format matches bundled packs)
- Audio files referenced by `config.json`

## 🤝 Contributing

PRs welcome. Please keep code modular and readable, and prefer small, focused changes.

## 📄 License

MIT — see [LICENSE](LICENSE).

## 🙏 Credits

- [MechVibes](https://github.com/hainguyents13/mechvibes) for sound packs
- [showmethekey](https://github.com/AlynxZhou/showmethekey) for code
