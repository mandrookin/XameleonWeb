
#include <openssl/ssl.h>
#include <openssl/err.h>

#if 0
#define CERTIFICATE_PUBLIC     "cert/debug.com.cer"
#define CERTIFICATE_PRIVATE     "cert/debug.com.key"
#else
#define CERTIFICATE_PUBLIC     "cert/cert.pem"
#define CERTIFICATE_PRIVATE     "cert/key.pem"
#endif

void configure_context(SSL_CTX *ctx)
{

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_file(ctx, CERTIFICATE_PUBLIC, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, CERTIFICATE_PRIVATE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_CLIENT_ONCE, nullptr);


    //    SSL_CTX_set_msg_callback(ctx, CTX_callback);
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    const unsigned char * id = (unsigned char*) "Pressure";
    SSL_CTX_set_session_id_context(ctx, id, 8);

    configure_context(ctx);

    return ctx;
}

