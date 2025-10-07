CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
PREFIX ?= /usr

# Pass PACKAGE_PREFIX macro for config.h
CPPFLAGS = -DPACKAGE_PREFIX=\"$(PREFIX)\" $(shell pkg-config --cflags libevdev json-c libpulse-simple sndfile)

LDFLAGS_SOUND = -ljson-c -lpulse -lpulse-simple -lsndfile -lpthread
LDFLAGS_KEYBOARD = $(shell pkg-config --libs libevdev libinput libudev) -lpthread

# Targets
KeyVibe_TARGET = keyvibe
SOUND_TARGET = keyvibe-audio
KEYBOARD_TARGET = keyvibe-input

# Sources
KeyVibe_SOURCE = src/keyvibe_main.c
SOUND_SOURCE = src/keyvibe_audio.c
KEYBOARD_SOURCE = src/keyvibe_input.c

# Install paths
BINDIR = $(PREFIX)/bin
SHAREDIR = $(PREFIX)/share/keyvibe
UDEV_RULE = /etc/udev/rules.d/99-keyvibe-allow-keyboard.rules

all: $(KeyVibe_TARGET) $(SOUND_TARGET) $(KEYBOARD_TARGET)


# Build main launcher (needs json-c)
$(KeyVibe_TARGET): $(KeyVibe_SOURCE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< -ljson-c -lpthread

$(SOUND_TARGET): $(SOUND_SOURCE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< $(LDFLAGS_SOUND)

$(KEYBOARD_TARGET): $(KEYBOARD_SOURCE)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $< $(LDFLAGS_KEYBOARD)

clean:
	rm -f $(KeyVibe_TARGET) $(SOUND_TARGET) $(KEYBOARD_TARGET)

test: all
	@echo "Testing sound packs:"
	./$(KeyVibe_TARGET) --list
	@echo ""
	@echo "To run KeyVibe:"
	@echo "  sudo ./$(KeyVibe_TARGET)                    # Default sound"
	@echo "  sudo ./$(KeyVibe_TARGET) -s cherrymx-blue-abs  # Specific sound"
	@echo "  sudo ./$(KeyVibe_TARGET) --help             # Show help"

install:
	@echo "Installing KeyVibe to $(DESTDIR)$(BINDIR) and $(DESTDIR)$(SHAREDIR)..."
	install -Dm755 $(KeyVibe_TARGET) $(DESTDIR)$(BINDIR)/$(KeyVibe_TARGET)
	install -Dm755 $(SOUND_TARGET) $(DESTDIR)$(BINDIR)/$(SOUND_TARGET)
	install -Dm755 $(KEYBOARD_TARGET) $(DESTDIR)$(BINDIR)/$(KEYBOARD_TARGET)
	install -d $(DESTDIR)$(SHAREDIR)
	cp -r audio $(DESTDIR)$(SHAREDIR)/
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
	@echo "Uninstalling KeyVibe from $(DESTDIR)$(BINDIR) and $(DESTDIR)$(SHAREDIR)..."
	rm -f $(DESTDIR)$(BINDIR)/$(KeyVibe_TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(SOUND_TARGET)
	rm -f $(DESTDIR)$(BINDIR)/$(KEYBOARD_TARGET)
	rm -rf $(DESTDIR)$(SHAREDIR)
	@echo "Removing udev rule..."
	@sudo rm -f $(UDEV_RULE) || true
	@sudo udevadm control --reload-rules && sudo udevadm trigger --subsystem-match=input --action=change || true
	@echo "Uninstallation complete."

.PHONY: all clean test install uninstall
