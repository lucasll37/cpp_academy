# aws/s3 — Integração com AWS SDK for C++

## O que é este projeto?

Demonstra como usar o **AWS SDK for C++** para interagir com o Amazon S3 (Simple Storage Service), o serviço de armazenamento em nuvem da AWS. O foco está na inicialização correta do SDK, autenticação automática via credenciais do sistema e operações básicas de listagem de buckets.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| AWS SDK lifecycle | `InitAPI` / `ShutdownAPI` que DEVE envolver todas as chamadas |
| `Aws::S3::S3Client` | Cliente de alto nível para o serviço S3 |
| Modelo `Outcome<Result, Error>` | Padrão de retorno sem exceções do SDK |
| Autenticação via ambiente | Credenciais lidas de `~/.aws/credentials` ou variáveis de ambiente |
| RAII com escopo | O cliente S3 é destruído antes de `ShutdownAPI` |

---

## Estrutura de arquivos

```
aws/
└── s3/
    ├── main.cpp      ← ponto de entrada
    └── meson.build   ← configuração de build
```

---

## O código explicado

```cpp
#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListBucketsRequest.h>

int main() {
    Aws::SDKOptions options;
    Aws::InitAPI(options);   // (1) SEMPRE primeiro
    {
        Aws::S3::S3Client client; // (2) cliente com credenciais automáticas

        auto outcome = client.ListBuckets(); // (3) faz a chamada HTTP à AWS

        if (outcome.IsSuccess()) {           // (4) verifica sucesso
            for (auto& bucket : outcome.GetResult().GetBuckets())
                fmt::print("bucket: {}\n", bucket.GetName().c_str());
        } else {
            fmt::print("erro: {}\n",
                outcome.GetError().GetMessage().c_str()); // (5) mensagem de erro
        }
    } // (6) client destruído ANTES do ShutdownAPI
    Aws::ShutdownAPI(options); // (7) SEMPRE por último
}
```

### Por que o `{ }` ao redor do client?

O AWS SDK usa recursos globais (thread pool, HTTP, logs). Se o `S3Client` for destruído **depois** de `ShutdownAPI`, os destructors acessam memória inválida e causam crash. O bloco garante a ordem correta de destruição via RAII.

---

## O modelo `Outcome<R, E>`

O SDK não usa exceções por padrão. Em vez disso, toda operação retorna um `Aws::Utils::Outcome<ResultType, ErrorType>`:

```
outcome.IsSuccess()           → bool
outcome.GetResult()           → resultado tipado (ListBucketsResult)
outcome.GetError()            → AWSError com código e mensagem
outcome.GetError().GetMessage() → std::string com descrição humana
```

Isso é semelhante ao `std::expected<T, E>` do C++23 ou ao tipo `Result<T, E>` do Rust.

---

## Configuração de credenciais

O SDK tenta autenticar na seguinte ordem:
1. Variáveis de ambiente: `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`
2. Arquivo `~/.aws/credentials`
3. Perfil de instância EC2 / ECS task role (quando rodando na nuvem)

Para configurar localmente:
```bash
aws configure
# ou
export AWS_ACCESS_KEY_ID=...
export AWS_SECRET_ACCESS_KEY=...
export AWS_DEFAULT_REGION=us-east-1
```

---

## Como compilar e executar

```bash
# A partir da raiz do projeto
meson setup dist
cd dist && ninja aws_s3

./bin/aws_s3
```

---

## Dependências

- `aws-sdk-cpp` com componente `s3` (via Conan ou sistema)
- `fmt` para formatação de strings

---

## Extensões possíveis

- `PutObject` — enviar arquivos para um bucket
- `GetObject` — baixar arquivos do S3
- `DeleteObject` — remover objetos
- `CreateBucket` — criar novos buckets
- Upload multipart para arquivos grandes (> 5 GB)
