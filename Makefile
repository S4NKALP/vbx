CC = gcc
# Updated include paths for modular headers
CFLAGS = -Wall -Wextra -std=c99 -Iinclude -Iinclude/app -Iinclude/audio -Iinclude/common -Iinclude/sound -Iinclude/config
PREFIX ?= /usr

# Pass PACKAGE_PREFIX macro for config.h
CPPFLAGS = -DPACKAGE_PREFIX=\"$(PREFIX)\" $(shell pkg-config --cflags libevdev json-c libpulse-simple sndfile)

LDFLAGS_SOUND = -ljson-c -lpulse -lpulse-simple -lsndfile -lpthread
LDFLAGS_KEYBOARD = $(shell pkg-config --libs libevdev libinput libudev) -lpthread

# Targets
VBX_TARGET = vbx
SOUND_TARGET = audio
KEYBOARD_TARGET = input

# Sources (reorganized)
VBX_SOURCE = src/main.c src/common/utils.c src/config.c src/soundpacks.c src/app/process.c src/app/watch.c src/cli.c src/app/reload.c
SOUND_SOURCE = src/audio/main.c src/audio/config.c src/audio/playback.c src/common/utils.c
KEYBOARD_SOURCE = src/input.c src/common/utils.c

# Install paths
BINDIR = $(PREFIX)/bin
SHAREDIR = $(PREFIX)/share/vbx
UDEV_RULE = /etc/udev/rules.d/99-vbx-allow-keyboard.rules

all: $(VBX_TARGET) $(SOUND_TARGET) $(KEYBOARD_TARGET)


# Build main launcher (needs json-c)
$(VBX_TARGET): $(VBX_SOURCE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ -ljson-c -lpthread

$(SOUND_TARGET): $(SOUND_SOURCE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS_SOUND)

$(KEYBOARD_TARGET): $(KEYBOARD_SOURCE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS_KEYBOARD)

clean:
	rm -f $(VBX_TARGET) $(SOUND_TARGET) $(KEYBOARD_TARGET)

test: all
	@echo "Testing sound packs:"
	./$(VBX_TARGET) --list
	@echo ""
	@echo "To run VBX:"
	@echo "  sudo ./$(VBX_TARGET)                    # Default sound"
	@echo "  sudo ./$(VBX_TARGET) -s cherrymx-blue-abs  # Specific sound"
	@echo "  sudo ./$(VBX_TARGET) --help             # Show help"

install:
	@echo "Installing VBX to $(DESTDIR)$(BINDIR) and $(DESTDIR)$(SHAREDIR)..."
	install -Dm755 $(VBX_TARGET) $(DESTDIR)$(BINDIR)/$(VBX_TARGET)
	install -Dm755 $(SOUND_TARGET) $(DESTDIR)$(BINDIR)/vbx-audio
	install -Dm755 $(KEYBOARD_TARGET) $(DESTDIR)$(BINDIR)/vbx-input
	install -d $(DESTDIR)$(SHAREDIR)
	cp -r soundpacks $(DESTDIR)$(SHAREDIR)/
	@echo "Installing udev rule for non-root keyboard access..."
	@echo '# Allow non-root access to input event devices for active seat users and input group' | sudo tee $(UDEV_RULE) >/dev/null
	@echo 'SUBSYSTEM=="input", KERNEL=="event*", TAG+="uaccess", GROUP="input", MODE="0660"' | sudo tee -a $(UDEV_RULE) >/dev/null
	@echo "Reloading udev rules..."
	@sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=input --action=change || true
	@echo "Applying immediate ACLs to current user for existing input devices..."
	@CURRENT_USER=$${SUDO_USER:-$$USER}; \
	for dev in /dev/input/event*; do \
		[ -e "$$dev" ] && sudo setfacl -m u:$${CURRENT_USER}:rw "$$dev" || true; \
	done
	@echo "Note: ACLs applied for the current session. Future devices are handled by udev via TAG+=uaccess."
	@echo "Installation complete."

uninstall:
	@echo "Uninstalling VBX from $(DESTDIR)$(BINDIR) and $(DESTDIR)$(SHAREDIR)..."
	rm -f $(DESTDIR)$(BINDIR)/$(VBX_TARGET)
	rm -f $(DESTDIR)$(BINDIR)/vbx-audio
	rm -f $(DESTDIR)$(BINDIR)/vbx-input
	rm -rf $(DESTDIR)$(SHAREDIR)
	@echo "Removing udev rule..."
	@sudo rm -f $(UDEV_RULE) || true
	@sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=input --action=change || true
	@echo "Uninstallation complete."

.PHONY: all clean test install uninstall
