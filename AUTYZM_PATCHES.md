# Autyzm Launcher patches

Autyzm Launcher is a GPL-3.0 fork of Fjord Launcher, itself based on Prism Launcher/PolyMC/MultiMC.

Initial downstream goals:

- keep upstream launcher code maintainable and rebased from Fjord Launcher;
- remove the Microsoft-account gate for offline accounts;
- let first-time users click Play, enter a nickname, and launch in offline mode;
- rebrand build metadata to Autyzm Launcher / autyzm.pl.

## Current patch set

1. Offline/authlib account creation no longer requires an existing Microsoft account.
2. Instance download/update is allowed without a Microsoft account.
3. First-run setup wizard no longer forces Microsoft login.
4. Launching with zero accounts opens an offline nickname dialog, creates an offline account, sets it as default, and launches offline instead of demo mode.
5. Basic build metadata points at `Autyzm-pl/autyzm_launcher`.

## Planned next steps

- Replace copied Fjord icons with final Autyzm.pl artwork.
- Add default instance/modpack bootstrap.
- Add CI release artifacts for Windows and Linux.
- Optional phase 2: authlib-injector + Discord/Yggdrasil-compatible auth backend.
