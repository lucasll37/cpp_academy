# =============================================================================
#  src/modules/Makefile
#
#  Este subprojeto NÃO usa o Meson da raiz. O motivo está no README.md, e a
#  razão curta é: módulos exigem ordem de compilação, e o suporte do Meson a
#  módulos com GCC 13 é experimental e frágil.
#
#  O que este Makefile demonstra, e que é o conteúdo didático:
#    1. `.cppm` precisa de `-x c++` explícito no GCC
#    2. cada interface é compilada ANTES de quem a importa
#    3. o artefato binário vai para `gcm.cache/`, gerado automaticamente
#    4. o link junta os `.o` normalmente — o `.gcm` não entra no link
# =============================================================================

CXX      := g++
CXXFLAGS := -std=c++20 -fmodules-ts -Wall -Wextra -Wpedantic

# ⚠ A ORDEM aqui é significativa, ao contrário de um projeto com headers:
#    geometria  não depende de ninguém
#    formatacao importa geometria      → precisa dele compilado antes
#    main       importa formatacao     → precisa dos dois
OBJS := geometria.o formatacao.o main.o

.PHONY: all run clean mostrar-cache

all: modules_demo

modules_demo: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# ── Regra para interfaces de módulo ──────────────────────────────────────────
#  O `-x c++` é OBRIGATÓRIO: sem ele o GCC não reconhece a extensão `.cppm` e
#  responde "linker input file unused because linking not done".
#  Compilar a interface produz DOIS artefatos: o `.o` (com o código) e o
#  `gcm.cache/<nome>.gcm` (com a interface, para os consumidores).
%.o: %.cppm
	$(CXX) $(CXXFLAGS) -x c++ -c $< -o $@

# ── Regra para código normal ─────────────────────────────────────────────────
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── As dependências que o Make não descobre sozinho ──────────────────────────
#  Um `#include` o Make consegue rastrear com -MMD. Um `import` NÃO — a
#  dependência é sobre o `.gcm`, que só existe depois de compilar.
#  Em projetos reais, isso exige um scanner (o `-fdeps-format` do GCC 14+, ou
#  o `clang-scan-deps`). Aqui declaramos à mão:
formatacao.o: geometria.o
main.o:       formatacao.o geometria.o

run: modules_demo
	./modules_demo

mostrar-cache: modules_demo
	@echo "=== artefatos binários de módulo (gcm.cache/) ==="
	@find gcm.cache -name '*.gcm' -exec ls -lh {} \; 2>/dev/null || echo "(vazio)"

clean:
	rm -rf $(OBJS) modules_demo gcm.cache
