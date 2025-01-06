# Default recipe
default:
    @just --list

# Build the project
build:
    pio run

# Clean the project
clean:
    pio run -t clean

# Upload the firmware
upload:
    pio run -t upload

# Monitor the serial output
monitor:
    pio device monitor

# Build and upload the firmware
flash: build upload

# Create a compilation database for tooling
compiledb:
    pio run -t compiledb

# Clean, compile, and upload
clean-flash: clean build upload

# Search for libraries
pkg-search query:
    pio pkg search "{{query}}"

# Install a specific library
pkg-install library:
    pio pkg install -l "{{library}}"

# Uninstall a specific library
pkg-uninstall library:
    pio pkg uninstall -l "{{library}}"

# List installed packages
pkg-list:
    pio pkg list

# Update installed packages
pkg-update:
    pio pkg update

trace-pc-address pc_address:
    addr2line -pfiaC -e ./.pio/build/lilygo-t-display/firmware.elf "{{pc_address}}"