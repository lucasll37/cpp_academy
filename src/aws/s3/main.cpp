#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/ListBucketsRequest.h>

int main() {
    Aws::SDKOptions options;
    Aws::InitAPI(options);  // SEMPRE inicializa antes de qualquer chamada

    {
        Aws::S3::S3Client client;
        auto outcome = client.ListBuckets();

        if (outcome.IsSuccess()) {
            for (auto& bucket : outcome.GetResult().GetBuckets())
                fmt::print("bucket: {}\n", bucket.GetName().c_str());
        } else {
            fmt::print("erro: {}\n",
                outcome.GetError().GetMessage().c_str());
        }
    }

    Aws::ShutdownAPI(options);  // SEMPRE finaliza no fim
}