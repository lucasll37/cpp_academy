// =============================================================================
//  aws/s3 — o AWS SDK for C++ do zero, com S3
//
//  Quatro demos, todas com o MESMO esqueleto:
//
//      Aws::InitAPI(options);
//      {                          // <- escopo obrigatorio
//          Aws::S3::S3Client client(config);
//          auto outcome = client.<Operacao>(request);
//          if (outcome.IsSuccess()) { ... } else { ... }
//      }
//      Aws::ShutdownAPI(options);
//
//  A Demo 1 (ListBuckets) roda SEMPRE. As demos 2, 3 e 4 escrevem no S3, entao
//  so rodam quando a variavel de ambiente ACADEMY_S3_BUCKET nomeia um bucket:
//  nenhum programa deste repositorio cria ou apaga dado remoto sem que voce
//  peca explicitamente.
//
//  Sem conta AWS? Aponte AWS_ENDPOINT_URL para um LocalStack/MinIO local.
//  Veja o README.md deste diretorio.
//
//  ⚠ Bug historico deste arquivo: faltava o #include <fmt/core.h> abaixo, e o
//    alvo estava desabilitado no src/meson.build, entao o erro nunca aparecia.
//    Mesma classe de bug latente de src/solid/srp e src/concepts — ver
//    ../ROADMAP.md.
// =============================================================================
#include <fmt/core.h>

#include <aws/core/Aws.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/S3ClientConfiguration.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListBucketsRequest.h>
#include <aws/s3/model/PutObjectRequest.h>

#include <cstdlib>
#include <sstream>
#include <string>

namespace {

// -----------------------------------------------------------------------------
// Lê uma variável de ambiente e devolve "" quando ela não existe.
// É assim que credenciais e endpoints entram no programa: NUNCA hardcoded.
// -----------------------------------------------------------------------------
std::string env(const char* nome) {
    const char* v = std::getenv(nome);
    return v ? std::string{v} : std::string{};
}

// -----------------------------------------------------------------------------
// Monta a configuração do cliente EXPLICITAMENTE, em vez de confiar no
// construtor padrão de S3Client:
//
//   region          — obrigatória; o SDK a usa para montar o host e assinar
//   endpointOverride— troca s3.amazonaws.com por um servidor local (LocalStack,
//                     MinIO). Vazio = AWS de verdade.
//   useVirtualAddressing — false força http://host/bucket/key (path-style), que
//                     é o que LocalStack e MinIO entendem. A AWS usa
//                     https://bucket.s3.amazonaws.com/key (virtual-hosted).
// -----------------------------------------------------------------------------
Aws::S3::S3ClientConfiguration make_config() {
    Aws::S3::S3ClientConfiguration config;

    std::string region = env("AWS_REGION");
    if (region.empty()) region = env("AWS_DEFAULT_REGION");
    if (region.empty()) region = "us-east-1";
    config.region = region.c_str();

    const std::string endpoint = env("AWS_ENDPOINT_URL");
    if (!endpoint.empty()) {
        config.endpointOverride     = endpoint.c_str();
        config.useVirtualAddressing = false;
        fmt::print("[config] endpoint local: {} (path-style)\n", endpoint);
    }
    fmt::print("[config] region: {}\n", region);
    return config;
}

// -----------------------------------------------------------------------------
// Imprime um erro do SDK do jeito útil: o TIPO (enum), o código textual e a
// mensagem. É o `GetErrorType()` que permite decidir o que fazer — não a
// mensagem, que é texto livre e muda entre versões.
// -----------------------------------------------------------------------------
template <typename Error>
void print_error(const char* operacao, const Error& e) {
    fmt::print("  ✗ {} falhou\n", operacao);
    fmt::print("    tipo    : {}\n", static_cast<int>(e.GetErrorType()));
    fmt::print("    codigo  : {}\n", e.GetExceptionName().c_str());
    fmt::print("    mensagem: {}\n", e.GetMessage().c_str());

    switch (e.GetErrorType()) {
        case Aws::S3::S3Errors::ACCESS_DENIED:
            fmt::print("    → credencial valida, mas SEM a permissao IAM exigida\n");
            break;
        case Aws::S3::S3Errors::NO_SUCH_BUCKET:
            fmt::print("    → o bucket nao existe nesta conta/regiao\n");
            break;
        case Aws::S3::S3Errors::NO_SUCH_KEY:
            fmt::print("    → o bucket existe, a key nao\n");
            break;
        default:
            fmt::print("    → sem credencial valida, sem rede, ou erro do servico\n");
            break;
    }
}

// -----------------------------------------------------------------------------
// Demo 1 — ListBuckets: a operação mais simples do S3.
// Sem request, sem corpo. Exige a permissão IAM s3:ListAllMyBuckets.
// -----------------------------------------------------------------------------
void demo1_list_buckets(const Aws::S3::S3Client& client) {
    fmt::print("\n=== Demo 1 · ListBuckets ===\n");
    auto outcome = client.ListBuckets();

    if (!outcome.IsSuccess()) {
        print_error("ListBuckets", outcome.GetError());
        return;
    }
    const auto& buckets = outcome.GetResult().GetBuckets();
    fmt::print("  ✓ {} bucket(s)\n", buckets.size());
    for (const auto& b : buckets)
        fmt::print("    bucket: {}\n", b.GetName().c_str());
}

// -----------------------------------------------------------------------------
// Demo 2 — PutObject: enviar bytes. O corpo é um std::iostream em shared_ptr,
// porque o SDK pode reler o stream ao reassinar/reenviar a requisição.
// -----------------------------------------------------------------------------
bool demo2_put_object(const Aws::S3::S3Client& client,
                      const std::string& bucket,
                      const std::string& key,
                      const std::string& conteudo) {
    fmt::print("\n=== Demo 2 · PutObject ===\n");

    Aws::S3::Model::PutObjectRequest req;
    req.SetBucket(bucket.c_str());
    req.SetKey(key.c_str());

    auto body = Aws::MakeShared<Aws::StringStream>("academy-s3");
    *body << conteudo;
    req.SetBody(body);
    req.SetContentType("text/plain");

    auto outcome = client.PutObject(req);
    if (!outcome.IsSuccess()) {
        print_error("PutObject", outcome.GetError());
        return false;
    }
    fmt::print("  ✓ s3://{}/{} gravado ({} bytes)\n", bucket, key, conteudo.size());
    return true;
}

// -----------------------------------------------------------------------------
// Demo 3 — GetObject: ler de volta e comparar com o que foi gravado.
// GetResultWithOwnership() move o resultado: o corpo é um stream consumível.
// -----------------------------------------------------------------------------
bool demo3_get_object(const Aws::S3::S3Client& client,
                      const std::string& bucket,
                      const std::string& key,
                      const std::string& esperado) {
    fmt::print("\n=== Demo 3 · GetObject ===\n");

    Aws::S3::Model::GetObjectRequest req;
    req.SetBucket(bucket.c_str());
    req.SetKey(key.c_str());

    auto outcome = client.GetObject(req);
    if (!outcome.IsSuccess()) {
        print_error("GetObject", outcome.GetError());
        return false;
    }
    auto result = outcome.GetResultWithOwnership();
    std::ostringstream buffer;
    buffer << result.GetBody().rdbuf();
    const std::string lido = buffer.str();

    fmt::print("  ✓ leu {} bytes: \"{}\"\n", lido.size(), lido);
    fmt::print("  {} round-trip {}\n",
               lido == esperado ? "✓" : "✗",
               lido == esperado ? "identico" : "DIVERGENTE");
    return lido == esperado;
}

// -----------------------------------------------------------------------------
// Demo 4 — DeleteObject: limpar o que a Demo 2 criou.
// ⚠ O S3 responde 204 mesmo quando a key não existia: apagar é idempotente.
// -----------------------------------------------------------------------------
void demo4_delete_object(const Aws::S3::S3Client& client,
                         const std::string& bucket,
                         const std::string& key) {
    fmt::print("\n=== Demo 4 · DeleteObject ===\n");

    Aws::S3::Model::DeleteObjectRequest req;
    req.SetBucket(bucket.c_str());
    req.SetKey(key.c_str());

    auto outcome = client.DeleteObject(req);
    if (!outcome.IsSuccess()) {
        print_error("DeleteObject", outcome.GetError());
        return;
    }
    fmt::print("  ✓ s3://{}/{} removido\n", bucket, key);
    fmt::print("  (o S3 responde sucesso mesmo se a key ja nao existia)\n");
}

}  // namespace

int main() {
    // (1) InitAPI SEMPRE primeiro: liga alocador, logging, cliente HTTP e a
    //     biblioteca de criptografia usada para assinar as requisicoes.
    Aws::SDKOptions options;
    Aws::InitAPI(options);

    {   // (2) ESCOPO: tudo que o SDK cria morre AQUI, antes do ShutdownAPI.
        const Aws::S3::S3ClientConfiguration config = make_config();
        Aws::S3::S3Client client(config);

        demo1_list_buckets(client);

        const std::string bucket = env("ACADEMY_S3_BUCKET");
        if (bucket.empty()) {
            fmt::print(
                "\n=== Demos 2-4 puladas ===\n"
                "  Elas ESCREVEM no S3. Defina ACADEMY_S3_BUCKET com um bucket\n"
                "  seu para habilita-las:\n"
                "      export ACADEMY_S3_BUCKET=meu-bucket-de-testes\n");
        } else {
            const std::string key      = "cpp_academy/demo.txt";
            const std::string conteudo = "ola do cpp_academy\n";

            if (demo2_put_object(client, bucket, key, conteudo)) {
                demo3_get_object(client, bucket, key, conteudo);
                demo4_delete_object(client, bucket, key);
            }
        }
    }   // (3) ~S3Client roda aqui, com os recursos globais ainda vivos.

    // (4) ShutdownAPI SEMPRE por ultimo.
    Aws::ShutdownAPI(options);
    return 0;
}
