# aws/s3 — o AWS SDK for C++ do zero, com Amazon S3

## O que é este projeto?

Quatro demos que usam o **AWS SDK for C++** para guardar e ler bytes no **Amazon S3**:
listar buckets, gravar um objeto, ler o mesmo objeto de volta e apagá-lo — sempre com o
mesmo esqueleto `InitAPI → escopo → ShutdownAPI`.

```bash
make build
./dist/bin/aws-s3
```

> ⚠ **O alvo se chama `aws-s3`, com hífen.** O diretório é `src/aws/s3/`, mas
> `meson.build` declara `executable('aws-s3', ...)`. `aws_s3` (com sublinhado) **não
> existe** — nem como alvo, nem como binário. Confira você mesmo:
>
> ```bash
> grep -n "executable(" src/aws/s3/meson.build     # → executable('aws-s3',
> ls dist/bin | grep aws                           # → aws-s3
> ```

Reconstruir só este alvo, sem passar pelo `make` inteiro:

```bash
source build/conanbuild.sh
meson compile -C build aws-s3
```

**Você não precisa de conta na AWS para rodar isto.** O programa funciona em três modos, e
os três são resultados legítimos:

| Modo | O que acontece |
|---|---|
| Sem credencial nenhuma | Demo 1 falha e **imprime o erro explicado**; demos 2–4 são puladas |
| Com um S3 local (MinIO/LocalStack) | as quatro demos rodam de verdade, de graça |
| Com credencial AWS real | as quatro demos rodam contra a AWS (e **custam dinheiro**) |

A seção [Sem conta AWS](#sem-conta-aws-suba-um-s3-local-em-30-segundos) mostra o modo do
meio, que é o recomendado para estudar.

---

## Antes de começar: os conceitos

Esta seção define **todo** termo que o resto do README usa. Se você nunca mexeu com nuvem,
leia daqui até o fim antes de rodar qualquer coisa. Não pressupomos nada além de C++ básico.

### 1. O que é o S3, em um desenho

O **S3** (*Simple Storage Service*) é um disco de rede da Amazon com uma API HTTP. Você não
monta um sistema de arquivos: você faz `PUT` e `GET` em URLs. O vocabulário tem exatamente
quatro palavras.

```
   região  us-east-1                    ← ONDE os bytes moram fisicamente
   └── bucket  "academy-bucket"          ← contêiner; o nome é único no MUNDO
       ├── key  "cpp_academy/demo.txt"   ← o nome completo do objeto
       │   └── object  19 bytes + metadados (Content-Type, ETag, ...)
       └── key  "logs/2026/08/27.log"
           └── object  4 KiB
```

| Termo | Definição |
|---|---|
| **bucket** | o contêiner. O nome é **globalmente único**: se alguém no planeta já registrou `teste`, você não pode criar outro `teste`. Por isso os nomes reais parecem `minha-empresa-logs-prod`. |
| **object** | o arquivo em si: uma sequência de bytes + metadados. Tamanho de 0 a 5 TiB. |
| **key** | o nome do objeto dentro do bucket. É uma **string**, não um caminho. |
| **region** | a região geográfica onde o bucket vive (`us-east-1`, `sa-east-1`, ...). Faz parte do endereço e da assinatura da requisição. |
| **endpoint** | o host HTTP com que o SDK conversa. Normalmente `s3.amazonaws.com`; aqui podemos trocá-lo por um servidor local. |

> ⚠ **Não existem diretórios no S3.** A key `cpp_academy/demo.txt` é uma string única que
> por acaso contém uma barra. Ninguém criou a "pasta" `cpp_academy/`. O que existe é a
> operação *ListObjects com prefixo*: peça todas as keys que começam com `cpp_academy/` e o
> console desenha uma pastinha para você. É pura ilusão de interface. A consequência
> prática: renomear uma "pasta" com um milhão de objetos custa um milhão de cópias.

### 2. As duas formas de endereçar um bucket

O mesmo objeto tem dois formatos de URL, e a diferença importa na hora de usar um servidor
local:

```
virtual-hosted (padrão da AWS):
    https://academy-bucket.s3.us-east-1.amazonaws.com/cpp_academy/demo.txt
           └──── bucket vira SUBDOMÍNIO ────┘

path-style (o que MinIO e LocalStack entendem):
    http://localhost:9000/academy-bucket/cpp_academy/demo.txt
                         └── bucket vira o primeiro segmento do CAMINHO ──┘
```

É por isso que `main.cpp` liga `config.useVirtualAddressing = false` quando você aponta para
um endpoint local: `academy-bucket.localhost` não resolveria em DNS nenhum.

### 3. Credenciais: usuário e senha de máquina

Você não digita senha para o S3. Toda requisição vai **assinada** com um par:

| Termo | O que é |
|---|---|
| **access key id** | o "usuário". Uma string pública como `AKIA...`. Viaja no cabeçalho. |
| **secret access key** | a "senha". **Nunca viaja pela rede.** É a chave usada para calcular um HMAC-SHA256 sobre a requisição. |
| **assinatura (SigV4)** | o resumo criptográfico do método, da URL, dos cabeçalhos, do corpo, da data e da região. O servidor recalcula com a sua chave secreta e compara. |

Como a assinatura cobre a data e a região, ela **expira** e **não serve para outra região** —
é por isso que o SDK exige uma região configurada mesmo para uma operação global.

O SDK procura as credenciais nesta ordem (a *credential provider chain*), parando na primeira
que encontrar:

```
1. variáveis de ambiente   AWS_ACCESS_KEY_ID / AWS_SECRET_ACCESS_KEY
2. arquivo                 ~/.aws/credentials  (seção [default] ou AWS_PROFILE)
3. metadados da máquina    role do EC2 / task role do ECS / IRSA no EKS
```

O item 3 merece uma frase, porque é o que se usa em produção: uma máquina rodando na AWS
recebe credenciais **temporárias** de um endereço interno (`169.254.169.254`), que o SDK
renova sozinho. Ninguém guarda chave em servidor de produção.

> ⚠ **Nunca escreva credencial no código, nem no README, nem no `docker-compose.yml` que
> vai para o Git.** Uma chave commitada é varrida por bots em minutos. Se acontecer, não
> basta apagar o commit: é preciso **revogar a chave** no console da AWS. E use sempre a
> credencial de **menor privilégio** possível — um usuário que só pode ler um bucket.

### 4. IAM: ter a chave não é ter permissão

**IAM** (*Identity and Access Management*) é o sistema de permissões da AWS. Cada operação da
API tem um nome de permissão própria, e a credencial precisa dela **explicitamente**:

| Operação do demo | Permissão IAM exigida |
|---|---|
| `ListBuckets` | `s3:ListAllMyBuckets` (é uma permissão de **conta**, não de bucket) |
| `PutObject` | `s3:PutObject` no recurso `arn:aws:s3:::bucket/key` |
| `GetObject` | `s3:GetObject` |
| `DeleteObject` | `s3:DeleteObject` |

Consequência que confunde todo iniciante: **credencial válida + permissão ausente = erro
`AccessDenied`**, não erro de autenticação. O programa está certo, a chave está certa, e
mesmo assim falha. A Demo 1 falhando com `AccessDenied` quase sempre é isto, e não um bug.

### 5. O que `InitAPI` e `ShutdownAPI` fazem

O AWS SDK for C++ mantém **estado global de processo**. `Aws::InitAPI(options)` liga:

- o **alocador** interno do SDK (`Aws::MakeShared`, `Aws::String` e afins usam-no);
- o **subsistema de log**;
- o **cliente HTTP** (libcurl no Linux), com o pool de conexões;
- a **biblioteca de criptografia** usada para assinar as requisições (OpenSSL).

`Aws::ShutdownAPI(options)` desliga tudo isso. Entre os dois, e **somente** entre os dois,
você pode criar clientes e fazer chamadas.

```
tempo ─────────────────────────────────────────────────────────────────▶

  Aws::InitAPI(options)
  ├─ alocador, log, curl, OpenSSL de pé
  │
  │   {                                  ← o escopo existe por causa disto
  │       S3Client client(config);       construtor: pega curl e credenciais
  │       client.ListBuckets();
  │   }                                  ← ~S3Client() roda AQUI, com curl vivo
  │
  Aws::ShutdownAPI(options)
  └─ curl e OpenSSL desligados
```

> ⚠ **Se o `S3Client` viver além do `ShutdownAPI`, o destrutor toca recursos globais já
> liberados** — devolve conexão para um pool que não existe mais. O sintoma é um crash *no
> fim* do programa, depois de tudo ter dado certo, e por isso é difícil de ler. As chaves
> `{ }` são a correção: RAII amarra a destruição ao fim do bloco. É o mesmo raciocínio de
> ordem de destruição de [`src/smart-pointers/`](../smart-pointers/README.md) e
> [`src/memory/`](../memory/README.md).

### 6. `Outcome<R, E>`: erro que é valor, não exceção

O SDK **não lança exceções** para erros de serviço. Toda operação devolve um
`Aws::Utils::Outcome<Result, Error>`, que é uma caixa com **ou** o resultado **ou** o erro —
nunca os dois:

```cpp
auto outcome = client.ListBuckets();

outcome.IsSuccess()                  // bool: qual dos dois está lá dentro
outcome.GetResult()                  // só é válido se IsSuccess() == true
outcome.GetError()                   // só é válido se IsSuccess() == false
outcome.GetError().GetErrorType()    // enum → é ISTO que se usa para decidir
outcome.GetError().GetExceptionName()// string curta: "NoSuchBucket"
outcome.GetError().GetMessage()      // texto livre para humano
```

Se `std::expected` for novidade para você: é o tipo do C++23 que representa exatamente essa
ideia — `std::expected<T, E>` guarda um `T` quando deu certo e um `E` quando deu errado, e o
chamador é **obrigado** a olhar qual dos dois veio antes de usar o valor. `Outcome<R,E>` é a
mesma ideia, escrita à mão anos antes de o padrão a ter. O `Result<T, E>` do Rust é o mesmo
conceito. Há um demo dedicado em
[`src/errors/demo_expected.hpp`](../errors/demo_expected.hpp).

| | exceção (`try/catch`) | `Outcome` / `expected` |
|---|---|---|
| Onde o erro aparece | em qualquer ponto, invisível na assinatura | no tipo de retorno, visível |
| Custo quando dá certo | ~zero | um branch |
| Custo quando dá errado | desenrolar a pilha (caro) | um branch |
| Dá para ignorar? | não (propaga) | **sim** — e é o risco desta abordagem |
| Atravessa fronteira de ABI/DLL? | mal | bem |

> ⚠ **Chamar `GetResult()` sem checar `IsSuccess()` é o bug clássico do SDK.** Não há
> exceção: você lê um objeto vazio e segue adiante com dados falsos. Todo demo deste
> subprojeto começa pelo `if (!outcome.IsSuccess())`.

### 7. `Aws::String`, `Aws::MakeShared` — por que não `std::`

Você vai ver tipos `Aws::` no lugar dos padrões. Não é gosto: são os mesmos tipos da
biblioteca padrão **parametrizados com o alocador do SDK**, para que toda a memória do SDK
passe pelo alocador que o `InitAPI` configurou (útil para instrumentar consumo ou usar memória
customizada). `Aws::MakeShared<T>("tag")` é `std::make_shared<T>()` com uma etiqueta de
diagnóstico. Por isso o código chama `.c_str()` ao passar essas strings para o `fmt`.

---

## Por que estudar isto?

Um programa que só grava em disco local morre com a máquina, não é lido por outro processo
noutro servidor e não escala além do disco. É o problema que armazenamento de objetos
resolve.

| Sem um serviço de objetos | Com o S3 |
|---|---|
| o arquivo vive no disco de **uma** máquina | vive num serviço replicado entre datacenters |
| escalar = comprar disco maior | ilimitado na prática; paga-se por GB armazenado |
| compartilhar entre serviços = NFS, rsync, dor | uma URL e uma permissão |
| backup é problema seu | versionamento e replicação são configuração |
| a máquina morreu, os dados morreram | a máquina é descartável |

Mas o motivo mais forte é outro: **o que você aprende aqui é o formato de todo SDK de nuvem.**
Os quatro padrões abaixo se repetem, idênticos, em DynamoDB, SQS, Lambda, SNS — e, com outros
nomes, no SDK do Google Cloud e do Azure:

1. **estado global** inicializado uma vez por processo (`InitAPI`/`ShutdownAPI`);
2. **um cliente por serviço**, construído a partir de um objeto de configuração;
3. **um objeto de *request* por operação**, preenchido com *setters*;
4. **erro como valor de retorno** (`Outcome`), nunca exceção.

Trocar `S3Client`/`PutObjectRequest` por `SQSClient`/`SendMessageRequest` é mudar os nomes; a
forma do código continua a mesma.

---

## Estrutura de arquivos

```
src/aws/
├── README.md            ← este arquivo
└── s3/
    ├── main.cpp         ← as 4 demos + make_config() + print_error()
    └── meson.build      ← executable('aws-s3', ...) — hífen, não sublinhado
```

> ⚠ O diretório `src/aws/` **não é um alvo de build**. Quem entra na compilação é
> `src/aws/s3/`, via `subdir('aws/s3')` no [`src/meson.build`](../meson.build) (linha 107).
> Este README fica um nível acima porque documenta o assunto "AWS", não só o S3.

---

## Sem conta AWS? Suba um S3 local em 30 segundos

**MinIO** é um servidor de armazenamento de objetos que fala o mesmo protocolo do S3. O SDK
não sabe a diferença: basta trocar o endpoint. Zero custo, zero cadastro, e você pode apagar
tudo com `docker rm`.

```bash
docker run -d --name academy-minio \
    -p 9000:9000 \
    -e MINIO_ROOT_USER=academykey \
    -e MINIO_ROOT_PASSWORD=academysecret \
    minio/minio:latest server /data

# criar o bucket que as demos 2-4 usam (mc é o cliente CLI, já vem na imagem)
docker exec academy-minio sh -c \
  'mc alias set local http://localhost:9000 academykey academysecret &&
   mc mb --ignore-existing local/academy-bucket'
```

Agora aponte o programa para ele:

```bash
export AWS_ACCESS_KEY_ID=academykey
export AWS_SECRET_ACCESS_KEY=academysecret
export AWS_ENDPOINT_URL=http://localhost:9000
export AWS_REGION=us-east-1
export ACADEMY_S3_BUCKET=academy-bucket

./dist/bin/aws-s3
```

**LocalStack** (`localstack/localstack`, porta 4566) serve igualmente bem e emula dezenas de
outros serviços da AWS; só troque a porta em `AWS_ENDPOINT_URL`.

> ⚠ As chaves acima são **de brinquedo**, valem só para o contêiner local e podem aparecer
> num README. Uma credencial de verdade nunca.

---

## O esqueleto comum às quatro demos

Todas as demos são o mesmo bloco, e vale decorá-lo:

```cpp
Aws::SDKOptions options;
Aws::InitAPI(options);                      // (1) estado global de pé
{
    const auto config = make_config();      // (2) região + endpoint, explícitos
    Aws::S3::S3Client client(config);       // (3) um cliente por serviço

    Aws::S3::Model::PutObjectRequest req;   // (4) um request por operação
    req.SetBucket("meu-bucket");
    req.SetKey("cpp_academy/demo.txt");

    auto outcome = client.PutObject(req);   // (5) chamada HTTP síncrona
    if (!outcome.IsSuccess()) {             // (6) erro é VALOR, sempre checado
        print_error("PutObject", outcome.GetError());
    }
}                                           // (7) ~S3Client aqui, ainda dentro do SDK
Aws::ShutdownAPI(options);                  // (8) por último, sempre
```

### A configuração explícita, e por que não o construtor padrão

O README antigo mostrava `Aws::S3::S3Client client;` — construtor padrão, sem argumentos.
Funciona **às vezes**: dependendo da versão do SDK ele é deprecado, e sem `AWS_REGION` no
ambiente ele não sabe montar o host nem assinar. O código atual monta a configuração à mão:

```cpp
Aws::S3::S3ClientConfiguration make_config() {
    Aws::S3::S3ClientConfiguration config;

    std::string region = env("AWS_REGION");
    if (region.empty()) region = env("AWS_DEFAULT_REGION");
    if (region.empty()) region = "us-east-1";     // padrão explícito, nunca implícito
    config.region = region.c_str();

    const std::string endpoint = env("AWS_ENDPOINT_URL");
    if (!endpoint.empty()) {
        config.endpointOverride     = endpoint.c_str();  // MinIO / LocalStack
        config.useVirtualAddressing = false;             // path-style (ver seção 2)
    }
    return config;
}
```

Três decisões dignas de nota: a região tem **padrão explícito** em vez de estourar; o endpoint
só é sobrescrito quando a variável existe (então o caminho da AWS real continua intacto); e
`useVirtualAddressing = false` acompanha o endpoint local **sempre junto**, porque um sem o
outro produz um erro de DNS incompreensível.

> ⚠ Note o que `make_config()` **não** faz: ler `AWS_ACCESS_KEY_ID`. Credencial é assunto da
> *provider chain* do SDK, e o programa não toca nela. Isso é proposital — código que lê
> segredo é código que um dia imprime segredo em log.

---

## Os demos, um a um

As demos 2, 3 e 4 **escrevem** no S3, então só rodam quando `ACADEMY_S3_BUCKET` nomeia um
bucket seu. Nenhum programa deste repositório cria ou apaga dado remoto sem que você peça.

### Demo 1 · `ListBuckets` — a chamada mais simples que existe

O problema: antes de qualquer coisa, você quer saber se **a credencial funciona**.
`ListBuckets` é o teste ideal — não tem request, não tem corpo, não altera nada.

```cpp
auto outcome = client.ListBuckets();                 // sem argumentos!
if (!outcome.IsSuccess()) { print_error("ListBuckets", outcome.GetError()); return; }

for (const auto& b : outcome.GetResult().GetBuckets())
    fmt::print("    bucket: {}\n", b.GetName().c_str());
```

Saída medida contra o MinIO local:

```
[config] endpoint local: http://localhost:9000 (path-style)
[config] region: us-east-1

=== Demo 1 · ListBuckets ===
  ✓ 1 bucket(s)
    bucket: academy-bucket
```

> ⚠ `ListBuckets` exige `s3:ListAllMyBuckets`, que é permissão **da conta**. Uma credencial
> restrita a um único bucket — o que é a boa prática — falha aqui com `AccessDenied` e
> funciona perfeitamente nas demos 2 a 4. Falhar na Demo 1 não significa credencial inválida.

### Demo 2 · `PutObject` — gravar bytes

O problema: mandar um conteúdo qualquer para uma key. O detalhe que surpreende é o corpo.

```cpp
Aws::S3::Model::PutObjectRequest req;
req.SetBucket(bucket.c_str());
req.SetKey(key.c_str());

auto body = Aws::MakeShared<Aws::StringStream>("academy-s3");  // shared_ptr<iostream>
*body << conteudo;
req.SetBody(body);
req.SetContentType("text/plain");

auto outcome = client.PutObject(req);
```

**Por que `shared_ptr<std::iostream>` e não `std::string`?** Porque o SDK pode precisar
**reler o corpo**: para calcular o checksum, para reassinar depois de um *redirect*, ou para
repetir a requisição num *retry*. Um stream posicionável permite `seekg(0)`; uma string por
valor obrigaria a copiar. O `shared_ptr` existe porque o corpo pode sobreviver à sua chamada
quando o envio é assíncrono. Nada de `new` cru: `Aws::MakeShared` é o `make_shared` do SDK —
o mesmo raciocínio de propriedade de [`src/smart-pointers/`](../smart-pointers/README.md).

Saída medida:

```
=== Demo 2 · PutObject ===
  ✓ s3://academy-bucket/cpp_academy/demo.txt gravado (19 bytes)
```

> ⚠ `SetContentType` não muda um byte do que é gravado, mas fica gravado **como metadado** e
> é o que o navegador lê ao baixar o objeto. Esquecer produz `binary/octet-stream`, e o
> browser oferece download em vez de exibir.

### Demo 3 · `GetObject` — ler de volta e conferir

O problema: gravou mesmo? A única prova honesta é ler e comparar byte a byte.

```cpp
auto outcome = client.GetObject(req);
if (!outcome.IsSuccess()) { ... }

auto result = outcome.GetResultWithOwnership();   // move o resultado para cá
std::ostringstream buffer;
buffer << result.GetBody().rdbuf();               // drena o stream inteiro
const std::string lido = buffer.str();
```

Saída medida:

```
=== Demo 3 · GetObject ===
  ✓ leu 19 bytes: "ola do cpp_academy
"
  ✓ round-trip identico
```

> ⚠ **`GetResultWithOwnership()`, e não `GetResult()`.** O corpo é um stream vivo ligado à
> conexão; `GetResult()` devolve uma referência ao resultado ainda dentro do `Outcome`, e o
> stream morre junto com ele. A versão *WithOwnership* **move** o resultado, e é a única
> forma correta de segurar o corpo. É o mesmo problema de tempo de vida de
> [`src/value_semantics/`](../value_semantics/README.md), aplicado a um stream de rede.
>
> ⚠ E o stream é **consumível uma vez só**: depois de `rdbuf()` drenado, ele está vazio. Ler
> duas vezes exige guardar a string, não reler o stream.

### Demo 4 · `DeleteObject` — limpar o que você criou

O problema: um demo que deixa lixo no bucket é um demo que custa dinheiro todo mês.

```cpp
Aws::S3::Model::DeleteObjectRequest req;
req.SetBucket(bucket.c_str());
req.SetKey(key.c_str());
auto outcome = client.DeleteObject(req);
```

Saída medida:

```
=== Demo 4 · DeleteObject ===
  ✓ s3://academy-bucket/cpp_academy/demo.txt removido
  (o S3 responde sucesso mesmo se a key ja nao existia)
```

> ⚠ **Apagar no S3 é idempotente.** Apagar uma key inexistente responde `204 No Content`, ou
> seja, **sucesso**. Não dá para usar `DeleteObject` como teste de existência — para isso
> existe `HeadObject`. Idempotente quer dizer: fazer a operação uma vez ou dez vezes tem o
> mesmo efeito final. É uma propriedade desejada em API de rede, porque um *retry* depois de
> um timeout não pode causar dano.

---

## Erros: como ler o `Outcome` direito

`print_error()` imprime três coisas, e só uma delas serve para tomar decisão:

```cpp
fmt::print("    tipo    : {}\n", static_cast<int>(e.GetErrorType()));  // enum → DECIDIR
fmt::print("    codigo  : {}\n", e.GetExceptionName().c_str());        // string curta
fmt::print("    mensagem: {}\n", e.GetMessage().c_str());              // humano

switch (e.GetErrorType()) {
    case Aws::S3::S3Errors::ACCESS_DENIED:   /* credencial ok, falta permissão IAM */
    case Aws::S3::S3Errors::NO_SUCH_BUCKET:  /* bucket não existe nesta conta/região */
    case Aws::S3::S3Errors::NO_SUCH_KEY:     /* bucket existe, key não */
    default:                                 /* rede, credencial ausente, erro do serviço */
}
```

> ⚠ **Nunca ramifique pela mensagem.** `GetMessage()` é texto livre, muda entre versões do
> SDK e entre implementações (a do MinIO não é igual à da AWS). `GetErrorType()` é um enum
> estável. O número que o programa imprime é só o valor numérico desse enum e **não** é
> contrato — serve para você comparar duas execuções, não para gravar em `if`.

Três cenários reais, medidos nesta máquina:

**Sem credencial e sem alcançar a AWS** (o cenário do leitor que só rodou `./dist/bin/aws-s3`):

```
=== Demo 1 · ListBuckets ===
  ✗ ListBuckets falhou
    tipo    : 100
    codigo  : Xml Parse Error
    mensagem: XML_ERROR_PARSING_ATTRIBUTE
    → sem credencial valida, sem rede, ou erro do servico
```

Traduzindo: o SDK mandou a requisição, recebeu uma resposta que **não era o XML do S3** e
desistiu. Numa rede aberta e com chave errada, o esperado seria `InvalidAccessKeyId`; sem
chave nenhuma, `AccessDenied` ou uma falha de resolução de credencial. **Qualquer um dos três
é uma execução bem-sucedida do demo** — o programa retorna 0 e as demos 2-4 são puladas.

**Endpoint local apontado para uma porta sem ninguém:**

```
    tipo    : 99
    codigo  :
    mensagem: curlCode: 7, Couldn't connect to server; Details: Failed to connect to
              localhost port 4566 after 0 ms: Couldn't connect to server
```

Repare que `codigo` vem **vazio**: não houve resposta HTTP nenhuma, logo não há código de erro
do S3. Erro de transporte e erro de serviço são coisas diferentes.

**Bucket inexistente** (`ACADEMY_S3_BUCKET=bucket-que-nao-existe` contra o MinIO):

```
=== Demo 2 · PutObject ===
  ✗ PutObject falhou
    tipo    : 136
    codigo  : NoSuchBucket
    mensagem: The specified bucket does not exist
    → o bucket nao existe nesta conta/regiao
```

Este é o formato ideal: tipo, código e mensagem, os três presentes e concordando.

Comparação direta com exceções, e o motivo de o SDK ter escolhido assim, está em
[`src/errors/`](../errors/README.md).

---

## ⚠ O bug histórico deste subprojeto

O `main.cpp` abre com um aviso, e ele não é decorativo:

> faltava o `#include <fmt/core.h>`, e o alvo estava **desabilitado** no `src/meson.build`,
> então o erro nunca aparecia.

O código chamava `fmt::print` sem incluir o `fmt`. Isso não compila — `error: 'fmt' has not
been declared`. Só que o `subdir('aws/s3')` estava comentado, ninguém compilava o arquivo, e o
erro ficou **latente** por meses. O README antigo chegava a reproduzir o trecho quebrado como
se fosse o código de referência.

A lição não é sobre includes: **alvo que não compila é código que não existe.** Um `subdir()`
comentado é dívida invisível. A mesma classe de bug apareceu em `src/solid/srp` e
`src/concepts`, e todos estão catalogados em [`src/ROADMAP.md`](../ROADMAP.md).

---

## Segurança e custo

- **A AWS cobra.** Armazenamento por GB/mês, requisições por milhar e transferência de saída
  por GB. Um demo custa frações de centavo, mas um bucket esquecido com terabytes, não. Por
  isso a Demo 4 apaga o que a Demo 2 criou.
- **As demos de escrita são opt-in.** Sem `ACADEMY_S3_BUCKET`, o programa nem tenta.
- **Menor privilégio:** crie um usuário IAM com uma política restrita a
  `arn:aws:s3:::seu-bucket/*` e às quatro ações usadas aqui. Não use a credencial raiz da
  conta — nunca, para nada.
- **Nada de credencial no Git.** Adicione `.env` ao `.gitignore`, exporte no shell ou use
  `aws configure` (que escreve em `~/.aws/credentials`, fora do repositório).
- **Prefira o MinIO local** enquanto estuda. Você aprende exatamente as mesmas chamadas com
  risco zero.

---

## Como validar

```bash
make build
ls dist/bin/aws-s3            # o binário existe, com hífen
```

**1. Sem nenhuma configuração** — deve terminar com código 0, imprimindo o erro explicado da
Demo 1 e pulando as demais:

```bash
./dist/bin/aws-s3; echo "exit=$?"
```

```
[config] region: us-east-1

=== Demo 1 · ListBuckets ===
  ✗ ListBuckets falhou
    ...
=== Demos 2-4 puladas ===
  Elas ESCREVEM no S3. Defina ACADEMY_S3_BUCKET com um bucket
  seu para habilita-las:
      export ACADEMY_S3_BUCKET=meu-bucket-de-testes
exit=0
```

**2. Contra o MinIO local** — as quatro demos, o caminho completo. Saída medida, integral:

```
[config] endpoint local: http://localhost:9000 (path-style)
[config] region: us-east-1

=== Demo 1 · ListBuckets ===
  ✓ 1 bucket(s)
    bucket: academy-bucket

=== Demo 2 · PutObject ===
  ✓ s3://academy-bucket/cpp_academy/demo.txt gravado (19 bytes)

=== Demo 3 · GetObject ===
  ✓ leu 19 bytes: "ola do cpp_academy
"
  ✓ round-trip identico

=== Demo 4 · DeleteObject ===
  ✓ s3://academy-bucket/cpp_academy/demo.txt removido
  (o S3 responde sucesso mesmo se a key ja nao existia)
```

**3. Conferindo por fora**, sem confiar na saída do próprio programa:

```bash
# durante uma pausa entre Put e Delete, ou comentando a Demo 4:
docker exec academy-minio sh -c \
  'mc alias set local http://localhost:9000 academykey academysecret &&
   mc ls --recursive local/academy-bucket'
```

**4. Limpando tudo:**

```bash
docker rm -f academy-minio
```

---

## Referências

Dentro deste repositório:

- [`src/errors/`](../errors/README.md) — exceções, `error_code` e `std::expected`: o
  vocabulário completo por trás do `Outcome<R,E>`
- [`src/smart-pointers/`](../smart-pointers/README.md) — `shared_ptr` e propriedade, que é o
  que `Aws::MakeShared` faz no corpo do `PutObject`
- [`src/memory/`](../memory/README.md) — alocadores, o motivo de existirem `Aws::String` e
  `Aws::Allocator`
- [`src/http-client/`](../http-client/README.md) — HTTP sem SDK, um nível abaixo
- [`src/network/`](../network/README.md) — TCP e o `io_context` do Asio, dois níveis abaixo
- [`src/ROADMAP.md`](../ROADMAP.md) — o catálogo dos alvos latentes, incluindo este
- [`s3/main.cpp`](s3/main.cpp) — o código comentado das quatro demos

Fora dele:

- *AWS SDK for C++ Developer Guide* — https://docs.aws.amazon.com/sdk-for-cpp/
- *Amazon S3 API Reference* — https://docs.aws.amazon.com/AmazonS3/latest/API/
- *Signature Version 4 signing process* —
  https://docs.aws.amazon.com/general/latest/gr/signature-version-4.html
- MinIO — https://min.io/docs/minio/linux/index.html
- LocalStack — https://docs.localstack.cloud/
