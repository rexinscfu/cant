local M = {}

-- Syntax groups
local syntax = {
    -- Keywords
    cantKeyword = {
        "ecu",
        "signal",
        "diagnostic",
        "service",
        "session",
        "security",
        "routine",
        "memory",
        "dtc",
        "frame",
        "pattern",
        "handler",
        "timeout",
        "response",
        "request",
    },

    -- Types
    cantType = {
        "u8", "u16", "u32", "u64",
        "i8", "i16", "i32", "i64",
        "f32", "f64",
        "bool", "string",
        "frame_id", "signal_id",
    },

    -- Constants
    cantConstant = {
        "true", "false", "null",
        "DEFAULT_SESSION",
        "PROGRAMMING_SESSION",
        "EXTENDED_SESSION",
    },

    -- Operators
    cantOperator = {
        "+", "-", "*", "/", "%",
        "==", "!=", "<", ">", "<=", ">=",
        "&&", "||", "!",
        "&", "|", "^", "~", "<<", ">>",
    },

    -- Special
    cantSpecial = {
        "@security_level",
        "@timeout",
        "@priority",
        "@periodic",
        "@diagnostic",
    },
}

-- Setup function
function M.setup()
    local highlight_groups = {
        CantKeyword = { link = "Keyword" },
        CantType = { link = "Type" },
        CantConstant = { link = "Constant" },
        CantOperator = { link = "Operator" },
        CantSpecial = { link = "Special" },
        CantComment = { link = "Comment" },
        CantString = { link = "String" },
        CantNumber = { link = "Number" },
    }

    -- Create highlight groups
    for group, def in pairs(highlight_groups) do
        vim.api.nvim_set_hl(0, group, def)
    end

    -- Set up treesitter if available
    if pcall(require, "nvim-treesitter") then
        require("nvim-treesitter.configs").setup({
            ensure_installed = { "cant" },
            highlight = {
                enable = true,
            },
        })
    end
end

return M 