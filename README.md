# Evolution M365 Calendar Subscribe

Evolution M365 Calendar Subscribe is a GNOME Evolution plugin that adds a **Quick Subscribe** action to the calendar view for subscribing to colleagues' Exchange Web Services or Microsoft 365 calendars.

## Features

- Adds **Quick Subscribe...** to Evolution's calendar File menu and toolbar.
- Searches Exchange/Microsoft 365 address books with fuzzy autocomplete.
- Subscribes to another person's calendar when readable.
- Falls back to the target user's free/busy availability calendar when full calendar access is unavailable.

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

Enable the plugin in Evolution through **Edit > Plugins**.

## Test

```bash
cd build
ctest --output-on-failure
```

