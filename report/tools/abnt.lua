-- ============================================================================
--  Filtro pandoc: numeracao e estilos ABNT para a saida .docx
--
--  Faz o que o writer do pandoc nao faz sozinho:
--    * numera legendas de figura, tabela e listagem ("Figura 12 - ...");
--    * transforma o `caption=` do lstlisting, que o writer descartaria, em um
--      paragrafo de legenda logo abaixo do bloco de codigo;
--    * alinha o numero das equacoes a direita, como pede a secao FORMATACAO do
--      modelo do IEAv.
--
--  Os estilos nomeados aqui ("Legenda Codigo", "Equacao") sao definidos em
--  build/doc/reference.docx por tools/build_reference_docx.py. As listas de
--  ilustracoes e de tabelas sao montadas no Word a partir desses estilos, e
--  nao de campos SEQ -- por isso o nome do estilo importa.
-- ============================================================================

local eqnums = {}      -- numeros das equacoes, na ordem do documento
local eq_index = 0
local n_fig, n_tab, n_lst = 0, 0, 0

local DASH = "\u{2013}"  -- travessao curto usado entre o numero e o titulo

--- Le a lista de numeros de equacao gravada pelo tex_preprocess.py.
local function load_eqnums(meta)
  local path = meta.eqnums_file and pandoc.utils.stringify(meta.eqnums_file) or nil
  if not path then return end
  local fh = io.open(path, "r")
  if not fh then return end
  local raw = fh:read("a")
  fh:close()
  local ok, decoded = pcall(pandoc.json.decode, raw)
  if ok and decoded then
    for _, v in ipairs(decoded) do eqnums[#eqnums + 1] = tostring(v) end
  end
end

--- "Figura 12 - " como lista de inlines, para prefixar uma legenda existente.
local function label_inlines(word, number)
  return { pandoc.Str(word), pandoc.Space(), pandoc.Str(tostring(number)),
           pandoc.Space(), pandoc.Str(DASH), pandoc.Space() }
end

--- Prefixa o primeiro bloco de uma legenda, preservando a formatacao interna.
local function prefix_caption(blocks, word, number)
  local prefix = label_inlines(word, number)
  if #blocks == 0 then
    return { pandoc.Plain(prefix) }
  end
  local first = blocks[1]
  if first.content then
    local merged = prefix
    for _, inline in ipairs(first.content) do merged[#merged + 1] = inline end
    first.content = merged
  end
  return blocks
end

--- Converte o texto LaTeX de um `caption=` em inlines do pandoc.
local function latex_to_inlines(latex)
  local ok, doc = pcall(pandoc.read, latex, "latex")
  if not ok or #doc.blocks == 0 or not doc.blocks[1].content then
    return { pandoc.Str(latex) }
  end
  return doc.blocks[1].content
end

local function on_figure(fig)
  n_fig = n_fig + 1
  fig.caption.long = prefix_caption(fig.caption.long, "Figura", n_fig)
  return fig
end

local function on_table(tbl)
  n_tab = n_tab + 1
  tbl.caption.long = prefix_caption(tbl.caption.long, "Tabela", n_tab)
  return tbl
end

local function on_codeblock(cb)
  local caption = cb.attributes["caption"]
  if not caption then return nil end
  n_lst = n_lst + 1
  -- O caption sai das opcoes do lstlisting; deixa-lo no bloco faria o writer
  -- emitir um atributo inutil no XML.
  cb.attributes["caption"] = nil
  local inlines = label_inlines("Listagem", n_lst)
  for _, inline in ipairs(latex_to_inlines(caption)) do
    inlines[#inlines + 1] = inline
  end
  local legenda = pandoc.Div({ pandoc.Para(inlines) },
                             pandoc.Attr("", {}, { ["custom-style"] = "Legenda Codigo" }))
  return { cb, legenda }
end

local TAB = pandoc.RawInline("openxml", "<w:r><w:tab/></w:r>")

--- Equacao destacada: centralizada, com o numero entre parenteses na margem
--- direita -- a apresentacao que a secao FORMATACAO do modelo pede.
---
--- A formula vira InlineMath de proposito: o writer do pandoc envolve uma
--- DisplayMath em <m:oMathPara>, que o Word obriga a ocupar um paragrafo
--- inteiro, e o numero acabaria sozinho na linha de baixo. Nenhuma das
--- equacoes deste relatorio usa somatorio, integral ou limite, entao o modo
--- inline as desenha exatamente igual. A posicao vem de duas tabulacoes
--- definidas no estilo "Equacao": uma centralizada e outra a direita.
local function on_para(para)
  if #para.content ~= 1 or para.content[1].t ~= "Math" then return nil end
  if para.content[1].mathtype ~= "DisplayMath" then return nil end
  eq_index = eq_index + 1
  local number = eqnums[eq_index]
  if not number or number == "" then return nil end
  local content = {
    TAB,
    pandoc.Math("InlineMath", para.content[1].text),
    TAB,
    pandoc.Str("(" .. number .. ")"),
  }
  return pandoc.Div({ pandoc.Para(content) },
                    pandoc.Attr("", {}, { ["custom-style"] = "Equacao" }))
end

function Pandoc(doc)
  load_eqnums(doc.meta)
  -- Um unico caminhamento, na ordem do documento: e o que garante que a
  -- numeracao das legendas siga a sequencia em que elas aparecem.
  local walked = doc:walk({
    Figure = on_figure,
    Table = on_table,
    CodeBlock = on_codeblock,
    Para = on_para,
  })
  io.stderr:write(string.format(
    "  filtro abnt: %d figuras, %d tabelas, %d listagens, %d equacoes\n",
    n_fig, n_tab, n_lst, eq_index))
  return walked
end
