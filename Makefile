CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Iinclude -I.
LDFLAGS ?=
LDLIBS ?= -lm -lpthread -ldl

CORE_SRCS := core/plan.c core/source.c core/export.c core/probe.c core/cache.c core/util.c core/source_yaml.c core/m3u.c
CORE_OBJS := $(CORE_SRCS:.c=.o)
LIBMSPCORE := build/libmspcore.a

BACKEND_SRCS := backend/player.c backend/miniaudio_impl.c
BACKEND_OBJS := $(BACKEND_SRCS:.c=.o)
LIBMSPBACK := build/libmspbackend.a

CLI_SRCS := cli/main.c
CLI_OBJS := $(CLI_SRCS:.c=.o)
CLI_BIN := build/msmp-cli

.PHONY: all core cli backend tui all-min clean help

all: cli

core: $(LIBMSPCORE)

$(LIBMSPCORE): $(CORE_OBJS) | build
	$(AR) rcs $@ $(CORE_OBJS)

backend: $(LIBMSPBACK)

$(LIBMSPBACK): $(BACKEND_OBJS) $(LIBMSPCORE) | build
	$(AR) rcs $@ $(BACKEND_OBJS)

cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_OBJS) $(LIBMSPCORE) $(LIBMSPBACK) | build
	$(CC) $(CLI_OBJS) $(LIBMSPCORE) $(LIBMSPBACK) -o $@ $(LDFLAGS) $(LDLIBS)

tui: | build
	@if pkg-config --exists ncurses 2>/dev/null; then \
		$(CC) -std=c99 -O2 -Iinclude -I. ui/ncurses/main.c $(LIBMSPCORE) $(LIBMSPBACK) -o build/msmp-tui `pkg-config --cflags --libs ncurses` $(LDLIBS); \
	else \
		echo "ncurses bulunamadı (opsiyonel)"; \
	fi

all-min: core cli
	@echo "core+cli hazır (GUI yok)"

build:
	mkdir -p build

clean:
	rm -f $(CORE_OBJS) $(BACKEND_OBJS) $(CLI_OBJS) $(LIBMSPCORE) $(LIBMSPBACK) $(CLI_BIN) build/msmp-tui

help:
	@echo "Hedefler:"
	@echo "  make core     - Sadece libmspcore (GUI bağımsız)"
	@echo "  make backend  - Oynatma backend (miniaudio)"
	@echo "  make cli      - CLI (GTK/libadwaita gerekmez)"
	@echo "  make tui      - ncurses TUI (opsiyonel)"
	@echo "  make all-min  - core + cli minimum"
	@echo "  make clean"
	@echo "Kural: make cli GTK linklemez"
