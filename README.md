A notepad *game*. This game injects into notepad.exe (the legacy one, win10 days) and will render a game inside of notepad which you can use WASD in.

## How it works

- Injection into Notepad to access Notepads memory with ease.
- Setting a keyboard hook
- Modifying the window procedure so that I can modify notepad renderring (still authentic, but the old notepad renderer flickered)
- Render frames using an immediate mode style API which writes to the notepads text buffer
