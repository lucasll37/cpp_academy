# plugin_registry — Plugins carregados em runtime com `dlopen`

## O que é este projeto?

Um programa que, ao iniciar, **varre um diretório, encontra arquivos `.so` que nunca viu antes,
carrega cada um, descobre o que ele sabe fazer e passa a usá-lo** — tudo sem que uma única
linha do programa principal conheça esses arquivos em tempo de compilação.

Os dois plugins de exemplo são controladores de voo (`pid.so` e `lqr.so`). Poderiam ser
qualquer coisa: filtros de imagem, codecs, regras de negócio. O assunto aqui é o **mecanismo
de plugin**, não aeronáutica — e há uma seção explicando os dois algoritmos para quem quiser
entender o exemplo por inteiro.

```bash
make build      # compila o executável e os dois .so em build/
make install    # copia para dist/bin/ e dist/plugins/
./dist/bin/plugin_registry
```

O programa acha os plugins sozinho (veja [Onde os plugins são
procurados](#onde-os-plugins-são-procurados)). Para apontar um diretório à mão:

```bash
./dist/bin/plugin_registry /caminho/para/os/so
```

---

## Antes de começar

Nada do que vem depois usa um termo que não esteja definido aqui.

### O que é uma biblioteca compartilhada (`.so`)

Quando você compila um `.cpp`, sai um **arquivo objeto** (`.o`): código de máquina com
lacunas — chamadas para funções que moram em outro lugar. O **linker** preenche as lacunas.
Ele pode fazer isso de três jeitos:

| Forma | O que acontece | Quando é decidido |
|---|---|---|
| **Biblioteca estática** (`.a`) | O código da biblioteca é **copiado para dentro** do seu executável. O `.a` não é mais necessário depois. | Em tempo de *link* |
| **Biblioteca compartilhada, ligada** (`.so` na linha de link) | O executável guarda só o **nome** da biblioteca. Ao iniciar o programa, o *dynamic linker* (`/lib64/ld-linux-x86-64.so.2`, também chamado `ld.so`) abre o `.so`, mapeia na memória e resolve os nomes. | Ao **iniciar** o programa |
| **Biblioteca compartilhada, carregada por `dlopen`** | Ninguém menciona o `.so` no link. O **próprio programa** pede, em runtime, que aquele arquivo seja carregado — e o arquivo pode ser escolhido a partir de uma string lida de um diretório. | Quando o **seu código** manda |

A terceira linha é este subprojeto. É a única em que o conjunto de bibliotecas **não precisa
ser conhecido quando o programa é compilado**.

Veja a diferença na prática: o `plugin_registry` não menciona `pid` em lugar nenhum do link,
mas `pid.so` tem suas próprias dependências, resolvidas quando ele é carregado:

```console
$ ldd dist/plugins/pid.so
	libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6
	libgcc_s.so.1  => /lib/x86_64-linux-gnu/libgcc_s.so.1
	libc.so.6      => /lib/x86_64-linux-gnu/libc.so.6
	libm.so.6      => /lib/x86_64-linux-gnu/libm.so.6
	/lib64/ld-linux-x86-64.so.2
```

### Símbolo, tabela de símbolos e `nm`

Um **símbolo** é um nome exportado por um binário: o nome de uma função ou de uma variável
global, junto com o endereço onde ela está. A lista deles é a **tabela de símbolos**, e
`nm -D` (de *dynamic*) mostra a parte dela que o carregador dinâmico enxerga.

`dlsym(handle, "plugin_name")` é literalmente uma busca nessa tabela **por string**. Se o
nome não estiver lá, `dlsym` devolve `nullptr`.

```console
$ nm -D --defined-only dist/plugins/pid.so | grep ' T '
00000000000037aa T create_controller
00000000000037ee T destroy_controller
0000000000003799 T plugin_name
```

Esses três — e só esses três — são os pontos de entrada do plugin. O `T` maiúsculo significa
"símbolo global na seção de código". Símbolos internos aparecem como `t` minúsculo e **não
são visíveis** para o `dlsym`.

### *Name mangling*, visto de perto

C++ tem sobrecarga, namespaces e templates. Como um símbolo é um nome só, o compilador
codifica namespace, classe e tipos dos parâmetros dentro do nome. Isso é o **name mangling**.

Compare, no mesmo arquivo, um método C++ e uma função `extern "C"`:

```console
$ nm dist/plugins/pid.so | grep -i compute
00000000000039ba t _ZN6plugin13PIDController7computeERKNS_5StateE

$ nm -C dist/plugins/pid.so | grep -i compute       # -C = "demangle"
00000000000039ba t plugin::PIDController::compute(plugin::State const&)

$ nm dist/plugins/pid.so | grep plugin_name
0000000000003799 T plugin_name                       # ← intacto
```

Leia o nome decorado da esquerda para a direita: `_ZN` inicia um nome aninhado, `6plugin` é um
identificador de 6 letras, `13PIDController` de 13, `7compute` de 7, e `ERKNS_5StateE` é
"recebe referência-const para `plugin::State`".

**Um nome desses não dá para escrever à mão numa chamada de `dlsym`** — ele muda entre
compiladores e até entre versões. Por isso os pontos de entrada do plugin são declarados
`extern "C"`, que desliga o mangling e deixa o símbolo com o nome exato que você escreveu.

### ABI × API

- **API** (*Application Programming Interface*) é o contrato no nível do **código-fonte**:
  quais funções existem, que argumentos recebem. Quebrar a API faz a **compilação** falhar.
- **ABI** (*Application Binary Interface*) é o contrato no nível do **binário**: em qual
  registrador vai cada argumento, quantos bytes tem cada struct, em que ordem ficam as
  entradas da vtable, como um `std::string` é representado por dentro.

Quebrar a ABI **não faz a compilação falhar**. O programa liga, roda e corrompe memória.
Essa é a razão de a fronteira `.so` ↔ host exigir cuidado — assunto da seção
[O que pode e o que não pode atravessar a fronteira](#-o-que-pode-e-o-que-não-pode-atravessar-a-fronteira).

### Visibilidade de símbolos

Por padrão, no GCC/Clang, todo símbolo global de um `.so` é **exportado**. A flag
`-fvisibility=hidden` inverte isso: tudo nasce **oculto**, e só é exportado o que estiver
marcado com `__attribute__((visibility("default")))`.

Este projeto usa `-fvisibility=hidden` (em `plugins/meson.build`). A consequência é grande o
bastante para ter [seção própria](#-visibilidade-a-armadilha-que-quebra-o-plugin-em-silêncio).

### POSIX, e o que isso quer dizer no Windows

`dlopen`/`dlsym`/`dlclose` são da API POSIX — Linux, macOS, BSD. No Windows os equivalentes
são `LoadLibrary`, `GetProcAddress` e `FreeLibrary`, e a exportação usa
`__declspec(dllexport)` no lugar do atributo de visibilidade. Os conceitos são os mesmos; os
nomes, não. Este subprojeto é Linux.

---

## Por que estudar isto?

Comece pelo problema, não pela API.

**Você escreveu um simulador de voo e quer testar cinco algoritmos de controle.** Sem
plugins, cada algoritmo é um `.cpp` do projeto, um `#include`, um `case` no `switch` e uma
recompilação do simulador inteiro. Testar um algoritmo novo obriga a **redistribuir o
programa**.

Com plugins:

- **um terceiro estende seu programa sem ter o seu código.** Ele compila um `.so` contra o
  seu cabeçalho de interface e o larga na pasta. Este é o modelo do VST (áudio), do LADSPA, dos
  plugins do Photoshop, do GStreamer, do `NVIDIA_ICD` do OpenGL;
- **você troca a implementação sem parar o processo.** O simulador continua rodando enquanto
  o controlador muda;
- **um plugin com defeito não derruba o programa.** `load_directory` captura a exceção de cada
  `.so` individualmente: um plugin ruim vira um aviso no `stderr`, não um crash;
- **o que se paga por isso** é uma fronteira binária que precisa ser respeitada — e é
  justamente o que o resto deste README ensina.

---

## Estrutura de arquivos

```
src/plugin_registry/
├── main.cpp               ← host: localiza o diretório, carrega, roda episódios
├── icontroller.hpp        ← A INTERFACE + a macro REGISTER_CONTROLLER
├── plugin_registry.hpp    ← ControllerRegistry: singleton, dlopen/dlsym/dlclose
├── meson.build            ← executável + link_args ['-ldl'] + subdir('plugins')
├── plugins/
│   ├── pid_controller.cpp ← plugin 1 → pid.so
│   ├── lqr_controller.cpp ← plugin 2 → lqr.so
│   └── meson.build        ← name_prefix:'' + -fvisibility=hidden + install_dir
└── README.md
```

Depois de `make install`:

```
dist/
├── bin/plugin_registry
└── plugins/
    ├── pid.so             ← sem o "lib" da frente; veja name_prefix abaixo
    └── lqr.so
```

---

## O menor plugin possível

Antes de olhar o controlador de voo, o esqueleto inteiro em três arquivos. **Isto compila e
roda** — sem meson, sem `fmt`, sem nada.

`isaudacao.hpp` — o contrato, compartilhado pelos dois lados:

```cpp
#pragma once
struct ISaudacao {
    virtual ~ISaudacao() = default;
    virtual void falar() = 0;
};

// extern "C"  → sem name mangling: o símbolo fica com o nome que escrevemos
// visibility  → exporta mesmo compilando com -fvisibility=hidden
#define EXPORTAR extern "C" __attribute__((visibility("default")))
```

`ola.cpp` — o plugin:

```cpp
#include "isaudacao.hpp"
#include <cstdio>

struct Ola : ISaudacao {
    void falar() override { std::puts("olá do plugin!"); }
};

EXPORTAR const char* plugin_nome()          { return "ola"; }
EXPORTAR ISaudacao*  criar()                { return new Ola(); }
EXPORTAR void        destruir(ISaudacao* p) { delete p; }
```

`host.cpp` — o programa que carrega:

```cpp
#include "isaudacao.hpp"
#include <dlfcn.h>
#include <cstdio>
#include <memory>

int main() {
    void* h = dlopen("./ola.so", RTLD_NOW | RTLD_LOCAL);
    if (!h) { std::printf("dlopen: %s\n", dlerror()); return 1; }

    auto nome     = reinterpret_cast<const char* (*)()>(dlsym(h, "plugin_nome"));
    auto criar    = reinterpret_cast<ISaudacao*  (*)()>(dlsym(h, "criar"));
    auto destruir = reinterpret_cast<void (*)(ISaudacao*)>(dlsym(h, "destruir"));
    if (!nome || !criar || !destruir) {
        std::printf("dlsym: %s\n", dlerror()); dlclose(h); return 1;
    }

    std::printf("plugin: %s\n", nome());
    {
        // unique_ptr com deleter customizado: quem destrói é o .so, não o host.
        std::unique_ptr<ISaudacao, void(*)(ISaudacao*)> obj(criar(), destruir);
        obj->falar();
    }                       // ← o objeto morre AQUI, antes do dlclose
    dlclose(h);
}
```

```console
$ g++ -std=c++17 -shared -fPIC -fvisibility=hidden ola.cpp -o ola.so
$ g++ -std=c++17 host.cpp -o host -ldl
$ ./host
plugin: ola
olá do plugin!
```

Cinco detalhes que valem por si:

- **`-shared -fPIC`** — `-shared` produz um `.so` em vez de executável; `-fPIC` gera código
  independente de posição, obrigatório porque o `.so` pode ser mapeado em qualquer endereço;
- **`-ldl`** — no Linux, `dlopen` mora na `libdl`, separada da libc; sem essa flag o link
  falha com *undefined reference to `dlopen`*;
- **o `reinterpret_cast`** — `dlsym` devolve `void*`, sem nenhuma informação de tipo. Você
  **afirma** a assinatura. Se errar, é UB silencioso. O contrato existe só no cabeçalho
  compartilhado, e é por isso que ele é a peça mais importante do desenho;
- **as chaves em volta do `unique_ptr`** — não são estilo. Elas garantem que o objeto seja
  destruído **antes** do `dlclose`. Veja [dlclose e o tempo de
  vida](#-dlclose-e-o-tempo-de-vida);
- **`criar()` faz `new` dentro do `.so`, `destruir()` faz `delete` dentro do `.so`.** O host
  nunca faz nenhum dos dois. O porquê está em [O que pode e o que não pode atravessar a
  fronteira](#-o-que-pode-e-o-que-não-pode-atravessar-a-fronteira).

---

## A interface real — `icontroller.hpp`

```cpp
namespace plugin {

// Dados que atravessam a fronteira: structs simples, sem vtable,
// sem alocação escondida, sem ponteiro para dentro delas.
struct State {
    double altitude_ft  = 0.0;
    double speed_kts    = 0.0;
    double pitch_deg    = 0.0;
    double roll_deg     = 0.0;
    double heading_deg  = 0.0;
};

struct Action {
    double elevator  = 0.0;   // profundor  [-1, 1]
    double throttle  = 0.0;   // acelerador [ 0, 1]
    double aileron   = 0.0;   // aileron    [-1, 1]
};

// O contrato. Todo plugin herda daqui.
class IController {
public:
    virtual ~IController() = default;

    virtual std::string name() const = 0;
    virtual void        reset()      = 0;   // zera o estado interno entre episódios
    virtual Action      compute(const State& state) = 0;
};

} // namespace plugin
```

> **⚠ `name()` devolve `std::string`, e isso é um risco assumido.** `std::string` atravessando
> a fronteira só funciona porque host e plugins são compilados **pelo mesmo compilador, com a
> mesma libstdc++ e as mesmas flags**, no mesmo `make build`. Num sistema de plugins de
> verdade — em que terceiros compilam os `.so` — isso é exatamente o que quebra. A assinatura
> segura seria `virtual const char* name() const = 0;`. Detalhes em
> [O que pode e o que não pode atravessar a fronteira](#-o-que-pode-e-o-que-não-pode-atravessar-a-fronteira).

### A macro `REGISTER_CONTROLLER` — o ponto de entrada

Cada plugin termina com **uma linha**:

```cpp
REGISTER_CONTROLLER(plugin::PIDController, "pid")
```

E a macro (definida no fim de `icontroller.hpp`) expande para exatamente isto:

```cpp
extern "C" __attribute__((visibility("default")))
const char* plugin_name() { return "pid"; }

extern "C" __attribute__((visibility("default")))
plugin::IController* create_controller() { return new plugin::PIDController(); }

extern "C" __attribute__((visibility("default")))
void destroy_controller(plugin::IController* p) { delete p; }
```

Três decisões dentro dessas nove linhas:

- **`extern "C"`** desliga o name mangling, para que `dlsym(h, "plugin_name")` encontre o
  símbolo pelo nome literal;
- **`__attribute__((visibility("default")))`** exporta o símbolo apesar de
  `-fvisibility=hidden` — leia a próxima seção antes de removê-lo;
- **por que macro e não template ou função normal?** Template tem name mangling, então
  `dlsym` não acharia. Uma função normal exigiria que o plugin se registrasse sozinho em algum
  ponto de inicialização — mais frágil. A macro resolve no pré-processador: custo zero em
  runtime, e é impossível esquecer metade dela.

E note o que **não** é `extern "C"`: `PIDController` é uma classe C++ normal, com herança,
vtable e `std::string`. Só as três funções de entrada precisam de ligação C. O interior do
plugin pode ser C++ moderno sem restrição.

### ⚠ Visibilidade: a armadilha que quebra o plugin em silêncio

`plugins/meson.build` compila os dois plugins com:

```meson
plugin_flags = ['-fvisibility=hidden']
```

Com essa flag, escrever só `extern "C" const char* plugin_name() { ... }` **não é suficiente**.
O símbolo fica oculto, `dlsym` devolve `nullptr` e o registry rejeita o plugin com uma
mensagem que parece falar de outra coisa:

```console
$ g++ -std=c++17 -shared -fPIC -fvisibility=hidden sem_atributo.cpp -o x.so
$ nm -D x.so | grep plugin_name          # nada: o símbolo não foi exportado
$ ./host
dlopen: ./x.so: undefined symbol: plugin_name
```

`extern "C"` e visibilidade resolvem problemas **diferentes**:

| | Resolve | Não resolve |
|---|---|---|
| `extern "C"` | o **nome** do símbolo (sem mangling) | se ele é exportado |
| `visibility("default")` | se ele é **exportado** | o nome |

Você precisa dos dois. É por isso que a macro tem os dois — e é o motivo de este README
mostrar `REGISTER_CONTROLLER` em vez de funções escritas à mão.

E por que usar `-fvisibility=hidden`, se dá trabalho? Porque com ele o `.so` exporta **só**
a sua interface: binário menor, carregamento mais rápido (menos símbolos para o `ld.so`
resolver) e, principalmente, **sem colisão** — dois plugins com uma função interna
`static double clamp(...)` de mesmo nome não interferem um no outro.

---

## O registry — `plugin_registry.hpp`

### `dlopen` e as quatro flags

```cpp
void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
if (!handle)
    throw std::runtime_error("dlopen falhou em '" + path + "': " + dlerror());
```

O retorno **não é um ponteiro para objeto**: é um token opaco que identifica a biblioteca
carregada. Só serve para passar de volta a `dlsym` e `dlclose`.

As flags vêm em dois pares, e cada escolha tem consequência prática:

| Flag | O que faz | Consequência |
|---|---|---|
| `RTLD_NOW` | Resolve **todos** os símbolos indefinidos do `.so` no momento do carregamento. | Se falta alguma coisa, o `dlopen` **falha na hora**, com mensagem. Carregar é mais lento. |
| `RTLD_LAZY` | Resolve cada símbolo só na primeira chamada. | Carregar é mais rápido — mas um símbolo faltando derruba o programa **no meio da execução**, talvez horas depois. |
| `RTLD_LOCAL` | Os símbolos do `.so` **não** ficam disponíveis para outros `.so` carregados depois. | Isolamento: dois plugins podem ter símbolos globais de mesmo nome sem se atrapalhar. |
| `RTLD_GLOBAL` | Os símbolos ficam globais no processo. | Necessário quando um plugin depende de símbolos de outro. Abre a porta para colisões silenciosas. |

Este projeto usa `RTLD_NOW | RTLD_LOCAL`: **falhar cedo e ficar isolado**. É o padrão certo
para plugins independentes.

### `dlsym` e o motivo de chamar `dlerror()` antes

```cpp
dlerror();                                             // 1. limpa erro pendente

auto fn_name = reinterpret_cast<FnPluginName>(
    dlsym(handle, "plugin_name"));                     // 2. busca

if (const char* err = dlerror()) {                     // 3. só ENTÃO verifica
    dlclose(handle);
    throw std::runtime_error("plugin_name não encontrado em '" + path + "': " + err);
}
```

Por que não basta testar `if (!fn_name)`? Porque um símbolo pode **legitimamente valer
`nullptr`** — uma variável global inicializada com zero, por exemplo. `dlsym` devolveria
`nullptr` sem que houvesse erro nenhum. O único jeito confiável é limpar o estado de erro
antes, chamar, e ver se `dlerror()` passou a devolver mensagem.

O código real repete esse bloco **três vezes**, uma por símbolo, cada uma com `dlclose` antes
de lançar — senão um plugin rejeitado ficaria mapeado na memória para sempre:

```cpp
dlerror(); auto fn_name    = ... dlsym(handle, "plugin_name");        // + checagem
dlerror(); auto fn_create  = ... dlsym(handle, "create_controller");  // + checagem
dlerror(); auto fn_destroy = ... dlsym(handle, "destroy_controller"); // + checagem

std::string canon_name = fn_name();          // o nome vem do PRÓPRIO plugin,
                                             // não do nome do arquivo
if (plugins_.count(canon_name)) {            // dois .so com o mesmo nome canônico:
    dlclose(handle);                         // o primeiro ganha, o segundo é ignorado
    return;
}

plugins_[canon_name] = PluginHandle{
    .dl_handle = handle, .name = canon_name,
    .create    = fn_create, .destroy = fn_destroy,
};
```

O nome vindo de `plugin_name()`, e não do nome do arquivo, é o que permite renomear
`pid.so` sem quebrar `reg.create("pid")`.

### Descoberta automática de diretório

```cpp
int load_directory(const std::string& dir_path) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir)                                       // ⚠ sem esta checagem,
        throw std::runtime_error(                   //   readdir(nullptr) é UB
            "Não foi possível abrir o diretório: " + dir_path);

    int loaded = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fname = entry->d_name;
        if (!ends_with(fname, ".so")) continue;

        try {
            load_plugin(dir_path + "/" + fname);
            ++loaded;
        } catch (const std::exception& e) {
            std::cerr << "[registry] aviso: " << e.what() << "\n";   // isola a falha
        }
    }

    closedir(dir);
    return loaded;
}
```

O `try`/`catch` **dentro** do laço é a decisão de projeto: um `.so` corrompido, de outra
arquitetura ou sem os símbolos vira um aviso — os demais continuam carregando.

### Instanciação com deleter customizado

```cpp
std::unique_ptr<IController, FnDestroyController>
create(const std::string& name) {
    const PluginHandle& ph = plugins_.at(name);
    IController* raw = ph.create();          // new acontece DENTRO do .so
    return { raw, ph.destroy };              // e delete também vai acontecer lá
}
```

Duas coisas para entender aqui.

**O deleter faz parte do tipo.** `std::unique_ptr<T>` é, na verdade,
`std::unique_ptr<T, std::default_delete<T>>`. Trocar o deleter troca o **tipo inteiro** —
por isso `std::unique_ptr<IController, FnDestroyController>` não é atribuível a um
`std::unique_ptr<IController>`. Em troca, o ponteiro deixa de ser do tamanho de um ponteiro:
ele carrega também o ponteiro de função do deleter.

**Por que não simplesmente `delete`?** Porque o `operator delete` do host devolve a memória
ao alocador do **host**. Se o `.so` tiver sido linkado com um alocador próprio (`-static-libstdc++`,
uma libc diferente, um `operator new` sobrecarregado), o bloco veio de outro lugar:

```
     host                                pid.so
  ─────────────                       ─────────────
  reg.create("pid") ──► create_controller()
                          new PIDController   ──► alocador DO PLUGIN
                       ◄── ponteiro
  delete ptr  ✗ ──────► free() no alocador DO HOST
                        → o bloco não pertence a esse heap
                        → corrupção silenciosa ou abort
```

A regra vale para qualquer par de módulos: **quem aloca, libera.** O plugin exporta
`destroy_controller` justamente para ser o lado que libera.

### ⚠ `dlclose` e o tempo de vida

Esta é *a* armadilha do assunto, e ela não dá aviso nenhum.

Um objeto criado pelo plugin tem um **ponteiro de vtable** que aponta para dentro do `.so`.
Depois de `dlclose`, o sistema pode desmapear aquelas páginas. O objeto continua parecendo
válido — os campos ainda têm valores plausíveis — mas a primeira chamada virtual salta para
memória desmapeada:

```cpp
auto ctrl = reg.create("pid");   // vtable aponta para dentro de pid.so
dlclose(handle);                 // ✗ pid.so desmapeado
ctrl->compute(state);            // ✗ SIGSEGV, ou pior
```

A ordem correta é sempre: **destrua todas as instâncias → só então `dlclose`.**

O `ControllerRegistry` respeita isso por construção, e vale seguir o raciocínio:

```cpp
~ControllerRegistry() {
    for (auto& [name, ph] : plugins_)
        dlclose(ph.dl_handle);
}

static ControllerRegistry& instance() {
    static ControllerRegistry reg;   // Meyers singleton: destruído no fim do programa
    return reg;
}
```

O registry é um `static` local — destruído **no encerramento**, depois que todos os
`unique_ptr` locais de `main` e de `run_episode` já saíram de escopo. Se algum plugin fosse
guardado em outro objeto estático, a ordem de destruição entre unidades de tradução deixaria
de ser garantida e o problema voltaria. Um sistema de plugins mais rigoroso resolve isso com
contagem de referências: o handle só fecha quando a última instância morre.

### ⚠ O que pode e o que não pode atravessar a fronteira

| Atravessa com segurança | Por quê |
|---|---|
| `int`, `double`, `bool`, ponteiros crus | Representação fixada pela ABI da plataforma |
| **structs de dados simples**, como `State` e `Action` | Sem vtable, sem alocação escondida; o layout é o que a ABI manda |
| **ponteiro para classe com só métodos virtuais puros** | O layout da vtable é fixado pela ABI (Itanium C++ ABI no Linux) |
| `const char*` | Só bytes; nenhum dos lados aloca ou libera |

| Perigoso | Por quê |
|---|---|
| `std::string`, `std::vector` **por valor** | O layout interno depende da versão da biblioteca padrão e de flags como `_GLIBCXX_USE_CXX11_ABI`. Dois lados diferentes = leitura de campos errados |
| **exceções atravessando** | Funciona com o mesmo compilador e runtime de unwinding; entre compiladores diferentes, não |
| **objeto alocado de um lado, liberado do outro** | O caso do deleter, acima |
| **membros de dado na interface** | Acrescentar um campo muda o `sizeof` e todos os offsets; a próxima seção trata disso |

A regra prática: quanto mais "C" for a fronteira, mais robusta ela é. Perde-se conforto e
ganha-se compatibilidade — foi a troca que o VST, o LADSPA e as APIs de driver fizeram.

### ⚠ Versionar a interface

Suponha que amanhã `IController` ganhe um método:

```cpp
class IController {
public:
    virtual ~IController() = default;
    virtual std::string name() const = 0;
    virtual void reset() = 0;
    virtual Action compute(const State&) = 0;
    virtual double confidence() const = 0;   // ← novo
};
```

O host recompila e passa a chamar `confidence()`, que ele espera no **slot 4** da vtable. Um
`pid.so` antigo tem uma vtable com quatro entradas: o slot 4 é lixo. O `.so` carrega
normalmente, `dlsym` acha os três símbolos, `create_controller` funciona — e o programa salta
para um endereço arbitrário na primeira chamada.

Nada disso é detectado por `dlopen`. **A ABI quebrou sem que a API tivesse quebrado.**

A defesa padrão é o plugin exportar sua versão, e o registry recusar o que não bate:

```cpp
// no icontroller.hpp, ao lado da interface
inline constexpr int PLUGIN_ABI_VERSION = 1;   // ← incremente ao mexer na interface

#define REGISTER_CONTROLLER(ClassName, name_str)                    \
    extern "C" __attribute__((visibility("default")))               \
    int plugin_abi_version() { return PLUGIN_ABI_VERSION; }         \
    /* ... as outras três funções ... */

// no load_plugin, antes de aceitar o plugin
auto fn_abi = reinterpret_cast<int(*)()>(dlsym(handle, "plugin_abi_version"));
if (!fn_abi || fn_abi() != PLUGIN_ABI_VERSION) {
    dlclose(handle);
    throw std::runtime_error("ABI incompatível em '" + path + "'");
}
```

O plugin antigo passa a ser **recusado com mensagem clara** em vez de derrubar o programa.
Este projeto **não** implementa isso — é o exercício mais valioso do subprojeto.

Duas regras que evitam o problema na origem:

1. **acrescente métodos virtuais só no fim** da interface, nunca no meio (mover um slot
   invalida todos os seguintes) — e mesmo assim, incremente a versão;
2. quando a mudança for grande, **crie `IController2`** e mantenha as duas, como faz a API COM
   do Windows há trinta anos.

---

## Onde os plugins são procurados

Um programa que carrega plugins não pode depender do diretório de trabalho: o mesmo binário
roda de `dist/bin/` e de dentro do build dir. `main.cpp` resolve isso relativo ao **próprio
executável**, e não ao CWD:

```cpp
static std::filesystem::path diretorio_do_executavel() {
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);   // Linux
    return ec ? std::filesystem::path{} : exe.parent_path();
}
```

`/proc/self/exe` é um symlink que o kernel mantém apontando para o binário em execução —
já com os symlinks resolvidos. A ordem de busca é:

| # | Candidato | Cobre o caso |
|---|---|---|
| 1 | `argv[1]`, se houver | Você mandou explicitamente |
| 2 | `<dir-do-exe>/../plugins` | Instalado: `dist/bin/` → `dist/plugins/` |
| 3 | `<dir-do-exe>/plugins` | Build dir: `build/src/plugin_registry/plugins/` |
| 4 | `./plugins` | Último recurso, relativo ao CWD |

Se nenhum existir, o programa **lista o que tentou** e sai com código 1 — em vez de estourar
uma exceção:

```console
$ ./dist/bin/plugin_registry /caminho/errado
=== Plugin Registry Demo ===

Varrendo '/caminho/errado'...
erro: Não foi possível abrir o diretório: /caminho/errado
```

---

## O build, comentado

`meson.build` do subprojeto:

```meson
fmt_dep = dependency('fmt')

subdir('plugins')          # ← ANTES do executable: os .so precisam existir
                           #   no build dir para o programa poder ser testado

executable('plugin_registry',
    files('main.cpp'),
    dependencies: [fmt_dep],
    link_args: ['-ldl'],   # ← dlopen/dlsym/dlclose vivem na libdl,
                           #   que no Linux é separada da libc
    include_directories: ['.'],
    install: true,
)
```

`plugins/meson.build`:

```meson
plugin_flags = ['-fvisibility=hidden']   # tudo oculto; só a macro exporta

shared_library('pid',
    files('pid_controller.cpp'),
    include_directories: ['..'],
    name_prefix: '',        # ← sem isto o Meson geraria "libpid.so".
                            #   Com '', o arquivo é "pid.so" — mais limpo
                            #   quando o nome vem de um arquivo de configuração
    cpp_args: plugin_flags,
    install: true,
    install_dir: 'plugins', # ← <prefix>/plugins = dist/plugins/
)
```

> Os arquivos gerados chamam-se **`pid.so` e `lqr.so`** — sem o prefixo `lib`. Um `dlopen`
> recebe um caminho literal, então o prefixo `lib` (que existe para o `-lnome` do linker) não
> tem função aqui.

---

## Os plugins de exemplo

O domínio é controle de voo. Os dois plugins recebem o mesmo `State` e devolvem um `Action`;
o que muda é **como** decidem.

### O que é um PID

Um controlador PID recebe o **erro** — a diferença entre onde você quer estar e onde está —
e produz uma correção somando três termos:

```
erro(t) = alvo − medido(t)

u(t) =  Kp · erro(t)              ← P: reage ao erro AGORA (força bruta)
      + Ki · ∫ erro dt            ← I: acumula o erro passado (mata o desvio residual)
      + Kd · d(erro)/dt           ← D: reage à VELOCIDADE do erro (freia, evita passar do ponto)
```

Em `pid_controller.cpp`, com alvo de 3000 ft e `dt = 1/60 s`:

```cpp
constexpr double Kp = 0.002, Ki = 0.0001, Kd = 0.005;

double error      = target_altitude - s.altitude_ft;
integral_        += error * dt;
double derivative = (error - prev_error_) / dt;
prev_error_       = error;

double elevator = Kp * error + Ki * integral_ + Kd * derivative;
elevator = clamp(elevator, -1.0, 1.0);          // satura no curso do atuador
```

`integral_` e `prev_error_` são **estado interno acumulado** — e é exatamente por isso que
`reset()` existe na interface. Sem ele, o integrador de um episódio contaminaria o seguinte.
O LQR, que não acumula nada, tem um `reset()` vazio: a interface serve aos dois.

### O que é um LQR

*Linear Quadratic Regulator*. Em vez de ajustar ganhos na mão, você descreve o sistema como
um modelo linear, diz **quanto custa** cada erro de estado e cada esforço de atuador (as
matrizes `Q` e `R`), e a matemática — a equação de Riccati — devolve a matriz de ganhos `K`
**ótima** para esse custo. A lei de controle é `u = −K·(x − x_ref)`.

`lqr_controller.cpp` usa `K` pré-calculado, sem resolver Riccati (para não depender de Eigen),
e trabalha sobre três erros de estado ao mesmo tempo:

```cpp
double e_alt   = 3000.0 - s.altitude_ft;
double e_pitch =    0.0 - s.pitch_deg;
double e_speed =   90.0 - s.speed_kts;

double elevator = K_alt * e_alt + K_pitch * e_pitch;   // duas variáveis para um atuador
double throttle = 0.75 + K_speed * e_speed;            // e o LQR ainda mexe no throttle
```

A diferença observável, no `elev` da saída: o PID satura em `+1.000` no primeiro passo, porque
o termo derivativo explode com o erro inicial de 200 ft; o LQR sai em `+0.460` e desce suave,
porque o termo de `pitch` já o freia. Nenhum dos dois é "o certo" — são compromissos
diferentes, e a graça é poder trocar um pelo outro sem recompilar o host.

---

## Como validar

```bash
make build && make install

ls dist/plugins/          # deve listar exatamente:  lqr.so  pid.so
nm -D --defined-only dist/plugins/pid.so | grep ' T '   # os três símbolos exportados

./dist/bin/plugin_registry
```

Compilação avulsa, com warnings e sanitizers:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)

g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include -Isrc/plugin_registry \
    src/plugin_registry/main.cpp -o /tmp/pr -L$FMTP/lib -lfmtd -ldl

g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include -Isrc/plugin_registry \
    src/plugin_registry/main.cpp -o /tmp/prsan -L$FMTP/lib -lfmtd -ldl \
    && /tmp/prsan dist/plugins
```

Estado atual: compila **sem warnings** com `-Wall -Wextra -Wpedantic` e roda com **0 achados
sob ASan+UBSan**. Exige `-std=c++20` — `main.cpp` e `plugin_registry.hpp` usam
*designated initializers* (`.altitude_ft = 2800.0`), que em C++17 são só uma extensão do GCC.

### A saída, de verdade

```console
$ ./dist/bin/plugin_registry
=== Plugin Registry Demo ===

Varrendo '/home/lima/Projects/cpp_academy/dist/plugins'...
[registry] plugin 'pid' carregado de /home/lima/Projects/cpp_academy/dist/plugins/pid.so
[registry] plugin 'lqr' carregado de /home/lima/Projects/cpp_academy/dist/plugins/lqr.so
2 plugin(s) carregado(s).

Plugins disponíveis:
  - lqr
  - pid

=== Controlador: lqr ===
  step 01   alt= 2800.0 ft  spd= 85.0 kts  pitch= -2.0°  | elev=+0.460  thr=0.650
  step 02   alt= 2799.8 ft  spd= 85.0 kts  pitch= -1.1°  | elev=+0.387  thr=0.649
  step 03   alt= 2799.8 ft  spd= 84.9 kts  pitch= -0.3°  | elev=+0.325  thr=0.648
  ...
  step 10   alt= 2801.7 ft  spd= 84.5 kts  pitch=  2.5°  | elev=+0.094  thr=0.641

=== Controlador: pid ===
  step 01   alt= 2800.0 ft  spd= 85.0 kts  pitch= -2.0°  | elev=+1.000  thr=0.750
  step 02   alt= 2800.0 ft  spd= 85.0 kts  pitch=  0.0°  | elev=+0.401  thr=0.750
  ...
  step 10   alt= 2804.0 ft  spd= 85.0 kts  pitch=  4.5°  | elev=+0.161  thr=0.750

=== Troca de controlador em runtime ===
(mesma interface, implementação diferente, zero recompilação)

  [pid]   alt= 2900.0 ft  spd= 88.0 kts  pitch=  0.0°  | elev=+1.000  thr=0.750
  [lqr]   alt= 2900.3 ft  spd= 88.0 kts  pitch=  2.0°  | elev=-0.011  thr=0.710
  [pid]   alt= 2900.7 ft  spd= 88.0 kts  pitch=  2.0°  | elev=+1.000  thr=0.750

Fim.
```

Dois detalhes honestos sobre essa saída:

- **a ordem em "Plugins disponíveis" não é garantida.** `available()` percorre um
  `std::unordered_map`; aqui saiu `lqr` antes de `pid`, mas isso depende da implementação de
  hash e pode mudar. Se a ordem importar para você, ordene explicitamente;
- **o bloco final é o ponto do subprojeto**: o mesmo estado passa por `pid`, depois `lqr`,
  depois `pid` de novo, e cada um responde à sua maneira. Nenhuma recompilação, nenhum
  `switch`, nenhum `if` sobre o nome do algoritmo.

### Experimentos que ensinam mais que ler

```bash
# 1. Remova um plugin e veja o host se adaptar
mv dist/plugins/lqr.so /tmp/ && ./dist/bin/plugin_registry
#    → "1 plugin(s) carregado(s)" e "[aviso] 'lqr' não disponível, pulando."
mv /tmp/lqr.so dist/plugins/

# 2. Ponha um .so que não é plugin e veja a falha ficar isolada
cp /lib/x86_64-linux-gnu/libm.so.6 dist/plugins/naoeplugin.so
./dist/bin/plugin_registry     # → aviso "plugin_name não encontrado", os outros carregam
rm dist/plugins/naoeplugin.so

# 3. Diretório vazio: erro claro, sem crash
mkdir -p /tmp/vazio && ./dist/bin/plugin_registry /tmp/vazio
```

---

## Referências

Dentro deste repositório:

- [`../linkage/`](../linkage/) — ligação interna/externa, `extern "C"`, ODR
- [`../type_erasure/`](../type_erasure/) — a alternativa a interfaces virtuais para o mesmo
  problema, quando não há fronteira de `.so`
- [`../oop/`](../oop/) — vtable, despacho virtual e o custo de cada um
- [`../layout/`](../layout/) — `sizeof`, padding e por que acrescentar um campo quebra a ABI
- [`../embed-python/`](../embed-python/) — outra forma de estender um programa em runtime
- [`../undefined_behavior/`](../undefined_behavior/) — os sanitizers usados acima

Fora dele:

- `man 3 dlopen` e `man 3 dlsym` — a documentação canônica, curta e completa
- Ulrich Drepper, *How To Write Shared Libraries* — o texto de referência sobre `.so`,
  visibilidade, PLT/GOT e custo de carregamento: <https://www.akkadia.org/drepper/dsohowto.pdf>
- GCC, *Visibility* — o wiki oficial sobre `-fvisibility` e o atributo:
  <https://gcc.gnu.org/wiki/Visibility>
- *Itanium C++ ABI* — a especificação de mangling e de layout de vtable usada por GCC e Clang
  no Linux: <https://itanium-cxx-abi.github.io/cxx-abi/abi.html>
- Microsoft, *LoadLibrary / GetProcAddress* — o equivalente Windows, para quem for portar:
  <https://learn.microsoft.com/windows/win32/dlls/using-run-time-dynamic-linking>
