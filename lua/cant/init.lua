local M = {}

-- Plugin initialization
function M.setup()
    vim.filetype.add({
        extension = {
            cant = "cant"
        }
    })
end

return M 