# Evolution M365 Calendar Subscribe

Evolution M365 Calendar Subscribe is a GNOME Evolution plugin for Microsoft 365,
Exchange Online, and Evolution EWS calendar users on Linux. It adds a
**Quick Subscribe...** action to the Evolution calendar view so you can find a
colleague in your company address book and subscribe to their shared calendar.

The plugin is useful for teams that use Evolution as a Microsoft 365 or Exchange
desktop client and need a faster way to add coworkers' calendars without digging
through account settings or server-side folder names.

## Features

- Adds **Quick Subscribe...** to Evolution's calendar File menu and toolbar.
- Searches Exchange, Exchange Online, and Microsoft 365 address books with fuzzy
  autocomplete.
- Fast availability calendar subscriptions (Free/Busy mode by default).
- Optional full calendar subscription with automated fallback to availability.
- Works with Evolution EWS-backed accounts and Microsoft 365 address books.

## Requirements

- GNOME Evolution with calendar support.
- Evolution EWS or Microsoft 365 account configured in Evolution.
- Evolution development headers for building from source.
- GTK 3, GLib, Evolution Data Server, and CMake.

See [INSTALL.md](INSTALL.md) for Debian/Ubuntu package names and detailed setup.

## Build

```bash
mkdir build
cd build
cmake ..
make
```

### Build Options

#### Free/Busy Only Mode (`FREEBUSY_ONLY`)

By default, the plugin is built with `-DFREEBUSY_ONLY=ON`.

* **Default (`-DFREEBUSY_ONLY=ON`)**: Directly creates a Free/Busy availability
  calendar (`freebusy-calendar::<email>`). This bypasses Exchange folder queries
  and permission checks, providing instant subscriptions without network lag.
* **Full Calendar Mode (`-DFREEBUSY_ONLY=OFF`)**: Attempts to query and subscribe
  to the user's primary "Calendar" folder first, falling back to Free/Busy
  availability only if permission is denied.

> **Performance Note**: Adding standard/full calendars can be very slow. It
> requires multiple synchronous EWS network round-trips to Exchange/M365 to
> switch mailboxes, query folder metadata, and inspect permission tables. In
> addition, Exchange will reject standard calendar access unless the coworker
> has explicitly configured delegate or read permissions for your account.
> For most users, the default Free/Busy mode (`-DFREEBUSY_ONLY=ON`) is recommended.

To disable Free/Busy-only mode and attempt full calendar subscriptions:

```bash
mkdir build
cd build
cmake -DFREEBUSY_ONLY=OFF ..
make
```

To explicitly enable Free/Busy-only mode:

```bash
cmake -DFREEBUSY_ONLY=ON ..
make
```

## Install

```bash
sudo make install
```

Enable the plugin in Evolution through **Edit > Plugins**, then open the calendar
view and choose **Quick Subscribe...** from the File menu or toolbar.

## Usage

1. Open Evolution and switch to the calendar view.
2. Choose **Quick Subscribe...**.
3. Type a coworker's name or email address.
4. Select the matching contact from autocomplete.
5. Subscribe to the calendar.

## Troubleshooting

- If no contacts appear, check that an Exchange EWS or Microsoft 365 address book
  is enabled in Evolution.
- If subscription fails, the target calendar may not be shared with your account.
- If only availability is added, the target user likely exposes free/busy data
  but not full calendar contents.

## Test

```bash
cd build
ctest --output-on-failure
```

## License

This project is licensed under the GNU General Public License version 3. See
[LICENSE](LICENSE).

## Related Search Terms

Evolution Microsoft 365 calendar plugin, Evolution EWS shared calendar,
Exchange Online calendar Linux, subscribe to coworker calendar in Evolution,
GNOME Evolution Microsoft 365 address book.
