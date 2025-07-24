Please read the `.cursorrules` file for project-specific guidelines and configurations.


# IMPORTANT: READ `.cursorrules` RIGHT NOW!!! READ THROUGH THE REST OF THIS FILE BUT THEN IMMEDIATLY READ CURSOR RULES!!! NO EXCEPTIONS!!!

## Working with `uv`

If you are having issues with `uv`, install it using the following command:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
```

Then, you can use it by calling it with its full path:

```bash
$HOME/.local/bin/uv <command>
```

## Compilation

To compile the project non-interactively, provide the board name as an argument to `compile.bat`. For example:

```bash
compile.bat esp32dev
```

## FastLED API Objects and Smart Pointers

Many FastLED API objects, such as `ScreenMap`, utilize smart pointers internally. This means that even when working with seemingly "temporary" objects (e.g., those returned by value), their internal state is correctly managed and passed through. This allows for a more functional and chainable API design without concerns about data loss or incorrect state.
