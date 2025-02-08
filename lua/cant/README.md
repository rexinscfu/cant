CANT Language Support for Neovim

Provides syntax highlighting and basic editor support for the CANT programming language.

Features:
- Syntax highlighting for CANT keywords, types, and operators
- Filetype detection for .cant files
- Support for CANT configuration files
- Integration with nvim-treesitter (if available)

Installation:
Using packer.nvim:
```lua
use {
    'your-username/cant',
    config = function()
        require('cant').setup()
    end
}
```

Using lazy.nvim:
```lua
{
    'your-username/cant',
    config = true
}
```

Configuration:
No additional configuration needed. The plugin will automatically detect and highlight .cant files.

Supported Filetypes:
- .cant (CANT source files)
- .cant.conf (CANT configuration files)
- .cant.json (CANT JSON configuration files)

Features:
- Syntax highlighting for:
  - Keywords (ecu, signal, diagnostic, etc.)
  - Types (u8, u16, u32, etc.)
  - Constants
  - Operators
  - Special annotations (@security_level, @timeout, etc.)
- Comment string support (// for line comments)
- Basic formatting options

Requirements:
- Neovim 0.5 or later
- Optional: nvim-treesitter for enhanced highlighting 