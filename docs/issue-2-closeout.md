# Issue 2 closeout — 2026-11-16

The previous tray advertised a system icon directory for a per-user installation,
which could display a checkerboard placeholder. The service now exports embedded
phone icon pixels using the StatusNotifierItem a(iiay) property, in network-order
ARGB. State changes emit NewIcon; the category is SystemServices. User-installed
desktop entries resolve their icon directly to the installed SVG.

The automated tray test covers valid nonempty pixel data, transparent backgrounds,
ARGB byte order, three resolutions and offline/connected/mirroring state changes.
The full Qt suite passes 15 tests on the target Omarchy PC. The installed desktop
entry passes desktop-file-validate and the Git diff passes whitespace validation.

Live desktop checks confirmed the phone icon renders in the tray, activation opens
the panel, closing the panel leaves the service active, and the tray survives a
service restart with the same stable item ID. The phone remains authorized wirelessly.
On this PC the tray widget was moved beside the network indicator and Phonelink was
pinned outside its drawer. That bar preference is user configuration, not a global
layout change performed by the installer. The normal window can still have a dock
entry while open. The separate guided setup workflow remains issue 1.
