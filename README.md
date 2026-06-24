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
- Subscribes to another person's calendar when calendar permissions allow it.
- Falls back to the target user's free/busy availability calendar when full
  calendar access is unavailable.
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
5. Subscribe to the shared calendar or availability calendar.

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
