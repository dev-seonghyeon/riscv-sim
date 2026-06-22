-- .nvim.lua — 프로젝트 로컬 설정 (Linux kernel style)

local grp = vim.api.nvim_create_augroup("kernel_style", { clear = true })

vim.api.nvim_create_autocmd("FileType", {
  group = grp,
  pattern = { "c", "cpp" },   -- .h 파일도 filetype이 c라 같이 잡힘
  callback = function()
    -- 탭: 진짜 탭 문자, 폭 8칸 (커널 필수)
    vim.bo.expandtab   = false
    vim.bo.tabstop     = 8
    vim.bo.softtabstop = 8
    vim.bo.shiftwidth  = 8

    -- 80컬럼 기준선
    vim.bo.textwidth   = 80
    vim.wo.colorcolumn = "+1"   -- 81번째 칸 표시

    -- 커널식 C 들여쓰기
    vim.bo.cindent     = true
    vim.bo.cinoptions  = ":0,l1,t0,g0,(0"

    -- 탭 / 끝 공백 시각화 (커널은 끝 공백 금지)
    vim.wo.list = true
    vim.opt_local.listchars = { tab = "» ", trail = "·", nbsp = "+" }
  end,
})
