/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/***************************************************************************
 * Copyright (C) 2013-2014 Ping Identity Corporation
 * All rights reserved.
 *
 * The contents of this file are the property of Ping Identity Corporation.
 * For further information please contact:
 *
 *      Ping Identity Corporation
 *      1099 18th St Suite 2950
 *      Denver, CO 80202
 *      303.468.2900
 *      http://www.pingidentity.com
 *
 * DISCLAIMER OF WARRANTIES:
 *
 * THE SOFTWARE PROVIDED HEREUNDER IS PROVIDED ON AN "AS IS" BASIS, WITHOUT
 * ANY WARRANTIES OR REPRESENTATIONS EXPRESS, IMPLIED OR STATUTORY; INCLUDING,
 * WITHOUT LIMITATION, WARRANTIES OF QUALITY, PERFORMANCE, NONINFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  NOR ARE THERE ANY
 * WARRANTIES CREATED BY A COURSE OR DEALING, COURSE OF PERFORMANCE OR TRADE
 * USAGE.  FURTHERMORE, THERE ARE NO WARRANTIES THAT THE SOFTWARE WILL MEET
 * YOUR NEEDS OR BE FREE FROM ERRORS, OR THAT THE OPERATION OF THE SOFTWARE
 * WILL BE UNINTERRUPTED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @Author: Hans Zandbelt - hzandbelt@pingidentity.com
 */

#ifndef MOD_AUTH_OPENIDC_H_
#define MOD_AUTH_OPENIDC_H_

#include <openssl/evp.h>
#include <apr_uri.h>
#include <apr_uuid.h>
#include <httpd.h>
#include <http_core.h>
#include <http_config.h>
#include <mod_auth.h>

#include "apr_memcache.h"
#include "apr_shm.h"
#include "apr_global_mutex.h"

#include "jose/apr_jose.h"

#include "cache/cache.h"

#ifdef APLOG_USE_MODULE
APLOG_USE_MODULE(auth_openidc);
#endif

#ifndef OIDC_DEBUG
#define OIDC_DEBUG APLOG_DEBUG
#endif

/* key for storing the claims in the session context */
#define OIDC_CLAIMS_SESSION_KEY "claims"
/* key for storing the id_token in the session context */
#define OIDC_IDTOKEN_SESSION_KEY "id_token"

/* parameter name of the callback URL in the discovery response */
#define OIDC_DISC_CB_PARAM "oidc_callback"
/* parameter name of the OP provider selection in the discovery response */
#define OIDC_DISC_OP_PARAM "iss"
/* parameter name of the original URL in the discovery response */
#define OIDC_DISC_RT_PARAM "target_link_uri"
/* parameter name of login hint in the discovery response */
#define OIDC_DISC_LH_PARAM "login_hint"

/* value that indicates to use server-side cache based session tracking */
#define OIDC_SESSION_TYPE_22_SERVER_CACHE 0
/* value that indicates to use client cookie based session tracking */
#define OIDC_SESSION_TYPE_22_CLIENT_COOKIE 1

/* name of the cookie that binds the state in the authorization request/response to the browser */
#define OIDCStateCookieName  "mod_auth_openidc_state"

/* the (global) key for the mod_auth_openidc related state that is stored in the request userdata context */
#define OIDC_USERDATA_KEY "mod_auth_openidc_state"

/* input filter hook name */
#define OIDC_UTIL_HTTP_SENDSTRING "OIDC_UTIL_HTTP_SENDSTRING"

/* the name of the keyword that follows the Require primitive to indicate claims-based authorization */
#define OIDC_REQUIRE_NAME "claim"

typedef struct oidc_provider_t {
	char *issuer;
	char *authorization_endpoint_url;
	char *token_endpoint_url;
	char *token_endpoint_auth;
	char *userinfo_endpoint_url;
	char *registration_endpoint_url;
	char *jwks_uri;
	char *client_id;
	char *client_secret;

	// the next ones function as global default settings too
	int ssl_validate_server;
	char *client_name;
	char *client_contact;
	char *registration_token;
	char *scope;
	char *response_type;
	char *response_mode;
	int jwks_refresh_interval;
	int idtoken_iat_slack;
	char *auth_request_params;

	char *client_jwks_uri;
	char *id_token_signed_response_alg;
	char *id_token_encrypted_response_alg;
	char *id_token_encrypted_response_enc;
	char *userinfo_signed_response_alg;
	char *userinfo_encrypted_response_alg;
	char *userinfo_encrypted_response_enc;
} oidc_provider_t ;

typedef struct oidc_oauth_t {
	int ssl_validate_server;
	char *client_id;
	char *client_secret;
	char *validate_endpoint_url;
	char *validate_endpoint_auth;
	char *remote_user_claim;
} oidc_oauth_t;

typedef struct oidc_cfg {
	/* indicates whether this is a derived config, merged from a base one */
	unsigned int merged;

	/* the redirect URI as configured with the OpenID Connect OP's that we talk to */
	char *redirect_uri;
	/* (optional) external OP discovery page */
	char *discover_url;
	/* (optional) default URL for 3rd-party initiated SSO */
	char *default_url;

	/* public keys in JWK format, used by parters for encrypting JWTs sent to us */
	apr_hash_t *public_keys;
	/* private keys in JWK format used for decrypting encrypted JWTs sent to us */
	apr_hash_t *private_keys;

	/* a pointer to the (single) provider that we connect to */
	/* NB: if metadata_dir is set, these settings will function as defaults for the metadata read from there) */
	oidc_provider_t provider;
	/* a pointer to the oauth server settings */
	oidc_oauth_t oauth;

	/* directory that holds the provider & client metadata files */
	char *metadata_dir;
	/* type of session management/storage */
	int session_type;

	/* pointer to cache functions */
	oidc_cache_t *cache;
	void *cache_cfg;
	/* cache_type = file: directory that holds the cache files (if not set, we'll try and use an OS defined one like "/tmp" */
	char *cache_file_dir;
	/* cache_type = file: clean interval */
	int cache_file_clean_interval;
	/* cache_type= memcache: list of memcache host/port servers to use */
	char *cache_memcache_servers;
	/* cache_type = shm: size of the shared memory segment (cq. max number of cached entries) */
	int cache_shm_size_max;

	/* tell the module to strip any mod_auth_openidc related headers that already have been set by the user-agent, normally required for secure operation */
	int scrub_request_headers;

	int http_timeout_long;
	int http_timeout_short;
	int state_timeout;
	int session_inactivity_timeout;

	char *cookie_domain;
	char *claim_delimiter;
	char *claim_prefix;
	char *remote_user_claim;

	char *outgoing_proxy;

	char *crypto_passphrase;

	EVP_CIPHER_CTX *encrypt_ctx;
	EVP_CIPHER_CTX *decrypt_ctx;
} oidc_cfg;

typedef struct oidc_dir_cfg {
	char *cookie_path;
	char *cookie;
	char *authn_header;
} oidc_dir_cfg;

int oidc_check_user_id(request_rec *r);
#if MODULE_MAGIC_NUMBER_MAJOR >= 20100714
authz_status oidc_authz_checker(request_rec *r, const char *require_args, const void *parsed_require_args);
#else
int oidc_auth_checker(request_rec *r);
#endif
void oidc_request_state_set(request_rec *r, const char *key, const char *value);
const char*oidc_request_state_get(request_rec *r, const char *key);

// oidc_oauth
int oidc_oauth_check_userid(request_rec *r, oidc_cfg *c);

// oidc_proto.c

typedef struct oidc_proto_state {
	const char *nonce;
	const char *original_url;
	const char *original_method;
	const char *issuer;
	const char *response_type;
	const char *response_mode;
	apr_time_t timestamp;
} oidc_proto_state;

int oidc_proto_authorization_request(request_rec *r, struct oidc_provider_t *provider, const char *login_hint, const char *redirect_uri, const char *state, oidc_proto_state *proto_state);
apr_byte_t oidc_proto_is_post_authorization_response(request_rec *r, oidc_cfg *cfg);
apr_byte_t oidc_proto_is_redirect_authorization_response(request_rec *r, oidc_cfg *cfg);
apr_byte_t oidc_proto_check_token_type(request_rec *r, oidc_provider_t *provider, const char *token_type);
apr_byte_t oidc_proto_resolve_code(request_rec *r, oidc_cfg *cfg, oidc_provider_t *provider, const char *code, char **s_id_token, char **s_access_token, char **s_token_type);
apr_byte_t oidc_proto_resolve_userinfo(request_rec *r, oidc_cfg *cfg, oidc_provider_t *provider, const char *access_token, const char **response, json_t **claims);
apr_byte_t oidc_proto_account_based_discovery(request_rec *r, oidc_cfg *cfg, const char *acct, char **issuer);
apr_byte_t oidc_proto_parse_idtoken(request_rec *r, oidc_cfg *cfg, oidc_provider_t *provider, const char *id_token, const char *nonce, char **user, apr_jwt_t **jwt, apr_byte_t is_code_flow);
apr_byte_t oidc_proto_validate_access_token(request_rec *r, oidc_provider_t *provider, apr_jwt_t *jwt, const char *response_type, const char *access_token, const char *token_type);
apr_byte_t oidc_proto_validate_code(request_rec *r, oidc_provider_t *provider, apr_jwt_t *jwt, const char *response_type, const char *code);
int oidc_proto_javascript_implicit(request_rec *r, oidc_cfg *c);
apr_array_header_t *oidc_proto_supported_flows(apr_pool_t *pool);
apr_byte_t oidc_proto_flow_is_supported(apr_pool_t *pool, const char *flow);
apr_byte_t oidc_proto_validate_authorization_response(request_rec *r, const char *response_type, const char *requested_response_mode, char **code, char **id_token, char **access_token, char **token_type, const char *used_response_mode);
apr_byte_t oidc_proto_validate_code_response(request_rec *r, const char *response_type, char **id_token, char **access_token, char **token_type);
apr_byte_t oidc_proto_idtoken_verify_signature(request_rec *r, oidc_cfg *cfg, oidc_provider_t *provider, apr_jwt_t *jwt, apr_byte_t *refresh);
apr_byte_t oidc_proto_validate_iat(request_rec *r, oidc_provider_t *provider, apr_jwt_t *jwt);
apr_byte_t oidc_proto_validate_exp(request_rec *r, apr_jwt_t *jwt);

// oidc_authz.c
int oidc_authz_worker(request_rec *r, const json_t *const claims, const require_line *const reqs, int nelts);
#if MODULE_MAGIC_NUMBER_MAJOR >= 20100714
authz_status oidc_authz_worker24(request_rec *r, const json_t * const claims, const char *require_line);
#endif

// oidc_config.c
void *oidc_create_server_config(apr_pool_t *pool, server_rec *svr);
void *oidc_merge_server_config(apr_pool_t *pool, void *BASE, void *ADD);
void *oidc_create_dir_config(apr_pool_t *pool, char *path);
void *oidc_merge_dir_config(apr_pool_t *pool, void *BASE, void *ADD);
void oidc_register_hooks(apr_pool_t *pool);

// oidc_util.c
int oidc_strnenvcmp(const char *a, const char *b, int len);
int oidc_base64url_encode(request_rec *r, char **dst, const char *src, int src_len, int remove_padding);
int oidc_base64url_decode(request_rec *r, char **dst, const char *src, int add_padding);
int oidc_encrypt_base64url_encode_string(request_rec *r, char **dst, const char *src);
int oidc_base64url_decode_decrypt_string(request_rec *r, char **dst, const char *src);
char *oidc_get_current_url(const request_rec *r, const oidc_cfg *c);
char *oidc_url_encode(const request_rec *r, const char *str, const char *charsToEncode);
char *oidc_normalize_header_name(const request_rec *r, const char *str);

void oidc_util_set_cookie(request_rec *r, const char *cookieName, const char *cookieValue);
char *oidc_util_get_cookie(request_rec *r, char *cookieName);
apr_byte_t oidc_util_http_get(request_rec *r, const char *url, const apr_table_t *params, const char *basic_auth, const char *bearer_token, int ssl_validate_server, const char **response, int timeout, const char *outgoing_proxy);
apr_byte_t oidc_util_http_post_form(request_rec *r, const char *url, const apr_table_t *params, const char *basic_auth, const char *bearer_token, int ssl_validate_server, const char **response, int timeout, const char *outgoing_proxy);
apr_byte_t oidc_util_http_post_json(request_rec *r, const char *url, const json_t *data, const char *basic_auth, const char *bearer_token, int ssl_validate_server, const char **response, int timeout, const char *outgoing_proxy);
apr_byte_t oidc_util_request_matches_url(request_rec *r, const char *url);
apr_byte_t oidc_util_request_has_parameter(request_rec *r, const char* param);
apr_byte_t oidc_util_get_request_parameter(request_rec *r, char *name, char **value);
apr_byte_t oidc_util_decode_json_and_check_error(request_rec *r, const char *str, json_t **json);
int oidc_util_http_sendstring(request_rec *r, const char *html, int success_rvalue);
char *oidc_util_escape_string(const request_rec *r, const char *str);
char *oidc_util_unescape_string(const request_rec *r, const char *str);
apr_byte_t oidc_util_read_post(request_rec *r, apr_table_t *table);
apr_byte_t oidc_util_generate_random_base64url_encoded_value(request_rec *r, int randomLen, char **randomB64);
apr_byte_t oidc_util_file_read(request_rec *r, const char *path, char **result);
apr_byte_t oidc_util_issuer_match(const char *a, const char *b);
int oidc_util_html_send_error(request_rec *r, const char *error, const char *description, int status_code);
apr_byte_t oidc_util_json_array_has_value(request_rec *r, json_t *haystack, const char *needle);
void oidc_util_set_app_headers(request_rec *r, const json_t *j_attrs, const char *claim_prefix, const char *claim_delimiter);
apr_hash_t *oidc_util_spaced_string_to_hashtable(apr_pool_t *pool, const char *str);
apr_byte_t oidc_util_spaced_string_equals(apr_pool_t *pool, const char *a, const char *b);
apr_byte_t oidc_util_spaced_string_contains(apr_pool_t *pool, const char *response_type, const char *match);
apr_byte_t oidc_json_object_get_string(apr_pool_t *pool, json_t *json, const char *name, char **value, const char *default_value);
apr_byte_t oidc_json_object_get_int(apr_pool_t *pool, json_t *json, const char *name, int *value, const int default_value);

// oidc_crypto.c
unsigned char *oidc_crypto_aes_encrypt(request_rec *r, oidc_cfg *cfg, unsigned char *plaintext, int *len);
unsigned char *oidc_crypto_aes_decrypt(request_rec *r, oidc_cfg *cfg, unsigned char *ciphertext, int *len);

// oidc_metadata.c
apr_byte_t oidc_metadata_list(request_rec *r, oidc_cfg *cfg, apr_array_header_t **arr);
apr_byte_t oidc_metadata_get(request_rec *r, oidc_cfg *cfg, const char *selected, oidc_provider_t **provider);
apr_byte_t oidc_metadata_jwks_get(request_rec *r, oidc_cfg *cfg, oidc_provider_t *provider, json_t **j_jwks, apr_byte_t *refresh);

// oidc_session.c
#if MODULE_MAGIC_NUMBER_MAJOR >= 20081201
// this stuff should make it easy to migrate to the post 2.3 mod_session infrastructure
#include "mod_session.h"
#else
typedef struct {
    apr_pool_t *pool;             /* pool to be used for this session */
    apr_uuid_t *uuid;             /* anonymous uuid of this particular session */
    const char *remote_user;      /* user who owns this particular session */
    apr_table_t *entries;         /* key value pairs */
    const char *encoded;          /* the encoded version of the key value pairs */
    apr_time_t expiry;            /* if > 0, the time of expiry of this session */
    long maxage;                  /* if > 0, the maxage of the session, from
                                   * which expiry is calculated */
    int dirty;                    /* dirty flag */
    int cached;                   /* true if this session was loaded from a
                                   * cache of some kind */
    int written;                  /* true if this session has already been
                                   * written */
} session_rec;
#endif

apr_status_t oidc_session_init();
apr_status_t oidc_session_load(request_rec *r, session_rec **z);
apr_status_t oidc_session_get(request_rec *r, session_rec *z, const char *key, const char **value);
apr_status_t oidc_session_set(request_rec *r, session_rec *z, const char *key, const char *value);
apr_status_t oidc_session_save(request_rec *r, session_rec *z);
apr_status_t oidc_session_kill(request_rec *r, session_rec *z);

#endif /* MOD_AUTH_OPENIDC_H_ */
