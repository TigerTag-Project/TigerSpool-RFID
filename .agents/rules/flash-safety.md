# Flash layout and flashing

**Rule.** The partition table (`firmware/partitions.csv`) is frozen. Never write a merged factory image at `0x0` on a provisioned device. Before flashing over USB, confirm which board answers on the port.

**Why.** A device cannot install a new partition table over the air. NVS at `0x9000` holds the Wi-Fi credentials, the TigerTag session and the imported printers; an image from `0x0` wipes them with no undo. On 2026-10-19 the bench's usual USB port carried a different product.

**How.**
- `bash scripts/flash.sh` writes the bootloader, the table and the app (`0x10000`), checks the board's MAC against `.bench-mac` first, and leaves NVS alone. `--erase` wipes everything, on purpose only.
- `esptool.py read_mac` identifies a board without writing anything.
- A downgrade or test build over USB: write `boot_app0.bin` at `0xe000` and the app at `0x10000`, nothing else.
