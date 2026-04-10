HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = CrankSurvivor.pdx

SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

SRC = src/main.c src/game.c src/images.c src/rendering.c src/entities.c \
      src/collision.c src/player.c src/enemy.c src/weapons.c src/bullets.c \
      src/ui.c src/sound.c src/save.c src/keeper.c src/boss.c src/relic.c

UINCDIR = src

UDEFS = -DDEBUG_BUILD=1

include $(SDK)/C_API/buildsupport/common.mk

run: all
	open -a "$(SDK)/bin/Playdate Simulator.app" $(PRODUCT)
