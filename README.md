# Editor

This is a proof of concept plugin editor for Endless Sky. It uses [Dear ImGui](https://github.com/ocornut/imgui) to render the UI on top of the game and as such it is technically a fork.

- Create, load and save plugins.
- Modify game objects and see the result immediately<sup>1</sup>
- Respects your plugin's file structure and definition order inside files.
- Doesn't remove any unrecognized nodes from your plugin.
- You can choose between 5 themes.
- It doesn't support everything yet (more complicated nodes like `mission`, `conversation`, `event`, ... are missing).

You can build this editor like the game (same build instructions) or use the CD generated artifacts for your OS.

---------

<sub><sup>1</sup>: More or less. Example: If you are flying in a system and you modify a stellar object's sprite, you'll see it change immediately. On the other hand, if you modify the system's haze, it will update when you reenter the system.</sub>
