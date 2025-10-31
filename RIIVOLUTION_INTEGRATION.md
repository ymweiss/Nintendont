# Riivolution Integration into Nintendont

**Status**: Phase 1 - XML Parsing Infrastructure Complete

**Date Started**: 2025-10-31

---

## Overview

This document tracks the integration of Riivolution-style XML patching capabilities into Nintendont. The integration follows strict project isolation rules - all code and libraries are self-contained within the Nintendont project.

## Phase 1: XML Parsing Infrastructure ✓ COMPLETE

### Objectives
- Copy XML libraries from HAI-Riivolution to Nintendont
- Integrate XML parsing into Nintendont's build system
- Create basic XML parser infrastructure

### Implementation

#### 1. Library Integration

**Copied Libraries:**
- `loader/lib/libxml2/` - Core XML parsing library (C)
- `loader/lib/libxml++/` - C++ wrapper for libxml2

**Source**: HAI-Riivolution/launcher/lib/

**Important**: These are **copied** into Nintendont, not linked. This maintains project independence as required.

#### 2. Makefile Updates

**File**: `loader/Makefile`

**Changes Made:**

1. Added XML library include paths:
```makefile
INCLUDES := include ../fatfs \
            lib/libxml2/include lib/libxml++ lib/libxml++/libxml++
```

2. Added XML libraries to linker:
```makefile
LIBS := -lxml++ -lxml2 -lfreetype -lpngu -lpng -lz \
        -lwiiuse -lwiidrc -lwupc -lbte -lfatfs-ppc -logc -ldi -lm
```

3. Added XML library paths:
```makefile
LDFLAGS += -L$(CURDIR)/../../fatfs -L$(CURDIR)/lib/libxml2 -L$(CURDIR)/lib/libxml++
```

4. Added XML library build targets:
```makefile
LIB_XML2 := $(CURDIR)/lib/libxml2/libxml2.a
LIB_XMLXX := $(CURDIR)/lib/libxml++/libxml++.a

$(LIB_XML2):
	@$(MAKE) --no-print-directory -C $(CURDIR)/lib/libxml2

$(LIB_XMLXX): $(LIB_XML2)
	@$(MAKE) --no-print-directory -C $(CURDIR)/lib/libxml++

$(BUILD): $(LIB_XML2) $(LIB_XMLXX)
	# ... rest of build
```

5. Added clean targets for XML libraries:
```makefile
clean:
	# ... existing clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/lib/libxml2 clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/lib/libxml++ clean
```

#### 3. XML Parser Infrastructure

**Created Files:**

1. **`loader/include/riivolution_config.h`**
   - Public API for Riivolution XML parsing
   - Functions:
     - `RiiConfig_Init()` - Initialize XML parser
     - `RiiConfig_LoadXML()` - Load and parse XML file
     - `RiiConfig_Cleanup()` - Clean up resources

2. **`loader/source/riivolution_config.cpp`**
   - Basic XML parsing implementation
   - Uses libxml2 for XML parsing
   - Currently implements:
     - XML file loading
     - Root element validation (`<wiidisc>`)
     - Version attribute parsing
   - Prepared for full Riivolution XML schema implementation

### Testing

**Build Test:**
```bash
cd /home/yaakov/WiiProjects/Nintendont
make
```

**Expected Result:**
- XML libraries compile successfully
- Loader compiles and links with XML libraries
- `loader.dol` generated with XML parsing capabilities

### Next Steps

See **Phase 2** below.

---

## Phase 2: XML Schema Implementation (TODO)

### Objectives
- Implement full Riivolution XML schema parsing
- Support `<id>`, `<section>`, `<option>`, `<choice>`, `<patch>` elements
- Parse file replacement and memory patch definitions
- Validate game ID matching

### Tasks
- [ ] Implement game ID validation (`<id>` element)
- [ ] Parse configuration hierarchy (sections → options → choices)
- [ ] Parse patch definitions (file replacement, memory patches)
- [ ] Implement parameter substitution (`{$param}`)
- [ ] Create data structures for patch storage
- [ ] Add configuration save/load (persistent settings)

### Reference
- See [RIIVOLUTION_ANALYSIS.md](../HAI-Riivolution/RIIVOLUTION_ANALYSIS.md) for complete XML schema
  *(Note: Link only works if HAI-Riivolution is cloned to the same parent directory)*
- See HAI-Riivolution `launcher/source/riivolution_config.cpp` for reference implementation (lines 1-600)

---

## Phase 3: File Replacement System (TODO)

### Objectives
- Implement file redirection at disc I/O level
- Integrate with Nintendont's existing file loading (ISO/FST/CISO)
- Support file replacements from SD/USB

### Integration Points

**Key Files** (from [NINTENDONT_ANALYSIS.md](NINTENDONT_ANALYSIS.md)):
- `kernel/ISO.c` - ISO/CISO reading
- `kernel/FST.c` - Extracted FST mode
- `kernel/DI.c` - Disc interface
- `kernel/Patch.c` - Existing patch system

### Tasks
- [ ] Hook `ISOReadDirect()` for file redirection
- [ ] Hook `FSTRead()` for file redirection
- [ ] Implement file path mapping (disc → SD/USB)
- [ ] Add file cache system for redirected files
- [ ] Handle enlarged files (shift space allocation)
- [ ] Test with actual game file replacements

---

## Phase 4: Memory Patching (TODO)

### Objectives
- Implement memory patching during game load
- Support direct offset patches, search/replace, and Ocarina codes
- Integrate with existing `DoPatches()` mechanism

### Integration Points

**Key Functions** (from [NINTENDONT_ANALYSIS.md](NINTENDONT_ANALYSIS.md)):
- `kernel/Patch.c:DoPatches()` - Called during apploader
- `kernel/Patch.c:PatchGame()` - Game-specific patches

### Tasks
- [ ] Parse memory patch definitions from XML
- [ ] Implement direct offset patching
- [ ] Implement search/replace pattern matching
- [ ] Implement Ocarina code support (optional)
- [ ] Apply patches during DOL load
- [ ] Test with known game patches

---

## Phase 5: User Interface Integration (TODO)

### Objectives
- Add Riivolution XML scanning on SD/USB
- Create patch selection menu
- Implement configuration save/load

### Tasks
- [ ] Scan for `.xml` files in `/riivolution/` directory
- [ ] Add menu for patch selection
- [ ] Extend `NIN_CFG` structure for patch settings
- [ ] Save/load patch configuration
- [ ] Display enabled patches during game launch

---

## Phase 6: Testing and Refinement (TODO)

### Objectives
- Test with real Riivolution XML files
- Verify compatibility with existing Riivolution patches
- Performance optimization
- Bug fixes

### Test Cases
- [ ] Test with simple file replacement patch
- [ ] Test with memory patch
- [ ] Test with combined file + memory patches
- [ ] Test with multiple games
- [ ] Test region-specific patches
- [ ] Performance testing (load times, file access)

---

## Technical Notes

### Project Isolation Compliance

This integration follows WiiProjects general rules:

1. ✓ All source code copied into Nintendont (not linked to HAI-Riivolution)
2. ✓ All headers self-contained within Nintendont
3. ✓ No `#include "../HAI-Riivolution/..."` references
4. ✓ Libraries copied and maintained independently

### Architecture Alignment

**Nintendont Architecture:**
- Loader (PPC) - Where XML parsing happens
- Kernel (ARM) - Where file redirection happens
- Patch system - Where memory patches happen

**Integration Strategy:**
- **Phase 1-2**: Loader parses XML and builds patch data structures
- **Phase 3**: Kernel receives patch information and redirects file I/O
- **Phase 4**: Patch system applies memory patches during game load

### Memory Considerations

**Nintendont Memory Map** (from [NINTENDONT_ANALYSIS.md](NINTENDONT_ANALYSIS.md)):
- Loader: 0x80000000 region
- Kernel: 0x12000000 region
- Game: 0x80000000+ region (after loader exits)

**Patch Storage:**
- XML parsing in loader (temporary)
- Patch data passed to kernel via config structure
- Kernel maintains patch state during game execution

---

## References

- [NINTENDONT_ANALYSIS.md](NINTENDONT_ANALYSIS.md) - Nintendont architecture analysis
- [RIIVOLUTION_ANALYSIS.md](../HAI-Riivolution/RIIVOLUTION_ANALYSIS.md) - Riivolution system analysis
  *(Note: Link only works if HAI-Riivolution is cloned to the same parent directory)*
- [CLAUDE.md](../CLAUDE.md) - Project isolation rules
  *(Note: Link only works if both projects are in the WiiProjects workspace)*
- HAI-Riivolution source code (reference only, not linked)

---

## Change Log

### 2025-10-31 - Phase 1 Complete
- Copied libxml2 and libxml++ to Nintendont
- Updated loader Makefile for XML library integration
- Created basic XML parser infrastructure (`riivolution_config.cpp/h`)
- Created this integration tracking document
