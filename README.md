<div align="center">
  <img src="assets/sample-logo-470x235.png" alt="Project Logo">

# KeyVibe

> A C-based CLI tool that brings realistic mechanical keyboard sounds to every keystroke.

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

## 🛠️ Build

```bash
make -j$(nproc)
sudo make install
```

## 💡 Usage

```bash
./keyvibe --list # List available sound packs (system + user)

./keyvibe # Run with defaults

./keyvibe -c -s `sound_pack` -V 60 # Select pack and volume, override persisted config for this run

./keyvibe --daemon # Background daemon mode

./keyvibe --stop  # Stop daemon

```

> config file will be create at `$HOME` as `.keyvibe.json`

```

Flags:

- `-s, --sound <name>`: sound pack name
- `-V, --volume <0-100>`: volume percent
- `-c, --override-config`: use CLI values for this run
- `-l, --list`: list sound packs
- `--daemon | --stop | --reload`
- `-v, --verbose`
```

## 🎵 Sound Packs

System packs are installed under: `/usr/share/keyvibe/audio/`

User packs go under: `~/.local/share/keyvibe/audio/<pack-name>/`

Required files inside each `<pack-name>/` directory:

- `config.json` (format matches bundled packs)
- Audio files referenced by `config.json`

Resolution order when selecting `-s <name>`:

1. `~/.local/share/keyvibe/audio/<name>/`
2. `/usr/share/keyvibe/audio/<name>/`

List shows `(user)` or `(system)` source next to each pack.

### config.json structure (custom packs)

KeyVibe supports two modes via `key_define_type`: `single` and `multi`.

- `single`: one long audio file; per-key plays a segment `[start_ms, duration_ms]`.
- `multi`: separate audio files; per-key maps to its own press/release files; also supports a set of generic press sounds and a generic release.

Minimal schemas and examples:

```json
// Single mode (one audio file, sliced by key)
{
  "key_define_type": "single",
  "sound": "purple.ogg", // relative to the pack dir
  "defines": {
    "30": [120, 80], // key_code 30: start_ms=120, duration_ms=80
    "48": [300, 100]
  }
}
```

```json
// Multi mode (separate files, optional generic patterns)
{
  "key_define_type": "multi",
  // Generic press sound(s): can be one file, a printf-like pattern, or a {0-4} pattern
  // Examples:
  //   "GENERIC.mp3"
  //   "GENERIC_R%d.mp3"     // expands R0..R4 if files exist
  //   "GENERIC_R{0-4}.mp3"  // also expands to R0..R4
  "sound": "GENERIC_R{0-4}.mp3",

  // Optional generic release sound
  "soundup": "release/GENERIC.mp3",

  // Per-key overrides (key codes from libinput/libevdev)
  // For release-specific files, append "-up" to the key code string
  "defines": {
    "57": "press/SPACE.mp3", // SPACE press
    "57-up": "release/SPACE.mp3", // SPACE release
    "28": "press/ENTER.mp3",
    "14": "press/BACKSPACE.mp3"
  }
}
```

Notes:

- All paths in `config.json` are relative to the sound pack directory.
- In multi mode, if a key press isn’t mapped, a random generic press file is chosen if provided.
- In multi mode, if a key release isn’t mapped, `soundup` is used if provided.
- In single mode, only presses are played; releases are ignored.

## Available Sound Packs:

- nk-cream
- cherrymx-black-abs
- cherrymx-red-pbt
- cherrymx-blue-abs
- eg-oreo
- cherrymx-brown-pbt
- eg-crystal-purple
- cherrymx-brown-abs
- topre-purple-hybrid-pbt
- cream-travel
- holy-pandas
- mxbrown-travel
- cherrymx-red-abs
- mxblack-travel
- cherrymx-black-pbt
- turquoise
- cherrymx-blue-pbt
- mxblue-travel
- banana-split-lubed

## 🤝 Contributing

PRs welcome. Please keep code modular and readable, and prefer small, focused changes.

## 📄 License

MIT — see [LICENSE](LICENSE).
