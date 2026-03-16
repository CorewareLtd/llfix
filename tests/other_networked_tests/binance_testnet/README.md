The example is for Binance exchange's testnet environment. Therefore you will need to create your own keys for Binance connectivity:

1. Generate key :
```bash
openssl genpkey -algorithm ed25519 -out ed25519_private.pem
openssl pkey -in ed25519_private.pem -pubout -out ed25519_public.pem
```
2. Visit https://testnet.binance.vision/. And authenticate with your Github account
3. Register your public key on https://testnet.binance.vision/ and note your API key.
4. Copy ed25519_private.pem to this directory.
5. Edit config.cfg and set your API key : logon_username=PLACE_YOUR_API_KEY_HERE