# Install Evolution M365 Calendar Subscribe

## Build Dependencies

On Debian/Ubuntu systems:

```bash
sudo apt-get install cmake pkg-config evolution-dev evolution-common \
    libglib2.0-dev libgtk-3-dev evolution-ews
```

The plugin uses GNOME Evolution, Evolution Data Server, GTK 3, and the
installed Evolution EWS libraries. Configure an Exchange EWS, Exchange Online,
or Microsoft 365 account in Evolution before using **Quick Subscribe...**.

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
5. Type a colleague's name or email address.
6. Select the matching Microsoft 365 or Exchange contact and subscribe.

## Troubleshooting

- No contacts found: confirm that the Microsoft 365 or Exchange address book is
  enabled in Evolution.
- Subscription failed: confirm that the colleague has shared the calendar with
  your account.
- Availability only: full calendar access may be blocked, but free/busy data is
  available.
