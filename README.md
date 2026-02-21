<div align="center">

# vbx

> A CLI tool that brings realistic mechanical keyboard and mouse sounds to every keystroke.

vbx listens to your keyboard and mouse events and plays high‑quality switch sounds with low latency. It ships with multiple sound packs for both devices and supports your own custom packs. Run it interactively or as a background daemon with live reload of your references.

</div>
<div align="center">

[![Contributors](https://img.shields.io/github/contributors/S4NKALP/vbx?style=for-the-badge&color=6366f1)](https://github.com/S4NKALP/vbx/graphs/contributors)
[![Stars](https://img.shields.io/github/stars/S4NKALP/vbx?style=for-the-badge&color=10b981)](https://github.com/S4NKALP/vbx/stargazers)
[![Forks](https://img.shields.io/github/forks/S4NKALP/vbx?style=for-the-badge&color=06b6d4)](https://github.com/S4NKALP/vbx/network/members)
[![License](https://img.shields.io/github/license/S4NKALP/vbx?style=for-the-badge&color=f59e0b)](LICENSE)
[![Issues](https://img.shields.io/github/issues/S4NKALP/vbx?style=for-the-badge&color=ef4444)](https://github.com/S4NKALP/vbx/issues)
[![Last Commit](https://img.shields.io/github/last-commit/S4NKALP/vbx?style=for-the-badge&color=8b5cf6)](https://github.com/S4NKALP/vbx/pulse)

[**Report Bug**](https://github.com/S4NKALP/vbx/issues) • [**Request Feature**](https://github.com/S4NKALP/vbx/discussions)

</div>

---

## ✨ Features

- Real-time keyboard key press/release sound playback
- Real-time mouse click sound playback
- Multiple bundled sound packs for both keyboard and mouse
- User sound packs under `~/.local/share/vbx/audio/`
- Background daemon mode with PID file and safe shutdown
- Live config reload on `~/.vbx.json` changes (daemon mode)

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
paru -S vbx-git
```

### 🛠️ Build From Source

```bash
git clone https://github.com/S4NKALP/vbx.git
cd vbx
make
sudo make install
```

## 💡 Usage

    Usage: vbx [OPTIONS]

    Options:
      -S, --sound SOUND_NAME        Select sound pack (default: eg-oreo)
      -V, --volume VOLUME           Set volume [0-100] (default: 50)
      -d --daemon                   Run as a background daemon
      -s --stop                     Stop the background daemon
      -m --mute                     Mute sound
      -u --unmute                   Unmute sound
      -- enbale/disable DEVICE_NAME Enable/disable a device
      -l, --list                    List available sound packs
      -f, --system-volume-following=0/1 Enable/disable dynamic tracking of system volume (default: 0)
      -h, --help                    Show this help message
      -v, --verbose                 Enable verbose output

Notes:

- First run creates `~/.vbx.json`. Subsequent runs use it unless `--config` is supplied.
- In daemon mode, editing `~/.vbx.json` will automatically reload.

## 🔊 Volume Mechanics

vbx lets you control your volume in three ways:
1. **Keyboard Volume** (`--volume`): Sets the base volume for your keyboard sounds (from 0 to 100).
2. **Mouse Volume** (`--mouse-volume`): Sets the base volume for your mouse clicks (from 0 to 100).
3. **System Volume Multiplexer** (`--system-volume-following=1`): When turned on, `vbx` automatically scales the app's sound to match your computer's main volume slider.

**How the multiplexer works:**
Instead of just cutting the volume linearly (which makes sounds too quiet at 50% OS volume), the multiplexer uses a gentle boost curve. This means if you lower your computer's volume, your typing sounds will still be punchy and easy to hear without ever being too loud!

**Example:**
If you set your keyboard volume to `50%` (`vbx --volume 50 --system-volume-following 1`), and your computer's master volume is currently at `80%`, `vbx` will intelligently scale its own output down to securely match your OS levels, without you having to change your `vbx` settings every time you watch a quiet video!

## 🎵 Sound Packs

System packs are installed under: `/usr/share/vbx/soundpacks/`

User packs go under: `~/.local/share/vbx/soundpacks/<device-name>/<pack-name>/`

Required files inside each `<pack-name>/` directory:

- `config.json` (format matches bundled packs)
- Audio files referenced by `config.json`

## 🤝 Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.

## 📄 License

MIT — see [LICENSE](LICENSE).

## 🙏 Credits

- [MechVibes](https://github.com/hainguyents13/mechvibes) for sound packs
- [mechavibes-dx](https://github.com/hainguyents13/mechvibes-dx) for sound packs
- [showmethekey](https://github.com/AlynxZhou/showmethekey) for code
