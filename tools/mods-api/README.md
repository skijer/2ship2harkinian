# 2ship mod SDK

A mod is an `.o2r` dropped into the `mods/` folder next to `2ship.exe`. At startup the game loads
the library inside it, hands it a table of function pointers, and calls its entry points.

## Sample mods

| Mod | What it does | What it shows |
| --- | --- | --- |
| `hello_mod` | Says hello when you pick up a recovery heart | the smallest mod: one hook |
| `floor_is_lava` | Standing on the ground burns you, unless you wear the Circus Leader's Mask | reading the player through `gPlayState`, and a VB that cancels a pickup |
| `faster_on_damage` | You move at 90% speed, and every hit adds 1% | a VB that rewrites the value the game is about to use |
| `reverse_time` | The clock runs backwards | writing `gSaveContext` every frame |
| `enemy_rain` | Every minute a random small enemy shows up | driving the game through console commands |

## Contract

A mod exports two symbols (see [hello_mod/hello_mod.c](hello_mod/hello_mod.c)):

| Symbol | When it runs |
| --- | --- |
| `ModSetApi(const S2HModApi*)` | First. Stash the pointer; it stays valid for the process. |
| `ModInit(void)` | After the API is set. Bind your symbols and register your hooks here. |

Registrations last for the process. To remove a mod, delete its `.o2r` and restart the game.

The table lives in [ModApi.h](../mm/2s2h/ModApi/ModApi.h) and holds three entry points:

```c
bool  RegisterHookByName(const char* name, void* callback);
bool  RegisterVB(GIVanillaBehavior flag, S2HVbCallback callback);
void* GetSymbol(const char* name);
```

Check it before use, and bail if it does not match:

```c
if (!S2H_MOD_API_MATCHES(sApi)) {
    return;
}
```

That is the only compatibility check there is. It compares `sizeof(S2HModApi)`, so it catches a
game whose table changed shape — but nothing checks the game's version, and nothing notices when a
struct you read through `gPlayState` was laid out differently in the build you compiled against.
Rebuild your mod against each 2ship release.

## Hooks

Every GameInteractor hook is reachable by name. Register through `S2H_REGISTER_HOOK`, which checks
your callback's signature at compile time:

```c
static void OnItemGive(u8 item) { ... }

S2H_REGISTER_HOOK(sApi, OnItemGive, OnItemGive);
```

The names and signatures come from
[GameInteractor_HookTable.h](../mm/2s2h/GameInteractor/GameInteractor_HookTable.h), the same list
the game itself registers against, so a hook added to the game shows up here on the next build.
Looking hooks up by name is what lets that list grow without invalidating mods built against an
older one.

Callbacks take the hook's own arguments and nothing else. Keep your state in globals: a mod compiled
as C has no capturing closures, and the callback carries no user pointer.

## Vanilla behaviours

The game asks itself 301 questions before acting, at 437 call sites, all through one hook. Subscribe
to a single flag with `RegisterVB`:

```c
// The call site in z_en_item00.c passes the collectible actor
static void ShouldGiveItemFromItem00(bool* should, va_list args) {
    Actor* item00 = va_arg(args, Actor*);

    if (item00->params == ITEM00_RECOVERY_HEART) {
        *should = false;
    }
}

sApi->RegisterVB(VB_GIVE_ITEM_FROM_ITEM00, ShouldGiveItemFromItem00);
```

`*should` starts at the game's own answer; write `false` to cancel the vanilla behaviour. Arguments
that arrive as pointers can be rewritten instead, which is how `faster_on_damage` scales speed.

**Nothing checks your `va_arg` calls.** How many arguments a flag carries, and of what type, is
decided by its call site in the game — grep the flag to find it. Read one too many, or read it as
the wrong type, and you corrupt memory. Test your mod.

## Everything else

`GetSymbol` returns the address of a game global or an exposed function, or `NULL`. You declare the
prototype; nothing verifies it. `S2H_BIND` binds a pointer named exactly like the symbol:

```c
static void (*S2H_Notify)(const char* message);
static PlayState** gPlayStatePtr;

if (S2H_BIND(sApi, S2H_Notify) == NULL) {
    return;
}
gPlayStatePtr = sApi->GetSymbol("gPlayState");
```

Note the extra `*` on globals: `GetSymbol` hands you the variable's **address**, so `gPlayState`
arrives as `PlayState**`. That is deliberate — the pointer inside changes between scenes.

Your own name cannot collide with the game's, since the headers already declare `gPlayState` and
friends. Call the local something else.

What is exposed:

| Group | |
| --- | --- |
| Helpers | `S2H_Log`, `S2H_Notify`, `S2H_CVarApply`, `S2H_RunCommand` |
| CVars | the LUS console-variable API: get / set / register / clear / copy / load / save |
| Flags | the `Flags_*` family: switch, treasure, collectible, clear, eventChkInf, infTable, weekEventReg, eventInf, randoInf |
| Globals | `gPlayState`, `gSaveContext`, `gRegEditor`, `gActorOverlayTable`, `gBitFlags`, `gCullBackDList`, `gEmptyDL`, `gItemIcons`, `gItemSlots`, `gSfxDefaultPos`, `gSfxDefaultReverb`, `gSfxDefaultFreqAndVolScale` |

Writing a CVar takes effect only after `S2H_CVarApply`, because enhancements register and
unregister their hooks from there. `CVarSave` is separate again, and without it the value is gone
on restart.

`S2H_RunCommand` runs a console command and returns the handler's result, `0` on success. That is
how a mod spawns actors, gives items or warps without any game function being exposed:

```c
S2H_RunCommand("spawn 12 0");
S2H_RunCommand("give_item 5");
S2H_RunCommand("entrance 0xD800");
```

The commands live in [DebugConsole.cpp](../mm/2s2h/DeveloperTools/DebugConsole.cpp).

Nothing else is exposed. There is no `Actor_Spawn`, no collider, no matrix and no effect function:
those are internal names that move as the decomp evolves, and a mod holding them would break
silently. Intervene through hooks and VBs, read and write state through the globals, and act
through the console.

## Building

Compile against the game's headers, since the hook signatures use the game's own types:

```sh
cd tools/mods-api
clang -shared -o out/enemy_rain.dll enemy_rain/enemy_rain.c \
  -I ../../mm/2s2h/ModApi -I ../../mm/include -I ../../mm/include/PR -I ../../mm/assets \
  -I ../../mm/src -I ../../mm -I ../../mm/2s2h -I ../../libultraship/include
python pack_mod.py enemy_rain --binary windows_x64=out/enemy_rain.dll
```

On Linux and macOS add `-fPIC` and name the output `.so` / `.dylib`.

That writes `out/mods/enemy_rain.o2r`; copy that `mods/` folder next to the executable. Pass
`--binary` once per platform you built for — `windows_x64`, `linux_x64`, `darwin` — and the archive
keeps only the ones you supplied.

The [mods-api workflow](../../.github/workflows/mods-api.yml) does all three platforms on every
push that touches this folder, and uploads the packed `.o2r` files as an artifact.

Because you compile against those headers, the binary is tied to the game's struct layouts. Nothing
enforces that at load time, so a mod carried over to a release that moved a field reads garbage
rather than being refused.

## Why a pointer table instead of calling the game directly

The executable exports no symbols and generates no import library, so there is nothing to link
against on any platform. The table the host hands you works the same everywhere, and it keeps mods
clear of the C++ ABI: the GameInteractor's registration tables are template statics, so a mod that
instantiated them would get its own private copies and register into tables the game never reads.
