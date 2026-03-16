#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/pem.h>

// https://github.com/binance/binance-spot-api-docs/blob/master/testnet/fix-api.md#how-to-sign-logon-a-request
inline std::string get_binance_logon_signature(
    const std::string& private_key_pem_file,
    const std::string& message_type,
    const std::string& sender_compid,
    const std::string& target_compid,
    uint32_t seqno,
    const std::string& sending_time)
{
    // std::cout << "get_binance_logon_signature" << " message_type: " << message_type << " sender_compid: " << sender_compid << " target_compid: " << target_compid << " seqno: " << seqno << " sending_time: " << sending_time << std::endl;

    constexpr char SOH = '\x01';
    std::string payload;
    
    payload.reserve(message_type.size()
        + sender_compid.size()
        + target_compid.size()
        + sending_time.size()
        + 32);

    payload += message_type;          payload += SOH; // 35 value, e.g. "A"
    payload += sender_compid;         payload += SOH; // 49 value
    payload += target_compid;         payload += SOH; // 56 value
    payload += std::to_string(seqno); payload += SOH; // 34 value
    payload += sending_time;                       // 52 value

    // Load and cache Ed25519 private key
    static EVP_PKEY* pkey = nullptr;

    if (!pkey)
    {
        FILE* fp = std::fopen(private_key_pem_file.c_str(), "rb");
        if (!fp)
            return "";

        pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
        std::fclose(fp);

        if (!pkey)
            return "";
    }

    // Sign with Ed25519 (EVP_DigestSign with null digest)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        return "";

    if (EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, pkey) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    size_t sig_len = 0;
    if (EVP_DigestSign(ctx,
        nullptr, &sig_len,
        reinterpret_cast<const unsigned char*>(payload.data()),
        payload.size()) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    std::vector<unsigned char> sig(sig_len);
    if (EVP_DigestSign(ctx,
        sig.data(), &sig_len,
        reinterpret_cast<const unsigned char*>(payload.data()),
        payload.size()) != 1)
    {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);
    sig.resize(sig_len);

    // Base64 encode signature (no newlines)
    std::string b64;
    b64.resize(4 * ((sig.size() + 2) / 3));
    int out_n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(&b64[0]),
        sig.data(),
        static_cast<int>(sig.size()));

    if (out_n < 0)
        return "";
    
    b64.resize(static_cast<size_t>(out_n));

    return b64;
}