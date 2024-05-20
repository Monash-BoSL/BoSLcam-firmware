RED=\x1b[31;01m
GREEN=\x1b[32;01m
BOLD=\x1b[1m
NORMAL=\x1b[0m
HIGHLIGHT=\e[30;48;5;41m

# BOARD			= native_camerans
BOARD			= nrf9160dk_nrf9160_ns
# BOARD			= BoSLcam_ns


ifndef ZEPHYR_BASE
  $(info [ERROR] ZEPHYR_BASE is not set. Did you run env.sh? )
  $(error  )
endif

.SILENT:

all:
	cd ${CURDIR}/../.. && ./setup.sh
	echo -e "$(GREEN)Building BoSLCam-firmware for $(BOARD)$(NORMAL)"
	west.exe build -b $(BOARD)

k:
	west build -t guiconfig

#prog:
#	nrfjprog --program build/zephyr/merged.hex --chiperase --verify --pinreset

clean:
	rm -rf build
