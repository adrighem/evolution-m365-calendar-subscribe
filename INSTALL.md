# Installation Guide

## Build Dependencies

On Debian/Ubuntu systems:

```bash
sudo apt-get install cmake pkg-config libevolution-dev evolution-common \
    libglib2.0-dev libgtk-3-dev evolution-ews
```

The plugin uses Evolution, Evolution Data Server, GTK 3, and the installed Evolution EWS libraries.

## Build and Install

```bash
mkdir build
cd build
cmake ..
make
sudo make install
```

Installed files:

- Plugin module: Evolution plugin directory, usually under `/usr/lib/evolution/plugins/`
- Plugin metadata: `org-gnome-evolution-m365-calendar-subscribe.eplug`

## Usage

1. Open Evolution.
2. Enable the plugin in **Edit > Plugins**.
3. Open the calendar view.
4. Use **Quick Subscribe...** from the File menu or toolbar.
5. Type a colleague's name or email address and subscribe.

