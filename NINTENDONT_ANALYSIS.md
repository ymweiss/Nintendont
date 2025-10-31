# Nintendont Project - Comprehensive Architecture Analysis

**Purpose**: Technical analysis for integrating Riivolution-style patching into Nintendont

**Date**: 2025-10-31

---

## OVERALL ARCHITECTURE

Nintendont is a two-stage boot loader system with a two-component kernel that runs GameCube games on Wii/Wii U:

**Key Components:**
1. **Loader (PPC)** - Wii/Wii U application that loads and patches the kernel
2. **Kernel (ARM)** - IOS emulation layer running inside IOS
3. **Multidol Loader** - Second-stage GameCube bootloader
4. **Codehandler** - Debug/cheat code execution engine

---

## 1. LOADER COMPONENT (`/loader/source/main.c`)

The loader is the initial Wii/Wii U application that:

**Initialization Flow:**
```
1. Load and patch IOS 58 kernel (non-Wii VC only)
2. Decompress and inject Nintendont kernel to 0x92F00000
3. Load kernelboot stub to 0x92FFFE00
4. Initialize storage devices (USB/SD)
5. Display game selection menu
6. Load game configuration and metadata
```

**Key Memory Locations:**
- Loader stub backup: `0x80001800` (1.5 KB)
- Kernel location: `0x92F00000` (1 MB)
- Kernel boot stub: `0x92FFFE00`
- Game config shared memory: `0xD3003000-0xD3004000`
- Version string marker: Used for launcher detection

**Configuration Loading:**
- Reads `/nincfg.bin` from primary device
- Contains game path, video mode, controller config, memory card settings
- Supports autoboot via `NIN_CFG_AUTO_BOOT` flag

**Game Launching Sequence:**
```
1. CheckForMultiGameAndRegion() - validates game and region
2. Mount storage devices
3. Load/setup memory card (emulated or real)
4. Load GameCube IPL (iplusa.bin, ipljap.bin, iplpal.bin) if needed
5. Write config to 0x80000020-0x80000FFF
6. Set control registers for video mode, power button, DRC detection
7. Clear ARAM (Audio RAM) at 0x90000000-0x90FFFFFF
8. Hand off to kernel via status flag 0xD3003420 = 0x0DEA
9. Jump to 0x81300000 (multidol loader entry)
```

---

## 2. KERNEL COMPONENT (`/kernel/main.c`)

The ARM kernel emulates IOS and controls all disc/game I/O:

**Kernel Boot Sequence (`kernel/start.s` + `main.c`):**
```
1. Early HID initialization (lines 109)
2. Clear BSS section manually (IOS doesn't touch it)
3. Signal loader ready (BootStatus = 1)
4. Wait for game selection (status == 0x0DEA)
5. Initialize storage (USB/SDHC)
6. Mount FAT filesystem
7. Initialize DI (Disc Interface)
8. Create DI reader thread
9. Initialize SI (Serial Interface - controllers)
10. Initialize EXI (External Interface - memory card/network)
11. Initialize patches system
12. Signal kernel ready (BootStatus = 0xdeadbeef)
13. Enter main event loop
```

**Main Event Loop (`main.c` lines 330-500):**
- Polls SI (controllers) at ~240 Hz
- Handles DI (disc/USB read) operations
- Updates memory card changes every 3 seconds
- Processes game reset/disc change requests
- Maintains disc caching
- Monitors game status flags

---

## 3. GAME LOADING AND EXECUTION FLOW

**Complete Boot Chain:**

```
Loader (0x80000000)
  ↓
Kernel Boot (0x92FFFE00 - ARM IOS)
  ↓
Kernel Main Loop (0x12000000)
  ↓
Multidol Loader (0x81300000 - PPC)
  ↓
AppLoader Code (0x01300000)
  ↓
Game DOL/ELF
  ↓
Game Entrypoint
```

**Multi-stage Patching:**

1. **IOS Kernel Patching (Loader)** (`main.c` lines 619-654):
   - Patches FS access pattern in IOS memory (0x93A00000-0x94000000)
   - Disables MEMPROT (0xD8B420A) to allow patches
   - Patches ES module boot code

2. **DOL/ELF Interception (Kernel Patch.c)**:
   - Hooks DOL entrypoint to `PATCH_OFFSET_ENTRY + 0x80000000`
   - Entry point location: `0x3000 - FakeEntryLoad_size`
   - Allows codehandler injection before game code runs

3. **Game-Specific Patches** (Kernel Patch.c lines 1174+):
   - Detects game via TITLE_ID/GAME_ID
   - Applies DSP patches (audio module fixes)
   - Applies widescreen patches
   - Applies timer patches
   - Applies DMA patches for problematic games
   - Patches PI_FIFO_WP register protection
   - Patches patch buffer relocation (PSO hack)

---

## 4. DISC EMULATION AND FILE I/O SYSTEM

**Disc Interface Architecture:**

**Three Disc Modes:**
1. **Real Disc** (`RealDI.c`) - Physical GameCube disc via DIP
2. **ISO/GCM Files** (`ISO.c`) - Standard disc images
3. **Extracted FST** (`FST.c`) - Decompressed filesystem tree
4. **CISO Images** - Compressed ISO with block mapping

**ISO Reading (`kernel/ISO.c` lines 84-200):**
- Supports standard ISO/GCM files
- Supports CISO (Compressed ISO) format with 2 MB blocks
- Block map caching for CISO
- Cache at 0x11000000-0x11E80000 (31 MB)
- Up to 1024 blocks can be mapped

**CISO Format Details:**
```
- Magic: "CISO" (0x4349534F)
- Block size: 2 MB (LE32)
- Header: 0x8000 bytes
- Block map: 1024 entries (16-bit indices)
- 0xFFFF = empty block (zeroed)
- Maps logical blocks to physical blocks in file
```

**Extracted FST Mode (`kernel/FST.c`):**
- Reads `root/sys/boot.bin`, `root/sys/bi2.bin`, `root/sys/fst.bin`
- Parses filesystem tree from FST entry table
- File caching with LRU (32 file cache entries)
- Maps disc offsets to filesystem paths dynamically
- Loads files via FatFS from primary storage device

**File Cache System (`FST.c` lines 156-284):**
```c
typedef struct {
    u32 Offset;      // Disc offset start
    u32 Size;        // File size
    FIL File;        // FatFS file handle
} FileCache;

FileCache FC[FILECACHE_MAX];  // 32 entries
```

**Multi-Game Disc Support (`main.c` lines 225-550):**
- Detects multi-game discs via header check
- Supports up to 15 games per disc
- Games aligned to 4-byte boundaries
- 34-bit shifted offsets (Wii-style) or 32-bit unshifted
- Stores ISOShift for multi-game offset mapping
- Stores BI2 region codes per game

---

## 5. MEMORY AND CODE LOADING

**Game Code Loading Path:**
```
Game disc/ISO (any format)
  ↓ [DI reads boot.bin header]
  ↓ [AppLoader loads to 0x01300000]
  ↓ [AppLoader relocates DOL/ELF sections]
  ↓ [DOL sections loaded to 0x80xxxxxx]
  ↓ [Game entry patched, kernel patches applied]
  ↓ [Entry jump to 0x81300000 (multidol loader)]
  ↓ [Multidol calls AppLoader callbacks]
  ↓ [AppLoader chains to original game entry]
```

**Memory Regions:**
- **0x00000000-0x00004000** - Exception vectors + TLB
- **0x80000000-0x81800000** - PPC RAM (24 MB usable for game)
- **0x90000000-0x91000000** - ARAM (16 MB audio RAM)
- **0xCC000000-0xCC008000** - Hardware registers (Flipper)
- **0x11000000-0x11E80000** - Kernel ISO cache
- **0x12E80000-0x12F00000** - DI read buffer (512 KB)
- **0x12A80000-0x12B80000** - SegaBoot IPL cache
- **0x12B80000-0x12E80000** - DIMM memory (emulated)

**DOL Format (`kernel/dol.h`):**
```c
typedef struct {
    u32 offsetText[7];   // Text section offsets in file
    u32 offsetData[11];  // Data section offsets
    u32 addressText[7];  // Target addresses (0x80xxxxxx)
    u32 addressData[11]; // Target addresses
    u32 sizeText[7];     // Section sizes
    u32 sizeData[11];
    u32 addressBSS;      // BSS start
    u32 sizeBSS;         // BSS size
    u32 entrypoint;      // Initial execution point
} dolhdr;
```

---

## 6. PATCHING MECHANISM

**Patch System Architecture (`kernel/Patch.c`):**

**Patch States:**
```c
#define PATCH_STATE_NONE  0      // Not loaded
#define PATCH_STATE_LOAD  1      // Loading code
#define PATCH_STATE_PATCH 2      // Applying patches
#define PATCH_STATE_DONE  4      // Patches done
```

**Game Detection & Patching:**
1. Game ID extracted from disc header (first 6 bytes)
2. Game-specific patch detection via:
   - TITLE_ID (3-byte identifier)
   - DOL size + memory signature patterns
   - SHA1 hashes of DSP modules

3. Applied patches:
   - **DSP Patches** - Audio resampling fixes (17+ known DSP hashes)
   - **Widescreen Patches** - 16:9 aspect ratio fixes
   - **Timer Patches** - Relative timer compensation
   - **DMA Patches** - ARStartDMA hooks for games with timing issues
   - **PI FIFO Patches** - Protect FIFO write pointer register
   - **Game-Specific Hacks** - PSO switcher, Sonic Riders, etc.

**Codehandler Injection (`codehandler/codehandler.s`):**
- Intercepts before game entrypoint execution
- Provides cheat code execution interface
- Communicates via EXI (serial port) at 0xCC00643C
- Supports:
  - Memory read/write (8/16/32-bit)
  - Code uploads
  - Breakpoints
  - Game pause/resume
  - Cheat code execution (60 at 0x01200000)

---

## 7. EXISTING PATCHING/CODE MODIFICATION MECHANISMS

**Hook Points for Patches:**

1. **Apploader Hook** (`Patch.c` line 1174):
   - `DoPatches()` called for every disc read, can intercept/modify game files before execution

2. **Entry Point Hook** (`Patch.c` lines 1333-1336):
   - DOL entry patched to `PATCH_OFFSET_ENTRY + 0x80000000`
   - Entry point saved to `GameEntry`
   - Kernel code executes first, then chains to game

3. **Patch Buffer** (`Patch.c` line 48-50):
   - Location: `0x3000 - FakeEntryLoad_size`
   - Stores original entry point
   - Patch code executes from here first

4. **Game-Specific Patches** (`Patch.c` lines 700-862):
   - Pattern matching for specific game functions
   - Relocatable patch insertion
   - Examples: PI_FIFO_WP protection, memory relocations

**Patch Application Methods:**
- Binary patching (direct code modification)
- Function hooking (branch redirection)
- Memory relocation (remapping code buffers)
- DSP code patches (audio module fixes)
- Register protection (prevent certain writes)

---

## 8. KEY SOURCE FILES FOR INTEGRATION

**Essential Files for Riivolution-style Patching:**

| File | Purpose | Key Functions |
|------|---------|---------------|
| `/kernel/Patch.c` | Game loading & patching | `DoPatches()`, `PatchGame()`, game detection |
| `/kernel/DI.c` | Disc/file I/O | `DiscReadSync()`, `DIChangeDisc()` |
| `/kernel/ISO.c` | ISO/CISO handling | `ISOReadDirect()`, CISO block mapping |
| `/kernel/FST.c` | Extracted FST reading | `FSTRead()`, filesystem tree traversal |
| `/kernel/main.c` | Kernel main loop | Event processing, status polling |
| `/loader/source/main.c` | Loader bootstrap | Game launching sequence |
| `/multidol/main.c` | Stage 2 loader | AppLoader invocation |
| `/kernel/Config.h` | Configuration | Game config structure (NIN_CFG) |
| `/kernel/dol.h` | DOL format | DOL header structure |
| `/kernel/elf.h` | ELF format | ELF loading support |
| `/codehandler/codehandler.s` | Code injection | Cheat/patch code execution |

---

## 9. CONFIGURATION AND GAME METADATA

**NIN_CFG Structure** (passed from loader to kernel):
```c
typedef struct {
    u32 Magicbytes;      // 0x01070CF6
    u32 Version;         // NIN_CFG_VERSION
    char GamePath[256];  // Disc path or "/di"
    u32 GameID;          // 4-byte game ID
    u32 Config;          // Feature flags (NIN_CFG_*)
    u32 VideoMode;       // Video output settings
    u32 Language;        // Language override
    u32 MaxPads;         // Max controllers
    u32 NetworkProfile;  // Network settings
    u32 MemCardBlocks;   // Memory card size
    // ... additional fields
} NIN_CFG;
```

**Configuration Flags** (NIN_CFG_*):
- `NIN_CFG_AUTO_BOOT` - Skip menu, launch directly
- `NIN_CFG_MEMCARDEMU` - Emulate memory cards
- `NIN_CFG_MC_MULTI` - Separate cards per region
- `NIN_CFG_USB` - Use USB instead of SD
- `NIN_CFG_HID` - Enable HID controller support
- `NIN_CFG_NATIVE_SI` - Use only GameCube ports
- `NIN_CFG_SKIP_IPL` - Skip GameCube IPL
- `NIN_CFG_LED` - Show access LED
- `NIN_CFG_BBA_EMU` - Broadband adapter emulation
- `NIN_CFG_FORCE_PROG` - Force progressive scan
- `NIN_CFG_WIIU_WIDE` - Wii U widescreen

---

## 10. BOOT SEQUENCE TECHNICAL DETAILS

**Complete PPC → ARM → PPC → Game Flow:**

```
STAGE 1: PPC Loader (Wii/Wii U)
├─ Patch IOS 58 FS access code
├─ Reload IOS 58 via ES
├─ Decompress kernel.bin (zlib) → 0x92F00000
├─ Load kernelboot stub → 0x92FFFE00
├─ Mount storage, show menu
├─ Load config & game path
├─ Trigger IOS_Ioctl(0x1F) to start kernel
└─ Set status = 0x0DEA

STAGE 2: ARM Kernel (IOS)
├─ Initialize HID, storage, DI, SI, EXI
├─ Set status = 0xdeadbeef (ready)
├─ Enter event loop
├─ Wait for status = 0x0DEA
├─ Load game metadata from /nincfg.bin
├─ Open disc image or real disc
└─ Signal loader kernel loaded

STAGE 3: PPC Game Boot (TinyLoad/Multidol)
├─ Patch game entry point
├─ Insert codehandler
├─ Load IPL ROM if needed
├─ Set status = 0x0DEA (start game)
├─ Clear ARAM
└─ Jump to 0x81300000

STAGE 4: Game Execution
├─ AppLoader executes (reads boot.bin)
├─ AppLoader reads 0x2440 from boot.bin:
│   ├─ DOL offset
│   ├─ FST offset
│   └─ FST size
├─ AppLoader loads DOL sections via DI
├─ DOL entry point = PATCH_OFFSET_ENTRY
├─ Patch code chains to real GameEntry
└─ Game runs with full emulation
```

---

## 11. KEY INSIGHTS FOR RIIVOLUTION INTEGRATION

**Leverage Points:**

1. **Patch Hook Location**: `DoPatches()` in `Patch.c` - called for every disc read, can intercept/modify game files before execution

2. **Game Identification**: Game ID available at boot time, enables game-specific patch selection

3. **Flexible File Loading**: FST mode allows per-file patching; ISO mode allows block-level patching

4. **Multi-Disc Support**: Already implemented, can be extended for patch disc selection

5. **Memory Layout**: Predictable memory addresses (0x80000000+), safe patching zones (0x81000000-0x82000000)

6. **Configuration System**: NIN_CFG can be extended to include patch options

7. **Disc Emulation**: Complete control over what data reaches the game - ideal for content replacement

**Recommended Implementation Approach:**

1. Extend `NIN_CFG` with patch configuration paths
2. Add `.riivolution` or `.patches` directory scanning in loader
3. Implement patch selection menu in loader
4. Hook `ISOReadDirect()`/`FSTRead()` to apply patches at disc-read time
5. Leverage existing `DoPatches()` for code modifications
6. Use existing multi-game disc infrastructure for patch versioning per region/revision

---

## SUMMARY

Nintendont's architecture provides a clean, modular system for game loading with clear interception points:

- **Loader** controls boot flow and configuration
- **Kernel** emulates IOS and manages all I/O
- **Patch system** intercepts code/data before game execution
- **Multi-stage loaders** ensure compatibility with diverse GameCube executables
- **File abstraction** (ISO/FST/CISO) enables flexible content delivery

The codebase is well-suited for Riivolution-style patching integration at the disc I/O layer, with game identification already in place and proven game-specific patching mechanisms already implemented.
