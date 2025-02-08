local M = {}

function M.setup()
    vim.filetype.add({
        extension = {
            cant = function(path, bufnr)
                -- Set up buffer-local options
                vim.bo[bufnr].syntax = "cant"
                vim.bo[bufnr].commentstring = "// %s"
                vim.bo[bufnr].formatoptions = "tcqj"

                return "cant"
            end
        },
        pattern = {
            -- Match files that look like CANT configs
            [".*%.cant%.conf"] = "cant",
            [".*%.cant%.json"] = "cant",
        }
    })
end

return M 