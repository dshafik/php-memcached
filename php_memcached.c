/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2008 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Andrei Zmievski <andrei@php.net>                            |
  +----------------------------------------------------------------------+
*/

/* $ Id: $ */

/* TODO
 * - compression and prefix key should be per class, not persistent
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>

#ifdef ZTS
#include "TSRM.h"
#endif

#include <php_ini.h>
#include <SAPI.h>
#include <ext/standard/info.h>
#include <zend_extensions.h>
#include <zend_exceptions.h>
#include <libmemcached/memcached.h>

#include "php_memcached.h"

#define MEMC_OPT_COMPRESSION   -1001
#define MEMC_OPT_PREFIX_KEY    -1002

#define MEMC_METHOD_INIT_VARS              \
    zval*             object  = getThis(); \
    php_memc_t*       i_obj   = NULL;      \

#define MEMC_METHOD_FETCH_OBJECT                                               \
    i_obj = (php_memc_t *) zend_object_store_get_object( object TSRMLS_CC );   \

static int le_memc;

static zend_class_entry *memcached_ce = NULL;
static zend_class_entry *memcached_exception_ce = NULL;
static zend_class_entry *spl_ce_RuntimeException = NULL;

typedef struct {
	zend_object zo;

	memcached_st *memc;

	unsigned is_persistent:1;
	const char *plist_key;
	int plist_key_len;

	unsigned compression:1;
} php_memc_t;

#ifdef COMPILE_DL_MEMCACHED
ZEND_GET_MODULE(memcached)
#endif

/****************************************
  Forward declarations
****************************************/
static int php_memc_list_entry(void);
static int php_memc_handle_error(memcached_return status TSRMLS_DC);


/****************************************
  Method implementations
****************************************/

/* {{{ Memcached::__construct() */
static PHP_METHOD(Memcached, __construct)
{
	zval *object = getThis();
	php_memc_t *i_obj;
	memcached_st *memc = NULL;
	char *persistent_id = NULL;
	int persistent_id_len;
	zend_bool skip_ctor = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &persistent_id,
							  &persistent_id_len) == FAILURE) {
		ZVAL_NULL(object);
		return;
	}

	i_obj = (php_memc_t *) zend_object_store_get_object(object TSRMLS_CC);

	if (persistent_id) {
		char *plist_key = NULL;
		int plist_key_len = 0;
		zend_rsrc_list_entry *le;
		php_memc_t *pi_obj = NULL;

		plist_key_len = spprintf(&plist_key, 0, "memcached:id=%s", persistent_id);
		if (zend_hash_find(&EG(persistent_list), plist_key, plist_key_len+1, (void *)&le) == SUCCESS) {
			if (le->type == php_memc_list_entry()) {
				pi_obj = (php_memc_t *) le->ptr;
			}
		}

		if (pi_obj) {
			skip_ctor = 1;
		} else {
			pi_obj = pecalloc(1, sizeof(*pi_obj), 1);

			if (pi_obj == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of memory: cannot allocate handle");
				/* not reached */
			}

			pi_obj->is_persistent = 1;
			if ((pi_obj->plist_key = pemalloc(plist_key_len + 1, 1)) == NULL) {
				php_error_docref(NULL TSRMLS_CC, E_ERROR, "out of memory: cannot allocate handle");
				/* not reached */
			}
			memcpy((char *)pi_obj->plist_key, plist_key, plist_key_len + 1);
			pi_obj->plist_key_len = plist_key_len + 1;

			/*
			 * Copy state bits because we've just constructed a new persistent object.
			 */
			pi_obj->compression = i_obj->compression;
		}

		/*
		 * Copy emalloc'ed bits.
		 */
		pi_obj->zo = i_obj->zo;

		/*
		 * Replace non-persistent object with the persistent one.
		 */
		efree(i_obj);
		i_obj = pi_obj;
		zend_object_store_set_object(object, i_obj TSRMLS_CC);

		if (plist_key) {
			efree(plist_key);
		}
	}

	if (skip_ctor) {
		return;
	}

	memc = memcached_create(NULL);
	if (memc == NULL) {
		php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not allocate libmemcached structure");
		/* not reached */
	}
	i_obj->memc = memc;

	if (persistent_id) {
		zend_rsrc_list_entry le;

		le.type = php_memc_list_entry();
		le.ptr = i_obj;
		if (zend_hash_update(&EG(persistent_list), (char *)i_obj->plist_key,
							 i_obj->plist_key_len, (void *)&le, sizeof(le), NULL) == FAILURE) {
			php_error_docref(NULL TSRMLS_CC, E_ERROR, "could not register persistent entry");
			/* not reached */
		}
	}
}
/* }}} */

/* {{{ Memcached::get() */
PHP_METHOD(Memcached, get)
{
}
/* }}} */

/* {{{ Memcached::getByKey() */
PHP_METHOD(Memcached, getByKey)
{
}
/* }}} */

/* {{{ Memcached::getDelayed() */
PHP_METHOD(Memcached, getDelayed)
{
}
/* }}} */

/* {{{ Memcached::getDelayedByKey() */
PHP_METHOD(Memcached, getDelayedByKey)
{
}
/* }}} */

/* {{{ Memcached::fetch() */
PHP_METHOD(Memcached, fetch)
{
}
/* }}} */

/* {{{ Memcached::fetchAll() */
PHP_METHOD(Memcached, fetchAll)
{
}
/* }}} */

/* {{{ Memcached::set() */
PHP_METHOD(Memcached, set)
{
}
/* }}} */

/* {{{ Memcached::setByKey() */
PHP_METHOD(Memcached, setByKey)
{
}
/* }}} */

/* {{{ Memcached::setMulti() */
PHP_METHOD(Memcached, setMulti)
{
}
/* }}} */

/* {{{ Memcached::getLastTokens() */
PHP_METHOD(Memcached, getLastTokens)
{
}
/* }}} */

/* {{{ Memcached::cas() */
PHP_METHOD(Memcached, cas)
{
}
/* }}} */

/* {{{ Memcached::casByKey() */
PHP_METHOD(Memcached, casByKey)
{
}
/* }}} */

/* {{{ Memcached::add() */
PHP_METHOD(Memcached, add)
{
}
/* }}} */

/* {{{ Memcached::addByKey() */
PHP_METHOD(Memcached, addByKey)
{
}
/* }}} */

/* {{{ Memcached::append() */
PHP_METHOD(Memcached, append)
{
}
/* }}} */

/* {{{ Memcached::appendByKey() */
PHP_METHOD(Memcached, appendByKey)
{
}
/* }}} */

/* {{{ Memcached::prepend() */
PHP_METHOD(Memcached, prepend)
{
}
/* }}} */

/* {{{ Memcached::prependByKey() */
PHP_METHOD(Memcached, prependByKey)
{
}
/* }}} */

/* {{{ Memcached::replace() */
PHP_METHOD(Memcached, replace)
{
}
/* }}} */

/* {{{ Memcached::replaceByKey() */
PHP_METHOD(Memcached, replaceByKey)
{
}
/* }}} */

/* {{{ Memcached::delete() */
PHP_METHOD(Memcached, delete)
{
}
/* }}} */

/* {{{ Memcached::deleteByKey() */
PHP_METHOD(Memcached, deleteByKey)
{
}
/* }}} */

/* {{{ Memcached::increment() */
PHP_METHOD(Memcached, increment)
{
}
/* }}} */

/* {{{ Memcached::decrement() */
PHP_METHOD(Memcached, decrement)
{
}
/* }}} */

/* {{{ Memcached::addServer() */
PHP_METHOD(Memcached, addServer)
{
	char *host;
	int   host_len;
	long  port, weight = 0;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|l", &host, &host_len,
							  &port, &weight) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	if (memcached_server_add_with_weight(i_obj->memc, host, port, weight) != MEMCACHED_SUCCESS) {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ Memcached::getServerList() */
PHP_METHOD(Memcached, getServerList)
{
	memcached_server_st *servers;
	unsigned int i, servers_count;
	zval *array;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "") == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	array_init(return_value);
	servers = memcached_server_list(i_obj->memc);
	servers_count = memcached_server_count(i_obj->memc);
	if (servers == NULL) {
		return;
	}

	for (i = 0; i < servers_count; i++) {
		MAKE_STD_ZVAL(array);
		array_init(array);
		add_assoc_string(array, "host", servers[i].hostname, 1);
		add_assoc_long(array, "port", servers[i].port);
		add_assoc_long(array, "weight", servers[i].weight);
		add_next_index_zval(return_value, array);
	}
}
/* }}} */

/* {{{ Memcached::getServerByKey() */
PHP_METHOD(Memcached, getServerByKey)
{
	char *server_key;
	int   server_key_len;
	memcached_server_st *server;
	memcached_return error;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &server_key, &server_key_len) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	server = memcached_server_by_key(i_obj->memc, server_key, server_key_len, &error);
	if (server == NULL) {
		php_memc_handle_error(error TSRMLS_CC);
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string(return_value, "host", server->hostname, 1);
	add_assoc_long(return_value, "port", server->port);
	add_assoc_long(return_value, "weight", server->weight);
	memcached_server_free(server);
}
/* }}} */

/* {{{ Memcached::setMultiByKey() */
PHP_METHOD(Memcached, setMultiByKey)
{
}
/* }}} */

/* {{{ Memcached::getOption() */
static PHP_METHOD(Memcached, getOption)
{
	long option;
	uint64_t result;
	memcached_behavior flag;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &option) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	switch (option) {
		case MEMC_OPT_COMPRESSION:
			RETURN_BOOL(i_obj->compression);

		case MEMC_OPT_PREFIX_KEY:
		{
			memcached_return retval;
			char *result;

			result = memcached_callback_get(i_obj->memc, MEMCACHED_CALLBACK_PREFIX_KEY, &retval);
			if (retval == MEMCACHED_SUCCESS) {
				RETURN_STRING(result, 1);
			} else {
				RETURN_EMPTY_STRING();
			}
		}

		case MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE:
		case MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE:
			if (memcached_server_count(i_obj->memc) == 0) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "no servers defined");
				return;
			}

		default:
			/*
			 * Assume that it's a libmemcached behavior option.
			 */
			flag = (memcached_behavior) option;
			result = memcached_behavior_get(i_obj->memc, flag);
			RETURN_LONG((long)result);
	}
}
/* }}} */

/* {{{ Memcached::setOption() */
static PHP_METHOD(Memcached, setOption)
{
	long option;
	memcached_behavior flag;
	zval *value;
	MEMC_METHOD_INIT_VARS;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz/", &option, &value) == FAILURE) {
		return;
	}

	MEMC_METHOD_FETCH_OBJECT;

	switch (option) {
		case MEMC_OPT_COMPRESSION:
			convert_to_boolean(value);
			i_obj->compression = Z_BVAL_P(value);
			break;

		case MEMC_OPT_PREFIX_KEY:
			convert_to_string(value);
			if (memcached_callback_set(i_obj->memc, MEMCACHED_CALLBACK_PREFIX_KEY,
									   Z_STRVAL_P(value)) == MEMCACHED_BAD_KEY_PROVIDED) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "bad key provided");
				RETURN_FALSE;
			}
			break;

		default:
			/*
			 * Assume that it's a libmemcached behavior option.
			 */
			flag = (memcached_behavior) option;
			convert_to_long(value);
			if (memcached_behavior_set(i_obj->memc, flag, (uint64_t)Z_LVAL_P(value)) == MEMCACHED_FAILURE) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "error setting memcached option");
				RETURN_FALSE;
			}
			break;
	}

	RETURN_TRUE;
}
/* }}} */


/****************************************
  Internal support code
****************************************/

/* {{{ constructor/destructor */
static void php_memc_destroy(php_memc_t *i_obj TSRMLS_DC)
{
	memcached_free(i_obj->memc);

	pefree(i_obj, i_obj->is_persistent);
}

static void php_memc_free_storage(php_memc_t *i_obj TSRMLS_DC)
{
	zend_object_std_dtor(&i_obj->zo TSRMLS_CC);

	if (!i_obj->is_persistent) {
		php_memc_destroy(i_obj TSRMLS_CC);
	}
}

zend_object_value php_memc_new(zend_class_entry *ce TSRMLS_DC)
{
    zend_object_value retval;
    php_memc_t *i_obj;
    zval *tmp;

    i_obj = ecalloc(1, sizeof(*i_obj));
	zend_object_std_init( &i_obj->zo, ce TSRMLS_CC );
    zend_hash_copy(i_obj->zo.properties, &ce->default_properties, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));

	i_obj->compression = 1;

    retval.handle = zend_objects_store_put(i_obj, (zend_objects_store_dtor_t)zend_objects_destroy_object, (zend_objects_free_object_storage_t)php_memc_free_storage, NULL TSRMLS_CC);
    retval.handlers = zend_get_std_object_handlers();

    return retval;
}

ZEND_RSRC_DTOR_FUNC(php_memc_dtor)
{
    if (rsrc->ptr) {
        php_memc_t *i_obj = (php_memc_t *)rsrc->ptr;
		php_memc_destroy(i_obj TSRMLS_CC);
        rsrc->ptr = NULL;
    }
}
/* }}} */

/* {{{ internal API functions */
static int php_memc_handle_error(memcached_return status TSRMLS_DC)
{
	int result = 0;

	switch (status) {
		case MEMCACHED_SUCCESS:
		case MEMCACHED_DATA_EXISTS:
		case MEMCACHED_STORED:
		case MEMCACHED_NOTFOUND:
		case MEMCACHED_BUFFERED:
			result = 0;
			break;

		case MEMCACHED_FAILURE:
		case MEMCACHED_HOST_LOOKUP_FAILURE:
		case MEMCACHED_CONNECTION_FAILURE:
		case MEMCACHED_CONNECTION_BIND_FAILURE:
		case MEMCACHED_READ_FAILURE:
		case MEMCACHED_UNKNOWN_READ_FAILURE:
		case MEMCACHED_PROTOCOL_ERROR:
		case MEMCACHED_CLIENT_ERROR:
		case MEMCACHED_SERVER_ERROR:
		case MEMCACHED_WRITE_FAILURE:
		case MEMCACHED_CONNECTION_SOCKET_CREATE_FAILURE:
		{
			char *message;
			int   message_len;
			zval *ex;
			zend_class_entry *def_ex  = php_memc_get_exception_base(1 TSRMLS_CC),
							 *memc_ex = php_memc_get_exception();

			MAKE_STD_ZVAL(ex);
			object_init_ex(ex, memc_ex);

			message_len = spprintf(&message, 0, "%s", memcached_strerror(NULL, status));
			zend_update_property_string(def_ex, ex, "message", sizeof("message")-1, message TSRMLS_CC);
			zend_throw_exception_object(ex TSRMLS_CC);
			efree(message);
			result = -1;
			break;
		}

		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", memcached_strerror(NULL, status));
			result = -1;
			break;
	}

	return result;
}

static int php_memc_list_entry(void)
{
	return le_memc;
}

PHP_MEMCACHED_API
zend_class_entry *php_memc_get_ce(void)
{
	return memcached_ce;
}

PHP_MEMCACHED_API
zend_class_entry *php_memc_get_exception(void)
{
	return memcached_exception_ce;
}

PHP_MEMCACHED_API
zend_class_entry *php_memc_get_exception_base(int root TSRMLS_DC)
{
#if HAVE_SPL
	if (!root) {
		if (!spl_ce_RuntimeException) {
			zend_class_entry **pce;

			if (zend_hash_find(CG(class_table), "runtimeexception",
							   sizeof("RuntimeException"), (void **) &pce) == SUCCESS) {
				spl_ce_RuntimeException = *pce;
				return *pce;
			}
		} else {
			return spl_ce_RuntimeException;
		}
	}
#endif
#if (PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 2)
	return zend_exception_get_default();
#else
	return zend_exception_get_default(TSRMLS_C);
#endif
}
/* }}} */

/* {{{ memcached_class_methods */
#define MEMC_ME(name, args) PHP_ME(Memcached, name, args, ZEND_ACC_PUBLIC)
static zend_function_entry memcached_class_methods[] = {
    MEMC_ME(__construct, NULL)

	MEMC_ME(get,                NULL)
	MEMC_ME(getByKey,           NULL)
	MEMC_ME(getDelayed,         NULL)
	MEMC_ME(getDelayedByKey,    NULL)
	MEMC_ME(fetch,              NULL)
	MEMC_ME(fetchAll,           NULL)

	MEMC_ME(set,                NULL)
	MEMC_ME(setByKey,           NULL)
	MEMC_ME(setMulti,           NULL)
	MEMC_ME(setMultiByKey,      NULL)

	MEMC_ME(getLastTokens,      NULL)

	MEMC_ME(cas,                NULL)
	MEMC_ME(casByKey,           NULL)
	MEMC_ME(add,                NULL)
	MEMC_ME(addByKey,           NULL)
	MEMC_ME(append,             NULL)
	MEMC_ME(appendByKey,        NULL)
	MEMC_ME(prepend,            NULL)
	MEMC_ME(prependByKey,       NULL)
	MEMC_ME(replace,            NULL)
	MEMC_ME(replaceByKey,       NULL)
	MEMC_ME(delete,             NULL)
	MEMC_ME(deleteByKey,        NULL)

	MEMC_ME(increment,          NULL)
	MEMC_ME(decrement,          NULL)

	MEMC_ME(addServer,          NULL)
	MEMC_ME(getServerList,      NULL)
	MEMC_ME(getServerByKey,     NULL)

    MEMC_ME(getOption,   NULL)
    MEMC_ME(setOption,   NULL)
	{ NULL, NULL, NULL }
};
#undef MEMC_ME
/* }}} */

/* {{{ memcached_module_entry
 */

#if ZEND_MODULE_API_NO >= 20050922
static const zend_module_dep memcached_deps[] = {
#ifdef HAVE_SPL
    ZEND_MOD_REQUIRED("spl")
#endif
    {NULL, NULL, NULL}
};
#endif

zend_module_entry memcached_module_entry = {
#if ZEND_MODULE_API_NO >= 20050922
    STANDARD_MODULE_HEADER_EX, NULL,
    (zend_module_dep*)memcached_deps,
#else
    STANDARD_MODULE_HEADER,
#endif
	"memcached",
	NULL,
	PHP_MINIT(memcached),
	PHP_MSHUTDOWN(memcached),
	NULL,
	NULL,
	PHP_MINFO(memcached),
	PHP_MEMCACHED_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/* {{{ php_memc_register_constants */
static void php_memc_register_constants(INIT_FUNC_ARGS)
{
	#define REGISTER_MEMC_CLASS_CONST_LONG(name, value) zend_declare_class_constant_long(php_memc_get_ce() , ZEND_STRS( #name ) - 1, value TSRMLS_CC)

	/*
	 * Class options
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(OPT_COMPRESSION, MEMC_OPT_COMPRESSION);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_PREFIX_KEY,  MEMC_OPT_PREFIX_KEY);

	/*
	 * libmemcached behavior options
	 */

	REGISTER_MEMC_CLASS_CONST_LONG(OPT_HASH, MEMCACHED_BEHAVIOR_HASH);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_DEFAULT, MEMCACHED_HASH_DEFAULT);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_MD5, MEMCACHED_HASH_MD5);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_CRC, MEMCACHED_HASH_CRC);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1_64, MEMCACHED_HASH_FNV1_64);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1A_64, MEMCACHED_HASH_FNV1A_64);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1_32, MEMCACHED_HASH_FNV1_32);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_FNV1A_32, MEMCACHED_HASH_FNV1A_32);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_HSIEH, MEMCACHED_HASH_HSIEH);
	REGISTER_MEMC_CLASS_CONST_LONG(HASH_MURMUR, MEMCACHED_HASH_MURMUR);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_DISTRIBUTION, MEMCACHED_BEHAVIOR_DISTRIBUTION);
	REGISTER_MEMC_CLASS_CONST_LONG(DISTRIBUTION_MODULA, MEMCACHED_DISTRIBUTION_MODULA);
	REGISTER_MEMC_CLASS_CONST_LONG(DISTRIBUTION_CONSISTENT, MEMCACHED_DISTRIBUTION_CONSISTENT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_BUFFER_WRITES, MEMCACHED_BEHAVIOR_BUFFER_REQUESTS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_BINARY_PROTOCOL, MEMCACHED_BEHAVIOR_BINARY_PROTOCOL);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_NO_BLOCK, MEMCACHED_BEHAVIOR_NO_BLOCK);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_TCP_NODELAY, MEMCACHED_BEHAVIOR_TCP_NODELAY);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SOCKET_SEND_SIZE, MEMCACHED_BEHAVIOR_SOCKET_SEND_SIZE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SOCKET_RECV_SIZE, MEMCACHED_BEHAVIOR_SOCKET_RECV_SIZE);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_CONNECT_TIMEOUT, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RETRY_TIMEOUT, MEMCACHED_BEHAVIOR_RETRY_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SEND_TIMEOUT, MEMCACHED_BEHAVIOR_SND_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_RECV_TIMEOUT, MEMCACHED_BEHAVIOR_RETRY_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_POLL_TIMEOUT, MEMCACHED_BEHAVIOR_POLL_TIMEOUT);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_CACHE_LOOKUPS, MEMCACHED_BEHAVIOR_CACHE_LOOKUPS);
	REGISTER_MEMC_CLASS_CONST_LONG(OPT_SERVER_FAILURE_LIMIT, MEMCACHED_BEHAVIOR_SERVER_FAILURE_LIMIT);

	#undef REGISTER_MEMC_CLASS_CONST_LONG
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(memcached)
{
	zend_class_entry ce;

    le_memc = zend_register_list_destructors_ex(NULL, php_memc_dtor, "Memcached persistent connection", module_number);

	INIT_CLASS_ENTRY(ce, "Memcached", memcached_class_methods);
	memcached_ce = zend_register_internal_class(&ce TSRMLS_CC);
	memcached_ce->create_object = php_memc_new;

    INIT_CLASS_ENTRY(ce, "MemcachedException", NULL);
	memcached_exception_ce = zend_register_internal_class_ex(&ce, php_memc_get_exception_base(0 TSRMLS_CC), NULL TSRMLS_CC);
	/* TODO
	 * possibly declare custom exception property here
	 */

	php_memc_register_constants(INIT_FUNC_ARGS_PASSTHRU);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(memcached)
{

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(memcached)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "memcached support", "enabled");
	php_info_print_table_row(2, "Version", PHP_MEMCACHED_VERSION);
	php_info_print_table_row(2, "Revision", "$Revision: 1.107 $");
	php_info_print_table_end();
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: noet sw=4 ts=4 fdm=marker:
 */